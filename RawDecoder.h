#ifndef RAW_DECODER_H
#define RAW_DECODER_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#include "Hit.h"

class RawDecoder {
public:

  /// One event's header fields. Small and trivially copyable: the DAQ reads these straight out of
  /// Hits() by reference, once per hit, so nothing here should cost a copy or an allocation.
  struct DecodedHit {
    uint8_t  channel             = 0;
    uint64_t timestamp           = 0;   // raw ticks (caller scales by tick2ns)
    uint16_t fine_timestamp      = 0;   // raw units
    uint16_t energy              = 0;
    uint16_t energy_short        = 0;   // PSD only, 0 for PHA
    uint16_t flags_low_priority  = 0;
    uint16_t flags_high_priority = 0;
    bool     board_fail          = false;
    uint32_t aggCounter          = 0;
    bool     flush               = false;
    /// Index into Waves() when the waveform was decoded, else -1. The DAQ never decodes
    /// waveforms (they stay in the blob written to the .raw file), so it is always -1 there.
    int32_t  wave                = -1;
  };

  /// A decoded waveform. Only built with LoadBlob(..., decodeWaveform = true), which only the
  /// offline test tool (Aux/testRawDecoder) asks for; kept apart from DecodedHit so the DAQ's
  /// per-hit struct carries none of it.
  struct DecodedWave {
    size_t   traceLenght        = 0;
    uint8_t  downSampling       = 0;
    uint16_t trigger_threashold = 0;
    uint8_t  analog_probes_type[2]  = {0, 0};
    uint8_t  digital_probes_type[4] = {0, 0, 0, 0};
    std::vector<int32_t> analog_probes_0;
    std::vector<int32_t> analog_probes_1;
    std::vector<uint8_t> digital_probes_0;
    std::vector<uint8_t> digital_probes_1;
    std::vector<uint8_t> digital_probes_2;
    std::vector<uint8_t> digital_probes_3;
  };

  struct StatUpdate {
    uint8_t  channel;
    uint64_t realTime;        // from Word 0 timestamp field (raw ticks; the caller scales to ns)
    uint64_t deadTime;        // from extra word (raw ticks; the caller scales to ns)
    uint32_t triggerCount;
    uint32_t savedEventCount;
  };

  /// Decode one blob. isPSD selects the PSD field layout (energy_short); the caller caches it once
  /// per board rather than comparing firmware-name strings per hit.
  void LoadBlob(const uint8_t* data, size_t dataSize, bool isPSD, bool decodeWaveform = false){
    hits_.clear();
    waves_.clear();
    statUpdates_.clear();
    sawStopRun_ = false;
    isPSD_ = isPSD;
    decodeWaveform_ = decodeWaveform;
    if( data == nullptr || dataSize < 8 ) return;
    ParseBlob(data, dataSize);
  }

  /// The loaded blob's events, in blob order. Valid until the next LoadBlob(); read by reference.
  const std::vector<DecodedHit>&  Hits()  const { return hits_; }
  /// Decoded waveforms, indexed by DecodedHit::wave. Empty unless decodeWaveform was set.
  const std::vector<DecodedWave>& Waves() const { return waves_; }

  const std::vector<StatUpdate>& GetStatUpdates() const { return statUpdates_; }

  /// The loaded blob holds the board's Stop Run event: the last thing it sends for a run.
  bool SawStopRun() const { return sawStopRun_; }

private:
  std::vector<DecodedHit>  hits_;
  std::vector<DecodedWave> waves_;
  std::vector<StatUpdate>  statUpdates_;
  bool sawStopRun_    = false;
  bool isPSD_         = false;
  bool decodeWaveform_ = false;

  static uint64_t ReadBE64(const uint8_t* p){
    uint64_t val;
    memcpy(&val, p, 8);
    return __builtin_bswap64(val);
  }

  /// Random access to the blob's big-endian words, swapped at the point of use. The decoder used
  /// to swap the whole blob into a std::vector<uint64_t> up front -- an allocation plus a full
  /// extra pass over every byte -- for a walk that only ever reads one word at a time anyway.
  static uint64_t W(const uint8_t* base, size_t i){ return ReadBE64(base + i * 8); }

  void ParseBlob(const uint8_t* data, size_t dataSize){

    // dataSize must be a multiple of 8
    size_t nTotalWords = dataSize / 8;
    if( nTotalWords == 0 ) return;

    size_t pos = 0;
    while( pos < nTotalWords ){
      uint64_t w0 = W(data, pos);
      uint8_t format = (w0 >> 60) & 0xF;

      if( format == 0x2 ){
        // Individual Trigger Mode aggregate
        pos += ParseAggregate(data + pos * 8, nTotalWords - pos);
      } else if( format == 0x3 ){
        // Special event: subtype [59:56] 0x0 = Start Run, 0x2 = Stop Run (format_RAW.md)
        uint32_t nWords = w0 & 0xFFFFFFFF;
        if( nWords == 0 || pos + nWords > nTotalWords ) break;
        if( ((w0 >> 56) & 0xF) == 0x2 ) sawStopRun_ = true;
        pos += nWords;
      } else {
        // Unknown format, try to skip 1 word
        pos++;
      }
    }
  }

  size_t ParseAggregate(const uint8_t* aggData, size_t remaining){
    if( remaining < 1 ) return 1;

    uint64_t header = W(aggData, 0);
    uint32_t nAggWords = header & 0xFFFFFFFF;
    if( nAggWords == 0 || nAggWords > remaining ) return remaining; // safety

    bool     flush      = (header >> 59) & 0x1;
    bool     boardFail  = (header >> 56) & 0x1;
    uint32_t aggCounter = (header >> 32) & 0x00FFFFFF;

    size_t pos = 1; // skip aggregate header
    while( pos < nAggWords ){

      if( pos >= nAggWords ) break;
      uint64_t w0 = W(aggData, pos);

      /// No "[63:60] == 0x3 = embedded special event" test here. Inside an aggregate Word 0 is
      /// 0 | channel[62:56] | ..., so channels 48-63 (0x30-0x3F) carry that nibble. The old test
      /// read them as Start/Stop events, took timestamp bits as a word count and broke out of the
      /// aggregate -- and since EnStatEvents puts one time/counter event per channel at the front
      /// of every aggregate, ch48's dropped almost every hit (tests/rawDecoderHighCh). Special
      /// events are top-level only (format_RAW.md; tests/rawBlobWalk saw start and stop run only
      /// between aggregates); ParseBlob() handles them there.

      // Check bit 63: single-word event (EnDataReduction)?
      if( (w0 >> 63) & 0x1 ){
        // Single-word event
        DecodedHit & hit = hits_.emplace_back();   // built in place, no copy
        hit.channel          = (w0 >> 56) & 0x7F;
        hit.flags_high_priority = (w0 >> 48) & 0xFF;
        hit.timestamp        = (w0 >> 16) & 0xFFFFFFFF; // 32-bit reduced timestamp
        hit.energy           = w0 & 0xFFFF;
        hit.energy_short     = 0;
        hit.fine_timestamp   = 0;
        hit.flags_low_priority = 0;
        hit.board_fail       = boardFail;
        hit.aggCounter       = aggCounter;
        hit.flush            = flush;

        pos++;
        continue;
      }

      // Multi-word event: Word 0
      uint8_t  channel   = (w0 >> 56) & 0x7F;
      uint8_t  se        = (w0 >> 54) & 0x3;
      uint64_t timestamp = w0 & 0x0000FFFFFFFFFFFF; // bits [47:0]

      // Check if this is a time/counter event (bit 55 set = s.e. bit 1)
      bool isTimeEvent = (se >> 1) & 0x1; // bit 55

      if( isTimeEvent ){
        // Time/counter statistics event
        // Word 1 is meaningless, skip it
        pos++; // skip Word 0
        if( pos >= nAggWords ) break;
        pos++; // skip Word 1 (meaningless)

        StatUpdate su;
        su.channel = channel;
        su.realTime = timestamp;
        su.deadTime = 0;
        su.triggerCount = 0;
        su.savedEventCount = 0;

        // Parse extra words until we find one with bit 63 set or run out
        // Extra Word 0: dead_time
        if( pos < nAggWords ){
          uint64_t ew0 = W(aggData, pos);
          su.deadTime = ew0 & 0x0000FFFFFFFFFFFF;
          pos++;
        }
        // Extra Word 1: trigger counter + saved event counter
        if( pos < nAggWords ){
          uint64_t ew1 = W(aggData, pos);
          su.triggerCount    = (ew1 >> 24) & 0x00FFFFFF;
          su.savedEventCount = ew1 & 0x00FFFFFF;
          pos++;
        }

        statUpdates_.push_back(su);
        continue;
      }

      // Normal physics event
      pos++; // consumed Word 0
      if( pos >= nAggWords ) break;

      uint64_t w1 = W(aggData, pos);
      pos++;

      DecodedHit & hit = hits_.emplace_back();     // built in place, no copy
      hit.channel          = channel;
      hit.timestamp        = timestamp;
      hit.board_fail       = boardFail;
      hit.aggCounter       = aggCounter;
      hit.flush            = flush;

      // Word 1 fields
      bool lastWord = (w1 >> 63) & 0x1;
      bool hasWf    = (w1 >> 62) & 0x1;   // named hasWf, not W, so it does not shadow W()
      hit.flags_low_priority  = (w1 >> 50) & 0x0FFF;
      hit.flags_high_priority = (w1 >> 42) & 0xFF;
      hit.energy_short        = isPSD_ ? ((w1 >> 26) & 0xFFFF) : 0;
      hit.fine_timestamp      = (w1 >> 16) & 0x03FF;
      hit.energy              = w1 & 0xFFFF;

      // If not last word, scan forward for extra words.
      // The last header word (bit 63=1) is the waveform extra word when hasWf=1.
      uint64_t lastHeaderWord = w1;
      if( !lastWord ){
        while( pos < nAggWords ){
          lastHeaderWord = W(aggData, pos);
          pos++;
          if( (lastHeaderWord >> 63) & 0x1 ) break; // last extra word
        }
      }

      // If waveform present (hasWf=1), handle waveform data
      // The waveform extra word is the last header word (already consumed above)
      if( hasWf ){
        if( decodeWaveform_ ){
          // Parse waveform extra word (probe info). `hit` stays valid: waves_ is a separate vector.
          hit.wave = (int32_t) waves_.size();
          DecodedWave & wv = waves_.emplace_back();
          uint64_t wfExtra = lastHeaderWord;
          wv.downSampling        = (wfExtra >> 48) & 0x3;
          wv.trigger_threashold  = (wfExtra >> 32) & 0xFFFF;
          wv.digital_probes_type[3] = (wfExtra >> 28) & 0xF;
          wv.digital_probes_type[2] = (wfExtra >> 24) & 0xF;
          wv.digital_probes_type[1] = (wfExtra >> 20) & 0xF;
          wv.digital_probes_type[0] = (wfExtra >> 16) & 0xF;
          uint8_t ap1info = (wfExtra >> 10) & 0x3F;
          uint8_t ap0info = (wfExtra >>  4) & 0x3F;
          wv.analog_probes_type[0] = ap0info & 0x07;
          wv.analog_probes_type[1] = ap1info & 0x07;

          // Waveform header word
          if( pos < nAggWords ){
            uint64_t wfHeader = W(aggData, pos);
            pos++;
            uint32_t wfNWords = wfHeader & 0xFFFFFFFF;
            /// Clamp BEFORE sizing anything. wfNWords is a raw 32-bit field off the wire, so a
            /// corrupt or misaligned waveform header asks the six resize() calls below for up to
            /// ~34 GB and throws bad_alloc. The sample loop is already bounded by nAggWords; this
            /// makes the allocation agree with it, the same way the skip branch clamps pos.
            if( wfNWords > nAggWords - pos ) wfNWords = (uint32_t)(nAggWords - pos);
            size_t nSamples = (size_t) wfNWords * 2;
            wv.traceLenght = nSamples;
            wv.analog_probes_0.resize(nSamples);
            wv.analog_probes_1.resize(nSamples);
            wv.digital_probes_0.resize(nSamples);
            wv.digital_probes_1.resize(nSamples);
            wv.digital_probes_2.resize(nSamples);
            wv.digital_probes_3.resize(nSamples);

            size_t sampleIdx = 0;
            for( uint32_t wi = 0; wi < wfNWords && pos < nAggWords; wi++, pos++ ){
              uint64_t sw = W(aggData, pos);
              if( sampleIdx < nSamples ){
                uint32_t s0 = sw & 0xFFFFFFFF;
                wv.analog_probes_0[sampleIdx] = (int32_t)((s0 & 0x3FFF) | ((s0 & 0x2000) ? 0xFFFFC000 : 0));
                wv.analog_probes_1[sampleIdx] = (int32_t)(((s0 >> 16) & 0x3FFF) | (((s0 >> 16) & 0x2000) ? 0xFFFFC000 : 0));
                wv.digital_probes_0[sampleIdx] = (s0 >> 14) & 0x1;
                wv.digital_probes_1[sampleIdx] = (s0 >> 15) & 0x1;
                wv.digital_probes_2[sampleIdx] = (s0 >> 30) & 0x1;
                wv.digital_probes_3[sampleIdx] = (s0 >> 31) & 0x1;
                sampleIdx++;
              }
              if( sampleIdx < nSamples ){
                uint32_t s1 = (sw >> 32) & 0xFFFFFFFF;
                wv.analog_probes_0[sampleIdx] = (int32_t)((s1 & 0x3FFF) | ((s1 & 0x2000) ? 0xFFFFC000 : 0));
                wv.analog_probes_1[sampleIdx] = (int32_t)(((s1 >> 16) & 0x3FFF) | (((s1 >> 16) & 0x2000) ? 0xFFFFC000 : 0));
                wv.digital_probes_0[sampleIdx] = (s1 >> 14) & 0x1;
                wv.digital_probes_1[sampleIdx] = (s1 >> 15) & 0x1;
                wv.digital_probes_2[sampleIdx] = (s1 >> 30) & 0x1;
                wv.digital_probes_3[sampleIdx] = (s1 >> 31) & 0x1;
                sampleIdx++;
              }
            }
          }
        } else {
          // Skip waveform: just advance past the waveform words
          if( pos < nAggWords ){
            uint64_t wfHeader = W(aggData, pos);
            pos++;
            uint32_t wfNWords = wfHeader & 0xFFFFFFFF;
            pos += wfNWords;
            if( pos > nAggWords ) pos = nAggWords;
          }
        }
      }
    }

    return nAggWords;
  }

};

#endif
