#ifndef ONLINE_EVENT_BUILDER_H
#define ONLINE_EVENT_BUILDER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

#include "LeanHit.h"
#include "RingBuffer.h"

/// Online event builder: merges the per-board LeanHit rings into time-correlated events.
///
/// Deliberately free of Qt, CAEN_FELib and ROOT, so it can be driven from the GUI, from a console
/// tool replaying a file, or from a future out-of-process analyzer.
///
/// The merge is a linear min-scan over board fronts rather than a priority queue. FSUDAQ's
/// MultiBuilder needs a heap because it merges 20 boards x 64 channels = 1280 sources; here a
/// board's output is already timestamp-sorted across its channels (format_RAW.md:91), so there is
/// one source per board — at most 20, well below any heap crossover, and without the ~120 lines of
/// heap bookkeeping that go wrong when a source runs dry and refills (which online is the normal
/// case, not the exception). PickEarliestBoard() is the one place to swap in a heap if
/// monotonicityViolations ever shows the sorting assumption has broken.

struct BuiltHit {
  uint64_t timestamp;       ///< ns
  uint16_t sn;              ///< board serial number
  uint16_t energy;
  uint16_t energy_short;
  uint16_t fine_timestamp;  ///< sub-tick time, scaled by tick2ns; see LeanHit.h
  uint8_t  digi;            ///< index into the view list given to the constructor
  uint8_t  channel;
  uint8_t  flagsHigh;       ///< see LeanHitFlag in LeanHit.h
};

/// One hit source. The ring must outlive the builder.
struct DigiHitView {
  uint16_t sn;
  const RingBuffer<LeanHit, LeanHitRingSize> * ring;
};

class OnlineEventBuilder {
public:
  explicit OnlineEventBuilder(const std::vector<DigiHitView> & viewList);

  /// Hits within timeWindow of the event seed join the event. The bound is EXCLUSIVE:
  /// [seed, seed + timeWindow). That matches Aux/EventBuilder.cpp:306, which every existing
  /// SOLARIS analysis was built against — FSUDAQ's MultiBuilder is inclusive and disagrees.
  /// timeWindow == 0 means no event building: exactly one hit per event.
  void SetTimeWindow(uint64_t ns)   { timeWindow = ns; }

  /// Extra margin beyond the window before an event is considered final. Larger means more
  /// latency but more tolerance to jitter between boards. Internally clamped to at least
  /// timeWindow, so the call order of the two setters does not matter.
  void SetGuardTime(uint64_t ns)    { guardTime = ns; }

  /// A hit more than this far BELOW the previous hit of the same board is treated as the board's
  /// clock having restarted rather than as data corruption.
  void SetTimeJump(uint64_t ns)     { timeJump = ns; }

  /// A board whose ring write index has not moved for this long is dropped from the build
  /// horizon so it cannot stall the build. Wall-clock, not timestamps: a stalled board's
  /// timestamps are by definition not advancing.
  void SetStallTimeout(unsigned ms) { stallTimeoutMs = ms; }

  uint64_t GetTimeWindow()   const { return timeWindow; }
  uint64_t GetGuardTime()    const { return guardTime < timeWindow ? timeWindow : guardTime; }
  uint64_t GetTimeJump()     const { return timeJump; }
  unsigned GetStallTimeout() const { return stallTimeoutMs; }

  /// Start from "now": every cursor jumps to its ring's current write index, so whatever is
  /// already buffered is skipped. Same idea as SingleSpectra::ClearInternalDataCount(). Call this
  /// on ACQ start — StartACQ() restarts the board timestamp counter but does NOT clear the host
  /// ring, so without it the builder would see pre-reset hits with huge timestamps ahead of new
  /// hits near zero. Does not touch the counters; use ResetStat() for those.
  void Reset();
  void ResetStat();

  /// Drain what is available and emit closed events through onEvent.
  /// isFinal = true skips the build horizon and flushes everything left; use it at end of run.
  /// maxEvents < 0 means unbounded. Returns the number of events emitted by this call.
  long BuildEvents(bool isFinal = false, long maxEvents = -1);

  /// Called once per completed event, on the caller's thread. Must not block — the caller is
  /// typically draining a ring the DAQ thread is still filling. The vector is reused between
  /// events, so copy anything you need to keep.
  std::function<void(const std::vector<BuiltHit> &)> onEvent;

  void PrintStat() const;

  //^---- counters. Non-zero values of the last three mean something is wrong; see PrintStat().
  long totalEventsBuilt;
  long totalHitsConsumed;
  long totalHitsDropped;        ///< lapped: the producer overwrote hits before we read them
  long totalHitsLate;           ///< arrived after their event was already settled
  long monotonicityViolations;  ///< a board's hits went backwards in time (small step)
  long timeJumpEvents;          ///< a board's hits went backwards by more than timeJump
  long stallEvents;             ///< a board went quiet and was dropped from the build horizon

private:
  struct BoardState {
    unsigned long cursor;        ///< next absolute ring index to read
    /// The ring's write index as seen once at the top of BuildEvents. Both the pulling and the
    /// build horizon work off this same value, which is what makes them consistent: see
    /// SnapshotIndices().
    unsigned long snapIndex;
    LeanHit  front;              ///< the one hit pulled and not yet consumed
    bool     hasFront;
    uint64_t lastPulledTs;
    bool     hasLastPulledTs;
    unsigned long lastIndexSeen;                       ///< for the liveness check
    std::chrono::steady_clock::time_point lastAdvance;
    bool     active;             ///< false while stalled: excluded from the build horizon only,
                                 ///< its buffered hits are still merged normally
  };

  std::vector<DigiHitView> views;
  std::vector<BoardState>  board;
  std::vector<BuiltHit>    event;   ///< reused across events so there is no malloc per event

  uint64_t timeWindow;
  uint64_t guardTime;
  uint64_t timeJump;
  unsigned stallTimeoutMs;

  /// Everything strictly below this has already been built into an event. Only a stalled board
  /// rejoining can deliver a hit under it; such hits are counted in totalHitsLate and dropped.
  uint64_t settledBelow;

  void     SnapshotIndices();
  bool     PullNext(size_t b);
  int      PickEarliestBoard() const;
  void     UpdateLiveness();
  uint64_t GetBuildHorizon() const;
};

#endif
