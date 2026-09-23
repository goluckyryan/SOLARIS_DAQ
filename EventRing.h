#ifndef EVENT_RING_H
#define EVENT_RING_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "OnlineEventBuilder.h"   // BuiltHit
#include "RingBuffer.h"

/// Buffer of built events, sitting between the event builder and whatever analyses them.
///
/// Why it exists: the builder must never be held up by a slow consumer. If it were, it would stop
/// draining the per-board LeanHit rings and RAW HITS would be lost. With a ring in between, a slow
/// consumer loses only events, which is recoverable.
///
/// Why a flat hit ring plus an index ring, rather than fixed-size event slots: events are variable
/// length, and any fixed MaxHitsPerEvent both wastes space on the common multiplicity-1..3 case and
/// truncates exactly during the noise bursts and common-mode events you opened the analyzer to
/// diagnose. 20 boards x 64 channels is already 1280 hits before pile-up, so there is no defensible
/// constant. Here memory is proportional to actual hits and nothing is ever truncated.
///
/// Sizing: the hit ring holds as many hits as ONE board's LeanHit ring (LeanHit.h, 262144 = 262 ms
/// at 1 MHz), so a stalled consumer starts losing events at the same point the raw hits would have
/// been lost anyway. Anything shallower would make the ring pointless.
///
/// ~8 MiB, so ALWAYS heap-allocate this. In particular it must not be a member of
/// OnlineEventBuilder, which Aux/testOnlineBuilder.cpp constructs on the stack.
///
/// Threading: single producer (the builder's thread) and, today, a single consumer. The read state
/// lives in Reader rather than in the ring, so more consumers -- or an out-of-process host mapping
/// this into shared memory -- can attach later without changing either side.
///
/// Run boundaries are handled by Clear() with both threads stopped, not by an epoch counter. The
/// analyzer's enable toggle is the only run boundary, and it stops everything before clearing.

struct EventIndex {
  uint64_t firstHit;   ///< absolute index into the hit ring
  uint32_t nHits;
};

/// 262144 x 24 B = 6.0 MiB
static constexpr size_t EventHitRingSize = 1 << 18;
/// 131072 x 16 B = 2.0 MiB
static constexpr size_t EventRingSize    = 1 << 17;

class EventRing {
public:

  //^=========================================== producer side (builder thread only)

  /// Hits first, then the index entry. The release store inside the index push is what publishes
  /// the event: a consumer that can see the EventIndex is guaranteed to see the hits it points at.
  void Publish(const std::vector<BuiltHit> & ev){
    if( ev.empty() ) return;
    const unsigned long first = hitRing.index();
    for( size_t i = 0; i < ev.size(); i++ ) hitRing.push(ev[i]);
    EventIndex e;
    e.firstHit = (uint64_t) first;
    e.nHits    = (uint32_t) ev.size();
    idxRing.push(e);
  }

  /// Only safe with the producer stopped. See the run-boundary note above.
  void Clear(){
    hitRing.clear();
    idxRing.clear();
  }

  unsigned long EventsWritten() const { return idxRing.index(); }
  unsigned long HitsWritten()   const { return hitRing.index(); }

  //^=========================================== consumer side

  /// One per consumer: holds the cursor and that consumer's own loss accounting.
  struct Reader {
    unsigned long cursor        = 0;
    long          eventsDropped = 0;   ///< lapped by the producer before we reached them
    long          eventsTorn    = 0;   ///< read caught mid-write, or a bad nHits; discarded

    /// Start from wherever the ring is now, and forget past losses.
    void Rewind(const EventRing & ring){
      cursor        = ring.idxRing.index();
      eventsDropped = 0;
      eventsTorn    = 0;
    }

    /// Copy the next event into `out`. Returns false when nothing new is available.
    ///
    /// Both rings are checked, because they have independent write cursors: the index entry can be
    /// intact while the hits it points at have already been overwritten.
    bool Next(const EventRing & ring, std::vector<BuiltHit> & out){

      while( true ){

        const unsigned long w = ring.idxRing.index();
        if( cursor >= w ) return false;                    // nothing new

        //---- index ring lapped: skip to the oldest entry still guaranteed intact
        if( w - cursor >= ring.idxRing.size() ){
          const unsigned long safe = w - ring.idxRing.size() + 1;
          eventsDropped += (long)(safe - cursor);
          cursor = safe;
          continue;
        }

        EventIndex e = ring.idxRing.at(cursor);

        /// Re-read after the copy: EventIndex is 16 bytes and cannot be copied atomically, so a
        /// producer that reached this slot mid-read leaves us with a torn value. Do not advance
        /// the cursor -- the lap check at the top of the next iteration will move it.
        if( ring.idxRing.index() - cursor >= ring.idxRing.size() ){
          eventsTorn++;
          continue;
        }

        //---- a torn or corrupt entry must never be used to size a copy loop
        if( e.nHits == 0 || e.nHits > ring.hitRing.size() / 2 ){
          eventsTorn++;
          cursor++;
          continue;
        }

        out.clear();
        out.reserve(e.nHits);
        for( uint32_t k = 0; k < e.nHits; k++ ){
          out.push_back(ring.hitRing.at(e.firstHit + k));  // absolute index, so wrap is automatic
        }

        //---- and check the hits themselves were not overwritten while we copied them
        if( ring.hitRing.index() - e.firstHit >= ring.hitRing.size() ){
          eventsDropped++;
          cursor++;
          continue;
        }

        cursor++;
        return true;
      }
    }
  };

private:
  RingBuffer<BuiltHit,   EventHitRingSize> hitRing;
  RingBuffer<EventIndex, EventRingSize>    idxRing;
};

#endif
