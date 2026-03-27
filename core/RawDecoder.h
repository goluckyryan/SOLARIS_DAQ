#ifndef RAW_DECODER_H
#define RAW_DECODER_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#include "Hit.h"

class RawDecoder {
public:

  struct DecodedHit {
    uint8_t  channel;
    uint64_t timestamp;          // raw ticks (caller scales by tick2ns)
    uint16_t fine_timestamp;     // raw units
    uint16_t energy;
    uint16_t energy_short;       // PSD only, 0 for PHA
    uint16_t flags_low_priority;
    uint16_t flags_high_priority;
    bool     board_fail;
    uint32_t aggCounter;
    bool     flush;

    // Waveform fields (only populated when decodeWaveform = true)
    bool     hasWaveform;
    size_t   traceLenght;
    uint8_t  downSampling;
    uint16_t trigger_threashold;
    uint8_t  analog_probes_type[2];
    uint8_t  digital_probes_type[4];
    std::vector<int32_t> analog_probes_0;
    std::vector<int32_t> analog_probes_1;
    std::vector<uint8_t> digital_probes_0;
    std::vector<uint8_t> digital_probes_1;
    std::vector<uint8_t> digital_probes_2;
    std::vector<uint8_t> digital_probes_3;

    void Clear(){
      channel = 0;
      timestamp = 0;
      fine_timestamp = 0;
      energy = 0;
      energy_short = 0;
      flags_low_priority = 0;
      flags_high_priority = 0;
      board_fail = false;
      aggCounter = 0;
      flush = false;
      hasWaveform = false;
      traceLenght = 0;
      downSampling = 0;
      trigger_threashold = 0;
      memset(analog_probes_type, 0, sizeof(analog_probes_type));
      memset(digital_probes_type, 0, sizeof(digital_probes_type));
    }
  };

  struct StatUpdate {
    uint8_t  channel;
    uint64_t realTime;        // from Word 0 timestamp field (raw ticks)
    uint64_t deadTime;        // from extra word (raw ticks)
    uint32_t triggerCount;
    uint32_t savedEventCount;
  };

  RawDecoder(){
    cursor_ = 0;
    decodeWaveform_ = false;
  }

  void LoadBlob(const uint8_t* data, size_t dataSize, const std::string& dppType, bool decodeWaveform = false){
    hits_.clear();
    statUpdates_.clear();
    cursor_ = 0;
    decodeWaveform_ = decodeWaveform;
    if( data == nullptr || dataSize < 8 ) return;
    ParseBlob(data, dataSize, dppType);
  }

  bool HasNext() const { return cursor_ < hits_.size(); }

  bool Next(DecodedHit& out){
    if( cursor_ >= hits_.size() ) return false;
    out = hits_[cursor_];
    cursor_++;
    return true;
  }

  const std::vector<StatUpdate>& GetStatUpdates() const { return statUpdates_; }

  uint32_t GetDecodedCount() const { return (uint32_t) hits_.size(); }

private:
  std::vector<DecodedHit> hits_;
  std::vector<StatUpdate> statUpdates_;
  size_t cursor_;
  bool decodeWaveform_;

  static uint64_t ReadBE64(const uint8_t* p){
    uint64_t val;
    memcpy(&val, p, 8);
    return __builtin_bswap64(val);
  }

  void ParseBlob(const uint8_t* data, size_t dataSize, const std::string& dppType){

    // dataSize must be a multiple of 8
    size_t nTotalWords = dataSize / 8;
    if( nTotalWords == 0 ) return;

    // Convert entire blob to host-endian words
    std::vector<uint64_t> words(nTotalWords);
    for( size_t i = 0; i < nTotalWords; i++ ){
      words[i] = ReadBE64(data + i * 8);
    }

    size_t pos = 0;
    while( pos < nTotalWords ){
      uint64_t w0 = words[pos];
      uint8_t format = (w0 >> 60) & 0xF;

      if( format == 0x2 ){
        // Individual Trigger Mode aggregate
        pos += ParseAggregate(&words[pos], nTotalWords - pos, dppType);
      } else if( format == 0x3 ){
        // Special event (Start Run / Stop Run)
        uint32_t nWords = w0 & 0xFFFFFFFF;
        if( nWords == 0 || pos + nWords > nTotalWords ) break;
        pos += nWords;
      } else {
        // Unknown format, try to skip 1 word
        pos++;
      }
    }
  }

  size_t ParseAggregate(const uint64_t* words, size_t remaining, const std::string& dppType){
    if( remaining < 1 ) return 1;

    uint64_t header = words[0];
    uint32_t nAggWords = header & 0xFFFFFFFF;
    if( nAggWords == 0 || nAggWords > remaining ) return remaining; // safety

    bool     flush      = (header >> 59) & 0x1;
    bool     boardFail  = (header >> 56) & 0x1;
    uint32_t aggCounter = (header >> 32) & 0x00FFFFFF;

    size_t pos = 1; // skip aggregate header
    while( pos < nAggWords ){

      if( pos >= nAggWords ) break;
      uint64_t w0 = words[pos];

      // Check if this is a special event embedded in the aggregate
      uint8_t topNibble = (w0 >> 60) & 0xF;
      if( topNibble == 0x3 ){
        // Special event (Start/Stop Run) embedded
        uint32_t seWords = w0 & 0xFFFFFFFF;
        if( seWords == 0 || pos + seWords > nAggWords ) break;
        pos += seWords;
        continue;
      }

      // Check bit 63: single-word event (EnDataReduction)?
      if( (w0 >> 63) & 0x1 ){
        // Single-word event
        DecodedHit hit;
        hit.Clear();
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

        hits_.push_back(hit);
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
          uint64_t ew0 = words[pos];
          su.deadTime = ew0 & 0x0000FFFFFFFFFFFF;
          pos++;
        }
        // Extra Word 1: trigger counter + saved event counter
        if( pos < nAggWords ){
          uint64_t ew1 = words[pos];
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

      uint64_t w1 = words[pos];
      pos++;

      DecodedHit hit;
      hit.Clear();
      hit.channel          = channel;
      hit.timestamp        = timestamp;
      hit.board_fail       = boardFail;
      hit.aggCounter       = aggCounter;
      hit.flush            = flush;

      // Word 1 fields
      bool lastWord = (w1 >> 63) & 0x1;
      bool W        = (w1 >> 62) & 0x1;
      hit.flags_low_priority  = (w1 >> 50) & 0x0FFF;
      hit.flags_high_priority = (w1 >> 42) & 0xFF;
      hit.energy_short        = (dppType == DPPType::PSD) ? ((w1 >> 26) & 0xFFFF) : 0;
      hit.fine_timestamp      = (w1 >> 16) & 0x03FF;
      hit.energy              = w1 & 0xFFFF;

      // If not last word, scan forward for extra words.
      // The last header word (bit 63=1) is the waveform extra word when W=1.
      uint64_t lastHeaderWord = w1;
      if( !lastWord ){
        while( pos < nAggWords ){
          lastHeaderWord = words[pos];
          pos++;
          if( (lastHeaderWord >> 63) & 0x1 ) break; // last extra word
        }
      }

      // If waveform present (W=1), handle waveform data
      // The waveform extra word is the last header word (already consumed above)
      if( W ){
        if( decodeWaveform_ ){
          // Parse waveform extra word (probe info)
          uint64_t wfExtra = lastHeaderWord;
          hit.downSampling        = (wfExtra >> 48) & 0x3;
          hit.trigger_threashold  = (wfExtra >> 32) & 0xFFFF;
          hit.digital_probes_type[3] = (wfExtra >> 28) & 0xF;
          hit.digital_probes_type[2] = (wfExtra >> 24) & 0xF;
          hit.digital_probes_type[1] = (wfExtra >> 20) & 0xF;
          hit.digital_probes_type[0] = (wfExtra >> 16) & 0xF;
          uint8_t ap1info = (wfExtra >> 10) & 0x3F;
          uint8_t ap0info = (wfExtra >>  4) & 0x3F;
          hit.analog_probes_type[0] = ap0info & 0x07;
          hit.analog_probes_type[1] = ap1info & 0x07;

          // Waveform header word
          if( pos < nAggWords ){
            uint64_t wfHeader = words[pos];
            pos++;
            uint32_t wfNWords = wfHeader & 0xFFFFFFFF;
            size_t nSamples = wfNWords * 2;
            hit.hasWaveform = true;
            hit.traceLenght = nSamples;
            hit.analog_probes_0.resize(nSamples);
            hit.analog_probes_1.resize(nSamples);
            hit.digital_probes_0.resize(nSamples);
            hit.digital_probes_1.resize(nSamples);
            hit.digital_probes_2.resize(nSamples);
            hit.digital_probes_3.resize(nSamples);

            size_t sampleIdx = 0;
            for( uint32_t wi = 0; wi < wfNWords && pos < nAggWords; wi++, pos++ ){
              uint64_t sw = words[pos];
              if( sampleIdx < nSamples ){
                uint32_t s0 = sw & 0xFFFFFFFF;
                hit.analog_probes_0[sampleIdx] = (int32_t)((s0 & 0x3FFF) | ((s0 & 0x2000) ? 0xFFFFC000 : 0));
                hit.analog_probes_1[sampleIdx] = (int32_t)(((s0 >> 16) & 0x3FFF) | (((s0 >> 16) & 0x2000) ? 0xFFFFC000 : 0));
                hit.digital_probes_0[sampleIdx] = (s0 >> 14) & 0x1;
                hit.digital_probes_1[sampleIdx] = (s0 >> 15) & 0x1;
                hit.digital_probes_2[sampleIdx] = (s0 >> 30) & 0x1;
                hit.digital_probes_3[sampleIdx] = (s0 >> 31) & 0x1;
                sampleIdx++;
              }
              if( sampleIdx < nSamples ){
                uint32_t s1 = (sw >> 32) & 0xFFFFFFFF;
                hit.analog_probes_0[sampleIdx] = (int32_t)((s1 & 0x3FFF) | ((s1 & 0x2000) ? 0xFFFFC000 : 0));
                hit.analog_probes_1[sampleIdx] = (int32_t)(((s1 >> 16) & 0x3FFF) | (((s1 >> 16) & 0x2000) ? 0xFFFFC000 : 0));
                hit.digital_probes_0[sampleIdx] = (s1 >> 14) & 0x1;
                hit.digital_probes_1[sampleIdx] = (s1 >> 15) & 0x1;
                hit.digital_probes_2[sampleIdx] = (s1 >> 30) & 0x1;
                hit.digital_probes_3[sampleIdx] = (s1 >> 31) & 0x1;
                sampleIdx++;
              }
            }
          }
        } else {
          // Skip waveform: just advance past the waveform words
          if( pos < nAggWords ){
            uint64_t wfHeader = words[pos];
            pos++;
            uint32_t wfNWords = wfHeader & 0xFFFFFFFF;
            pos += wfNWords;
            if( pos > nAggWords ) pos = nAggWords;
          }
        }
      }

      hits_.push_back(hit);
    }

    return nAggWords;
  }

};

#endif
