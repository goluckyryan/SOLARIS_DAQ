#ifndef BUILT_HIT_H
#define BUILT_HIT_H

#include <cstdint>

/// One hit inside a built event.
///
/// This lives in its own header, away from OnlineEventBuilder.h, because it is part of the ANALYSIS
/// PLUGIN ABI. A loadable analysis includes Analysis.h, which includes only this — so a plugin does
/// not depend on the builder's internals, and changing them does not oblige anyone to rebuild their
/// analyses.
///
/// Keep it a POD of fixed-width types. Nothing here may become an STL type, a pointer into host
/// memory, or anything whose layout depends on compiler flags: this struct crosses a shared-object
/// boundary, and that it does so as plain bytes is what makes the ABI robust.

struct BuiltHit {
  uint64_t timestamp;       ///< ns
  uint16_t sn;              ///< board serial number
  uint16_t energy;
  uint16_t energy_short;    ///< PSD only; 0 under PHA firmware
  uint16_t fine_timestamp;  ///< sub-tick time, scaled by tick2ns; see LeanHit.h
  uint8_t  digi;            ///< the board number the GUI shows, not the builder's view position
  uint8_t  channel;
  uint8_t  flagsHigh;       ///< quality flags; see LeanHitFlag in LeanHit.h
};

static_assert(sizeof(BuiltHit) == 24, "BuiltHit is part of the plugin ABI; size must not drift");

#endif
