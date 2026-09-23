#include "OnlineEventBuilder.h"

#include "EventRing.h"

#include <cstdio>

OnlineEventBuilder::OnlineEventBuilder(const std::vector<DigiHitView> & viewList){

  views = viewList;
  board.resize(views.size());

  eventSink = nullptr;

  timeWindow     = 100;    // ns
  guardTime      = 1000;   // ns
  timeJump       = 100000000ULL; // 100 ms
  stallTimeoutMs = 500;

  event.reserve(64);

  ResetStat();
  Reset();
}

void OnlineEventBuilder::ResetStat(){
  totalEventsBuilt      = 0;
  totalHitsConsumed     = 0;
  totalHitsDropped      = 0;
  totalHitsLate         = 0;
  monotonicityViolations = 0;
  timeJumpEvents        = 0;
  stallEvents           = 0;
}

void OnlineEventBuilder::Reset(){

  const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

  for( size_t b = 0; b < views.size(); b++ ){
    BoardState & s = board[b];
    s.cursor           = views[b].ring ? views[b].ring->index() : 0;
    s.snapIndex        = s.cursor;
    s.hasFront         = false;
    s.lastPulledTs     = 0;
    s.hasLastPulledTs  = false;
    s.lastIndexSeen    = s.cursor;
    s.lastAdvance      = now;
    s.active           = true;
  }

  settledBelow = 0;
  event.clear();
}

//^=============================================================== pulling from one ring

/// Read every ring's write index once, at the top of a build pass, and work off those values for
/// the rest of the pass.
///
/// This is not an optimisation, it is what makes the build horizon sound. Reading index() separately
/// for the pulling and for the build horizon lets a board look empty when its front is pulled and then
/// look advanced when the limit is computed: the limit then covers hits that are sitting unpulled
/// in that ring and therefore invisible to the merge, so events get closed over them and they turn
/// up late. With one snapshot, a board either has un-pulled hits below the snapshot — in which case
/// the merge sees its front — or has none, in which case everything it delivers later is at or
/// above its newest, hence at or above the limit.
void OnlineEventBuilder::SnapshotIndices(){
  for( size_t b = 0; b < views.size(); b++ ){
    board[b].snapIndex = views[b].ring ? views[b].ring->index() : 0;
  }
}

/// Advance board b by one hit, leaving it in s.front. Returns false when the ring is exhausted.
///
/// The lap check is per copy, not once before the loop. SingleSpectra::FillHistograms() checks the
/// gap once and then drains, which lets the producer lap it mid-loop; that costs a few smeared
/// histogram entries there, but here it would mean building an event out of torn hits. This
/// follows the stricter recipe from Scope::UpdateScope() (scope.cpp:752-767) and the contract
/// written at the top of RingBuffer.h.
bool OnlineEventBuilder::PullNext(size_t b){

  BoardState & s = board[b];
  const RingBuffer<LeanHit, LeanHitRingSize> * r = views[b].ring;

  if( r == nullptr ){ s.hasFront = false; return false; }

  while( true ){

    /// Bounded by the snapshot, not by a fresh index(): see SnapshotIndices().
    if( s.cursor >= s.snapIndex ){ s.hasFront = false; return false; } // nothing new this pass

    LeanHit h = r->at(s.cursor);

    /// Re-read after the copy: a LeanHit is 16 bytes and cannot be copied atomically, so if the
    /// producer reached this slot while we were reading it, h is torn and must be thrown away.
    const unsigned long w2 = r->index();
    if( w2 - s.cursor >= r->size() ){
      const unsigned long safe = w2 - r->size() + 1; // oldest slot still guaranteed intact
      totalHitsDropped += (long)(safe - s.cursor);
      s.cursor = safe;
      continue;
    }

    s.cursor++;

    if( s.hasLastPulledTs && h.timestamp < s.lastPulledTs ){
      const uint64_t back = s.lastPulledTs - h.timestamp;
      if( back > timeJump ){
        /// The board's timestamp counter restarted (a new ACQ without a Reset(), typically).
        /// Nothing to repair here: the event in progress closes on its own because the window
        /// test below rejects a hit earlier than the seed, and the next seed re-anchors.
        timeJumpEvents++;
        settledBelow = 0;
      }else{
        /// Small backwards step. The whole merge assumes a board's hits are time-ordered
        /// (format_RAW.md:91). If this fires, that assumption is broken and the events built
        /// from this point are not trustworthy.
        monotonicityViolations++;
      }
    }

    s.lastPulledTs    = h.timestamp;
    s.hasLastPulledTs = true;
    s.front           = h;
    s.hasFront        = true;
    totalHitsConsumed++;
    return true;
  }
}

//^=============================================================== merge + build horizon

int OnlineEventBuilder::PickEarliestBoard() const {

  int      best   = -1;
  uint64_t bestTs = 0;

  for( size_t b = 0; b < board.size(); b++ ){
    if( !board[b].hasFront ) continue;
    if( best < 0 || board[b].front.timestamp < bestTs ){
      best   = (int) b;
      bestTs = board[b].front.timestamp;
    }
  }

  return best;
}

/// A stalled board is one whose ring write index has not moved for stallTimeoutMs. It keeps its
/// buffered hits in the merge; it is only removed from the build horizon, so that a board with the
/// beam off cannot freeze event building for every other board.
void OnlineEventBuilder::UpdateLiveness(){

  const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

  for( size_t b = 0; b < views.size(); b++ ){

    BoardState & s = board[b];
    if( views[b].ring == nullptr ) continue;

    const unsigned long w = s.snapIndex;

    if( w != s.lastIndexSeen ){
      s.lastIndexSeen = w;
      s.lastAdvance   = now;
      s.active        = true;
      continue;
    }

    if( !s.active ) continue;

    const long idleMs = (long) std::chrono::duration_cast<std::chrono::milliseconds>(now - s.lastAdvance).count();
    if( idleMs >= (long) stallTimeoutMs ){
      s.active = false;
      stallEvents++;
    }
  }
}

/// The newest timestamp every live board has already delivered. Because a board emits in
/// non-decreasing time order, it can never produce anything below its own newest — so an event
/// whose window closes below the minimum over all live boards is final.
///
/// Note this reads the NEWEST entry in each ring, not the one at our cursor: the question is what
/// the board has produced, not what we have consumed.
///
/// FSUDAQ takes this minimum over channels rather than boards, which online is fatal — one channel
/// that fires early and then goes quiet pins the limit forever and the builder never advances
/// again. Per board, that cannot happen: the limit moves as long as any channel of the board fires.
uint64_t OnlineEventBuilder::GetBuildHorizon() const {

  uint64_t limit = UINT64_MAX;

  for( size_t b = 0; b < views.size(); b++ ){

    if( !board[b].active ) continue;
    const RingBuffer<LeanHit, LeanHitRingSize> * r = views[b].ring;
    if( r == nullptr ) continue;

    /// The snapshot, deliberately, not a fresh index(): see SnapshotIndices().
    const unsigned long w = board[b].snapIndex;

    /// A live board that has delivered nothing yet has an UNKNOWN horizon, and the next thing it
    /// says will start near zero — so nothing can be declared final while we are still waiting on
    /// it. FSUDAQ skips this board instead (MultiBuilder.cpp:169); offline that is right, because
    /// a file with no data never acquires any, but online it lets the limit run ahead of a board
    /// that is merely slow to start, and every hit it then delivers arrives late.
    /// A board that never speaks is dealt with by the stall timeout in UpdateLiveness(), not here.
    if( w == 0 ) return 0;

    LeanHit h = r->at(w - 1);
    if( r->index() - (w - 1) >= r->size() ) continue; // torn; skip it this round

    if( h.timestamp < limit ) limit = h.timestamp;
  }

  return limit;
}

//^=============================================================== the build loop

long OnlineEventBuilder::BuildEvents(bool isFinal, long maxEvents){

  SnapshotIndices();
  UpdateLiveness();

  for( size_t b = 0; b < views.size(); b++ ){
    if( !board[b].hasFront ) PullNext(b);
  }

  const uint64_t guard = GetGuardTime();
  long built = 0;

  while( maxEvents < 0 || built < maxEvents ){

    int b = PickEarliestBoard();

    /// Hits under settledBelow belong to an event that has already been emitted. In normal running
    /// the build horizon makes this impossible; it happens when a stalled board rejoins with buffered
    /// data. Dropping them is a bounded, counted loss — the alternative is mis-binning them.
    while( b >= 0 && board[b].front.timestamp < settledBelow ){
      totalHitsLate++;
      PullNext(b);
      b = PickEarliestBoard();
    }

    if( b < 0 ) break; // nothing anywhere

    const uint64_t eventStart = board[b].front.timestamp;

    if( !isFinal ){
      const uint64_t horizon = GetBuildHorizon();
      /// UINT64_MAX means no live board constrains us — every board has been quiet longer than the
      /// stall timeout — so whatever is still buffered can be flushed now rather than waiting for
      /// end of run. That is what makes the builder drain itself when the beam goes off.
      if( horizon != UINT64_MAX ){
        /// Written to be safe against unsigned wraparound. FSUDAQ (MultiBuilder.cpp:197) and the
        /// offline builder (Aux/EventBuilder.cpp:306) both compute this difference unguarded, so a
        /// backwards hit wraps to ~1.8e19 and force-closes the event.
        if( horizon <= eventStart ) break;
        if( horizon - eventStart <= guard ) break;
      }
    }

    event.clear();

    while( true ){

      const int k = PickEarliestBoard();
      if( k < 0 ) break;

      const LeanHit & f = board[k].front;

      if( !event.empty() ){                                  // the seed always joins
        if( timeWindow == 0 ) break;                         // no event building: one hit each
        if( f.timestamp < eventStart ) break;                // went backwards; not this event
        if( f.timestamp - eventStart >= timeWindow ) break;  // exclusive upper bound
      }

      BuiltHit bh;
      bh.timestamp      = f.timestamp;
      bh.sn             = views[k].sn;
      bh.energy         = f.energy;
      bh.energy_short   = f.energy_short;
      bh.fine_timestamp = f.fine_timestamp;
      bh.digi           = views[k].digiIndex;   // the caller's board number, not the view position
      bh.channel        = f.channel;
      bh.flagsHigh      = f.flagsHigh;
      event.push_back(bh);

      PullNext(k);
    }

    if( event.empty() ) break; // defensive; PickEarliestBoard said there was a hit

    settledBelow = eventStart + timeWindow;
    totalEventsBuilt++;
    built++;

    if( eventSink ) eventSink->Publish(event);
    if( onEvent )   onEvent(event);
  }

  return built;
}

//^===============================================================

void OnlineEventBuilder::PrintStat() const {

  printf("=========== OnlineEventBuilder\n");
  printf("  window %lu ns, guard %lu ns, timeJump %lu ns, stall timeout %u ms\n",
         (unsigned long) timeWindow, (unsigned long) GetGuardTime(),
         (unsigned long) timeJump, stallTimeoutMs);
  printf("  events built  : %ld\n", totalEventsBuilt);
  printf("  hits consumed : %ld\n", totalHitsConsumed);
  printf("  hits dropped  : %ld%s\n", totalHitsDropped,
         totalHitsDropped ? "   <== ring lapped, the builder could not keep up" : "");
  printf("  hits late     : %ld%s\n", totalHitsLate,
         totalHitsLate ? "   <== hits arrived below an already-settled boundary "
                         "(a stalled board rejoining is the expected cause)" : "");
  printf("  time jumps    : %ld%s\n", timeJumpEvents,
         timeJumpEvents ? "   <== a board clock restarted; call Reset() on ACQ start" : "");
  printf("  board stalls  : %ld\n", stallEvents);
  printf("  monotonicity  : %ld%s\n", monotonicityViolations,
         monotonicityViolations ? "   <== HITS OUT OF ORDER, EVENTS ARE NOT TRUSTWORTHY" : "");
}
