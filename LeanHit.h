#ifndef LEAN_HIT_H
#define LEAN_HIT_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

/// A timestamped hit, stripped to what online event building and online analysis need.
///
/// This exists because HitSummary (ClassDigitizer2Gen.h) carries only {energy, energy_short}:
/// the timestamp is decoded and then thrown away at the ring push, which makes event building
/// impossible. LeanHit is pushed alongside HitSummary, into a separate ring, so SingleSpectra's
/// hot path and memory footprint are untouched.
///
/// Contract notes for anyone touching this struct:
///
///  - It must stay trivially copyable: RingBuffer's constructor does memset(buffer, 0, ...) over
///    an array of these, and consumers copy slots by value.
///  - 16 bytes is NOT atomically copyable on x86-64, so a copy taken out of a live ring can tear.
///    Consumers must use the copy-then-recheck discipline (see scope.cpp:752-767): read index(),
///    copy with at(), re-read index(), discard the copy if the producer advanced by size() or
///    more in between.
///  - Hits within one board's ring are expected to be in non-decreasing timestamp order. This is
///    load-bearing for the event builder and is documented in format_RAW.md:91 ("interleaved
///    events (all channels, sorted by timestamp)"). OnlineEventBuilder verifies it at runtime and
///    counts monotonicityViolations; a non-zero count means the assumption has broken.

struct LeanHit {
  uint64_t timestamp;       ///< ns, already scaled by tick2ns
  uint16_t energy;
  uint16_t energy_short;    ///< PSD only; deterministically 0 under PHA firmware
  /// Sub-tick time in ps, scaled by tick2ns exactly as Digitizer2Gen::ReadData() and
  /// Aux/EventBuilderRaw.cpp:137 do, so it matches Hit::fine_timestamp and the .sol files.
  /// The raw field is a 10-bit fraction of ONE coarse tick (RawDecoder.h:249), so scaling by
  /// tick2ns is what makes the unit the same for a 500 Msps VX2730 and a 125 Msps VX2740.
  /// Max value is 1023 * 8 = 8184, so it cannot overflow the uint16.
  ///
  /// NOTE the scale is 1024ths of a tick, not 1000ths: the true LSB is 8 ns / 1024 = 7.8125 ps
  /// (format_RAW.md:308), so this number read as ps is high by 1024/1000, i.e. 2.4%. See the
  /// comment in ClassDigitizer2Gen.cpp at the fine_timestamp scaling.
  uint16_t fine_timestamp;
  uint8_t  channel;         ///< the ring is per-board, so the channel has to be carried here
  uint8_t  flagsHigh;       ///< high-priority flags; see LeanHitFlag below
};

static_assert(sizeof(LeanHit) == 16, "LeanHit must stay 16 bytes");
static_assert(std::is_trivially_copyable<LeanHit>::value, "LeanHit must be trivially copyable");

/// High-priority flag bits, from format_RAW.md:275-284.
/// Bits 1 and 7 are not defined by the firmware.
///
/// CAVEAT: these are only populated for DataFormat ALL / OneTrace / NoTrace and the Raw path.
/// The Minimum and MiniWithFineTime formats never pass &hit->flags_high_priority to
/// CAEN_FELib_ReadData, so flagsHigh is deterministically 0 there — a pile-up cut would silently
/// keep everything. Check the data format before trusting these.
namespace LeanHitFlag {
  constexpr uint8_t PileUp          = 1 << 0; ///< two events overlapping before peaking time
  constexpr uint8_t EventSaturation = 1 << 2; ///< input dynamics saturated
  constexpr uint8_t PostSaturation  = 1 << 3; ///< event inside the ADCVetoWidth window
  constexpr uint8_t ChargeOverflow  = 1 << 4; ///< integrated charge overflow (PSD)
  constexpr uint8_t SCASelected     = 1 << 5; ///< event in the SCA window
  constexpr uint8_t FineTSValid     = 1 << 6; ///< fine timestamp was properly calculated
}

/// Depth of one board's hit ring. 16 B x 262144 = 4 MiB per board, which holds 262 ms of a 1 MHz
/// board. One ring per board rather than 64 per-channel rings, so the depth pools where the rate
/// actually is instead of being statically partitioned across mostly-idle channels.
static constexpr size_t LeanHitRingSize = 262144;

#endif
