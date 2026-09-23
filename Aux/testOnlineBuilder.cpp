/// Exercise OnlineEventBuilder without a digitizer.
///
/// Builds no GUI and needs no CAEN, no ROOT and no Qt: it pushes LeanHits into real RingBuffers
/// and drains them through the real builder, so the streaming, build-horizon and lap paths are all
/// genuinely exercised rather than bypassed.
///
///   ./testOnlineBuilder --selftest                     the whole verification ladder
///   ./testOnlineBuilder --synth --boards 4 --dump      look at synthetic events
///   ./testOnlineBuilder --replay run_0_51554_000.sol   replay a real .sol file
///   ./testOnlineBuilder --threaded                     push from threads while draining
///
/// Build:  make -C Aux tools

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "SolReader.h"
#include "../Analysis.h"
#include "../EventRing.h"
#include "../LeanHit.h"
#include "../OnlineEventBuilder.h"

typedef RingBuffer<LeanHit, LeanHitRingSize> HitRing;
typedef std::vector<BuiltHit>               Event;

/// A stand-in for one digitizer: the hits it will emit, plus the ring it emits them into.
struct Board {
  uint16_t sn;
  std::vector<LeanHit>     hits;    ///< source data, must be sorted by timestamp
  std::unique_ptr<HitRing> ring;    ///< 4 MiB, so always on the heap
  size_t                   pushed;

  Board() : sn(0), ring(new HitRing()), pushed(0) {}
};

static void RewindBoards(std::vector<Board> & boards){
  for( size_t b = 0; b < boards.size(); b++ ){
    boards[b].pushed = 0;
    boards[b].ring->clear();
  }
}

static size_t TotalHits(const std::vector<Board> & boards){
  size_t n = 0;
  for( size_t b = 0; b < boards.size(); b++ ) n += boards[b].hits.size();
  return n;
}

//^===================================================================== the oracle

/// Dead simple, deliberately: everything in one vector, sorted, greedy leading-edge window.
/// No rings, no build horizon, no streaming. If the streaming builder disagrees with this, the
/// streaming builder is wrong.
static std::vector<Event> ReferenceBuild(const std::vector<Board> & boards, uint64_t window){

  std::vector<BuiltHit> all;
  for( size_t b = 0; b < boards.size(); b++ ){
    for( size_t i = 0; i < boards[b].hits.size(); i++ ){
      const LeanHit & h = boards[b].hits[i];
      BuiltHit bh;
      bh.timestamp = h.timestamp;   bh.sn        = boards[b].sn;
      bh.energy    = h.energy;      bh.energy_short = h.energy_short;
      bh.fine_timestamp = h.fine_timestamp;
      bh.digi      = (uint8_t) b;   bh.channel   = h.channel;
      bh.flagsHigh = h.flagsHigh;
      all.push_back(bh);
    }
  }

  std::stable_sort(all.begin(), all.end(),
                   [](const BuiltHit & a, const BuiltHit & b){ return a.timestamp < b.timestamp; });

  std::vector<Event> out;
  size_t i = 0;
  while( i < all.size() ){
    const uint64_t seed = all[i].timestamp;
    Event ev;
    ev.push_back(all[i]);
    i++;
    if( window > 0 ){
      while( i < all.size() && all[i].timestamp - seed < window ){ ev.push_back(all[i]); i++; }
    }
    out.push_back(ev);
  }
  return out;
}

//^===================================================================== comparison

static bool HitLess(const BuiltHit & a, const BuiltHit & b){
  if( a.timestamp != b.timestamp ) return a.timestamp < b.timestamp;
  if( a.digi      != b.digi      ) return a.digi      < b.digi;
  if( a.channel   != b.channel   ) return a.channel   < b.channel;
  return a.energy < b.energy;
}

/// Within one event the two builders may order simultaneous hits differently, so canonicalise
/// before comparing. Across events the order is fully determined by the seed timestamp.
static void Canonicalise(std::vector<Event> & evs){
  for( size_t i = 0; i < evs.size(); i++ ) std::sort(evs[i].begin(), evs[i].end(), HitLess);
}

static bool SameEvents(std::vector<Event> a, std::vector<Event> b, const char * what){

  Canonicalise(a);
  Canonicalise(b);

  if( a.size() != b.size() ){
    printf("  FAIL [%s]: %zu events vs %zu\n", what, a.size(), b.size());
    return false;
  }

  for( size_t i = 0; i < a.size(); i++ ){
    if( a[i].size() != b[i].size() ){
      printf("  FAIL [%s]: event %zu multiplicity %zu vs %zu\n", what, i, a[i].size(), b[i].size());
      return false;
    }
    for( size_t k = 0; k < a[i].size(); k++ ){
      const BuiltHit & x = a[i][k];
      const BuiltHit & y = b[i][k];
      if( x.timestamp != y.timestamp || x.digi != y.digi || x.channel != y.channel ||
          x.energy != y.energy || x.energy_short != y.energy_short || x.sn != y.sn ||
          x.fine_timestamp != y.fine_timestamp || x.flagsHigh != y.flagsHigh ){
        printf("  FAIL [%s]: event %zu hit %zu differs "
               "(ts %" PRIu64 "/%" PRIu64 ", digi %u/%u, ch %u/%u, e %u/%u)\n",
               what, i, k, x.timestamp, y.timestamp, x.digi, y.digi,
               x.channel, y.channel, x.energy, y.energy);
        return false;
      }
    }
  }
  return true;
}

//^===================================================================== streaming runs

/// Push `chunk` hits per board per round, draining between rounds, so the build horizon has to
/// do the work. The stall timeout is pushed out of reach: these runs must not depend on
/// wall-clock speed.
static std::vector<Event> StreamBuild(std::vector<Board> & boards, uint64_t window, size_t chunk,
                                      OnlineEventBuilder * statOut = nullptr){

  RewindBoards(boards);

  std::vector<DigiHitView> views;
  for( size_t b = 0; b < boards.size(); b++ ){
    DigiHitView v; v.sn = boards[b].sn; v.digiIndex = (uint8_t)b; v.ring = boards[b].ring.get();
    views.push_back(v);
  }

  OnlineEventBuilder eb(views);
  eb.SetTimeWindow(window);
  eb.SetGuardTime(window);
  eb.SetStallTimeout(1u << 30);
  eb.Reset();

  std::vector<Event> out;
  eb.onEvent = [&out](const Event & e){ out.push_back(e); };

  bool more = true;
  while( more ){
    more = false;
    for( size_t b = 0; b < boards.size(); b++ ){
      Board & bd = boards[b];
      const size_t n = std::min(chunk, bd.hits.size() - bd.pushed);
      for( size_t k = 0; k < n; k++ ) bd.ring->push(bd.hits[bd.pushed++]);
      if( bd.pushed < bd.hits.size() ) more = true;
    }
    eb.BuildEvents(false);
  }
  eb.BuildEvents(true);

  if( statOut ) *statOut = eb;
  return out;
}

/// Push everything up front, then drain. With more hits than LeanHitRingSize this guarantees the
/// producer laps the consumer, which is the only way to exercise the fast-forward path.
static std::vector<Event> LapBuild(std::vector<Board> & boards, uint64_t window,
                                   long & dropped, long & late, long & consumed){

  RewindBoards(boards);

  std::vector<DigiHitView> views;
  for( size_t b = 0; b < boards.size(); b++ ){
    DigiHitView v; v.sn = boards[b].sn; v.digiIndex = (uint8_t)b; v.ring = boards[b].ring.get();
    views.push_back(v);
  }

  OnlineEventBuilder eb(views);
  eb.SetTimeWindow(window);
  eb.SetGuardTime(window);
  eb.SetStallTimeout(1u << 30);
  eb.Reset();

  std::vector<Event> out;
  eb.onEvent = [&out](const Event & e){ out.push_back(e); };

  for( size_t b = 0; b < boards.size(); b++ ){
    Board & bd = boards[b];
    while( bd.pushed < bd.hits.size() ) bd.ring->push(bd.hits[bd.pushed++]);
  }
  eb.BuildEvents(true);

  dropped  = eb.totalHitsDropped;
  late     = eb.totalHitsLate;
  consumed = eb.totalHitsConsumed;
  return out;
}

/// One pusher thread per board running flat out while the main thread drains. Deterministic tests
/// cannot catch a tearing or lapping race; this can, especially under TSan.
static void ThreadedRun(std::vector<Board> & boards, uint64_t window){

  RewindBoards(boards);

  std::vector<DigiHitView> views;
  for( size_t b = 0; b < boards.size(); b++ ){
    DigiHitView v; v.sn = boards[b].sn; v.digiIndex = (uint8_t)b; v.ring = boards[b].ring.get();
    views.push_back(v);
  }

  OnlineEventBuilder eb(views);
  eb.SetTimeWindow(window);
  eb.SetGuardTime(window);
  eb.SetStallTimeout(1u << 30);
  eb.Reset();

  size_t hitsInEvents = 0;
  eb.onEvent = [&hitsInEvents](const Event & e){ hitsInEvents += e.size(); };

  /// Each board's `pushed` is touched only by its own pusher thread; the main thread watches this
  /// counter instead, so the harness itself contributes no race for TSan to trip over.
  std::atomic<int> stillPushing((int) boards.size());

  std::vector<std::thread> pushers;
  for( size_t b = 0; b < boards.size(); b++ ){
    Board * bd = &boards[b];
    pushers.push_back(std::thread([bd, &stillPushing](){
      while( bd->pushed < bd->hits.size() ) bd->ring->push(bd->hits[bd->pushed++]);
      stillPushing--;
    }));
  }

  while( stillPushing.load() > 0 ) eb.BuildEvents(false);

  for( size_t i = 0; i < pushers.size(); i++ ) pushers[i].join();
  eb.BuildEvents(true);

  const size_t pushedTotal = TotalHits(boards);
  printf("  pushed %zu, in events %zu, dropped %ld, late %ld\n",
         pushedTotal, hitsInEvents, eb.totalHitsDropped, eb.totalHitsLate);
  eb.PrintStat();

  const long accounted = (long)hitsInEvents + eb.totalHitsDropped + eb.totalHitsLate;
  printf("  accounting %s: %zu pushed vs %ld accounted\n",
         accounted == (long)pushedTotal ? "OK" : "MISMATCH", pushedTotal, accounted);
}

//^===================================================================== sources

/// Poisson singles per board, plus injected coincidence groups whose membership is known by
/// construction. Fixed seed, so runs are reproducible.
static std::vector<Board> MakeSynth(int nBoards, int nCh, double singlesHzPerBoard,
                                    double coincHz, double seconds, uint64_t window,
                                    unsigned seed){

  std::vector<Board> boards(nBoards);
  for( int b = 0; b < nBoards; b++ ) boards[b].sn = (uint16_t)(50000 + b);

  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> chPick(0, nCh - 1);
  std::uniform_int_distribution<int> ePick(100, 30000);
  std::uniform_int_distribution<int> boardPick(0, nBoards - 1);

  const double totalNs = seconds * 1e9;

  //---- singles
  for( int b = 0; b < nBoards; b++ ){
    std::exponential_distribution<double> gap(singlesHzPerBoard / 1e9);
    double t = 0;
    while( true ){
      t += gap(rng);
      if( t >= totalNs ) break;
      LeanHit h;
      h.timestamp = (uint64_t) t;
      h.energy = (uint16_t) ePick(rng);  h.energy_short = 0;
      h.fine_timestamp = 0;
      h.channel = (uint8_t) chPick(rng); h.flagsHigh = 0;
      boards[b].hits.push_back(h);
    }
  }

  //---- coincidence groups: 2..nBoards boards firing inside the window
  if( nBoards > 1 && coincHz > 0 ){
    std::exponential_distribution<double> gap(coincHz / 1e9);
    std::uniform_int_distribution<uint64_t> jitter(0, window > 1 ? window - 1 : 0);
    double t = 0;
    while( true ){
      t += gap(rng);
      if( t >= totalNs ) break;
      const int mult = 2 + (boardPick(rng) % (nBoards - 1));
      std::vector<int> used;
      for( int m = 0; m < mult; m++ ){
        int b = boardPick(rng);
        if( std::find(used.begin(), used.end(), b) != used.end() ) continue;
        used.push_back(b);
        LeanHit h;
        h.timestamp = (uint64_t) t + jitter(rng);
        h.energy = (uint16_t) ePick(rng);  h.energy_short = 0;
        h.fine_timestamp = 0;
        h.channel = (uint8_t) chPick(rng); h.flagsHigh = LeanHitFlag::FineTSValid;
        boards[b].hits.push_back(h);
      }
    }
  }

  /// A board's stream must be time-ordered — that is the assumption the whole merge rests on.
  for( int b = 0; b < nBoards; b++ ){
    std::sort(boards[b].hits.begin(), boards[b].hits.end(),
              [](const LeanHit & a, const LeanHit & c){ return a.timestamp < c.timestamp; });
  }

  return boards;
}

/// sn is the second-to-last underscore-separated token, as Aux/EventBuilder.cpp:145-170 parses it.
static uint16_t SnFromName(const std::string & path){
  size_t slash = path.find_last_of('/');
  std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
  std::vector<std::string> tok;
  size_t start = 0;
  while( true ){
    size_t u = base.find('_', start);
    if( u == std::string::npos ){ tok.push_back(base.substr(start)); break; }
    tok.push_back(base.substr(start, u - start));
    start = u + 1;
  }
  if( tok.size() < 2 ) return 0;
  return (uint16_t) atoi(tok[tok.size() - 2].c_str());
}

static std::vector<Board> MakeReplay(const std::vector<std::string> & files){

  std::vector<Board> boards(files.size());

  for( size_t f = 0; f < files.size(); f++ ){

    boards[f].sn = SnFromName(files[f]);

    SolReader reader;
    reader.OpenFile(files[f]);

    unsigned short fmt = 0xFFFF;
    while( !reader.IsEndOfFile() ){
      if( reader.ReadNextBlock() != 0 ) break;
      if( fmt == 0xFFFF ) fmt = reader.hit->dataType;
      LeanHit h;
      /// .sol timestamps were already scaled by tick2ns before being written, so no scaling here.
      h.timestamp      = reader.hit->timestamp;
      h.energy         = reader.hit->energy;
      h.energy_short   = reader.hit->energy_short;
      h.fine_timestamp = reader.hit->fine_timestamp;
      h.channel        = reader.hit->channel;
      h.flagsHigh      = (uint8_t)(reader.hit->flags_high_priority & 0xFF);
      boards[f].hits.push_back(h);
    }

    printf("  %s : sn %u, %zu hits, dataType 0x%02X%s\n",
           files[f].c_str(), boards[f].sn, boards[f].hits.size(), fmt,
           (fmt == DataFormat::Minimum || fmt == DataFormat::MiniWithFineTime)
             ? "  (Minimum format: flagsHigh is always 0, a pile-up cut would keep everything)"
             : "");
  }

  return boards;
}

//^===================================================================== analysis driver

/// A HistRegistry that owns no widgets and just tallies. Enough to prove the interface is
/// genuinely Qt-free, that the registrar self-registers, and that an analysis fills what it says.
class TallyRegistry : public HistRegistry {
public:
  struct Hist {
    std::string name;
    int    nX, nY, row, col;
    double xLo, xHi, yLo, yHi;
    long   fills, under, over;
    double xSum, ySum;
  };
  std::vector<Hist> h1, h2;

  struct Chan { std::string label; int * digi; int * ch; };
  struct Flag { std::string label; bool * value; };
  std::vector<Chan> chans;
  std::vector<Flag> flags;

  int Hist1D(const char * name, const char *, int nBin, double xMin, double xMax,
             int row, int col) override {
    Hist h{}; h.name = name; h.nX = nBin; h.xLo = xMin; h.xHi = xMax; h.row = row; h.col = col;
    h1.push_back(h);
    return (int)h1.size() - 1;
  }

  int Hist2D(const char * name, const char *, const char *,
             int nX, double xMin, double xMax,
             int nY, double yMin, double yMax, int row, int col) override {
    Hist h{}; h.name = name; h.nX = nX; h.xLo = xMin; h.xHi = xMax;
    h.nY = nY; h.yLo = yMin; h.yHi = yMax; h.row = row; h.col = col;
    h2.push_back(h);
    return (int)h2.size() - 1;
  }

  void Fill1(int id, double x) override {
    if( id < 0 || id >= (int)h1.size() ) return;
    Hist & h = h1[id];
    if( x < h.xLo ) { h.under++; return; }
    if( x >= h.xHi ){ h.over++;  return; }
    h.fills++; h.xSum += x;
  }

  void Fill2(int id, double x, double y) override {
    if( id < 0 || id >= (int)h2.size() ) return;
    Hist & h = h2[id];
    if( x < h.xLo || x >= h.xHi || y < h.yLo || y >= h.yHi ){ h.over++; return; }
    h.fills++; h.xSum += x; h.ySum += y;
  }

  void ChannelParam(const char * label, int * digi, int * channel) override {
    chans.push_back({std::string(label), digi, channel});
  }
  void BoolParam(const char * label, bool * value) override {
    flags.push_back({std::string(label), value});
  }

  void Print() const {
    for( size_t i = 0; i < h1.size(); i++ ){
      const Hist & h = h1[i];
      printf("  1D %-16s [%g, %g) %4d bins : %8ld fills, mean %10.2f, under %ld, over %ld\n",
             h.name.c_str(), h.xLo, h.xHi, h.nX, h.fills,
             h.fills ? h.xSum / h.fills : 0.0, h.under, h.over);
    }
    for( size_t i = 0; i < h2.size(); i++ ){
      const Hist & h = h2[i];
      printf("  2D %-16s x[%g, %g) y[%g, %g) : %8ld fills, mean (%.1f, %.1f), outside %ld\n",
             h.name.c_str(), h.xLo, h.xHi, h.yLo, h.yHi, h.fills,
             h.fills ? h.xSum / h.fills : 0.0, h.fills ? h.ySum / h.fills : 0.0, h.over);
    }
  }
};

/// Drive a registered analysis over the boards, through the real builder and the real event ring.
static bool RunAnalysis(std::vector<Board> & boards, uint64_t window, const std::string & name,
                        int dA, int cA, int dB, int cB){

  Analysis * ana = AnalysisRegistry::Instance().Create(name);
  if( ana == nullptr ){
    printf("no analysis called \"%s\". Registered:", name.c_str());
    std::vector<std::string> n = AnalysisRegistry::Instance().Names();
    for( size_t i = 0; i < n.size(); i++ ) printf(" %s", n[i].c_str());
    printf("\n");
    return false;
  }

  std::vector<uint8_t> nCh;
  std::vector<DigiHitView> views;
  for( size_t b = 0; b < boards.size(); b++ ){
    DigiHitView v; v.sn = boards[b].sn; v.digiIndex = (uint8_t)b; v.ring = boards[b].ring.get();
    views.push_back(v);
    nCh.push_back(64);
  }

  AnalysisContext ctx;
  ctx.nBoards    = (int)boards.size();
  ctx.nChannels  = nCh.data();
  ctx.timeWindow = window;

  TallyRegistry reg;
  ana->Declare(reg, ctx);

  //---- apply the channel selection the way the GUI would
  if( reg.chans.size() >= 1 ){ *reg.chans[0].digi = dA; *reg.chans[0].ch = cA; }
  if( reg.chans.size() >= 2 ){ *reg.chans[1].digi = dB; *reg.chans[1].ch = cB; }
  printf("analysis \"%s\": %zu channel params, %zu flags, %zu 1D, %zu 2D\n",
         name.c_str(), reg.chans.size(), reg.flags.size(), reg.h1.size(), reg.h2.size());
  for( size_t i = 0; i < reg.chans.size(); i++ ){
    printf("  param %-12s = digi %d ch %d\n",
           reg.chans[i].label.c_str(), *reg.chans[i].digi, *reg.chans[i].ch);
  }

  RewindBoards(boards);

  std::unique_ptr<EventRing> ring(new EventRing());
  EventRing::Reader reader;
  reader.Rewind(*ring);

  OnlineEventBuilder eb(views);
  eb.SetTimeWindow(window);
  eb.SetGuardTime(window);
  eb.SetStallTimeout(1u << 30);
  eb.SetEventSink(ring.get());
  eb.Reset();

  ana->OnRunStart();

  Event scratch;
  long nEvents = 0;
  bool more = true;
  while( more ){
    more = false;
    for( size_t b = 0; b < boards.size(); b++ ){
      Board & bd = boards[b];
      const size_t n = std::min((size_t)500, bd.hits.size() - bd.pushed);
      for( size_t k = 0; k < n; k++ ) bd.ring->push(bd.hits[bd.pushed++]);
      if( bd.pushed < bd.hits.size() ) more = true;
    }
    eb.BuildEvents(false);
    while( reader.Next(*ring, scratch) ){
      ana->ProcessEvent(scratch.data(), (int)scratch.size(), reg);
      nEvents++;
    }
  }
  eb.BuildEvents(true);
  while( reader.Next(*ring, scratch) ){
    ana->ProcessEvent(scratch.data(), (int)scratch.size(), reg);
    nEvents++;
  }

  ana->OnRunStop();

  printf("\n%ld events through the analysis (ring dropped %ld, torn %ld)\n",
         nEvents, reader.eventsDropped, reader.eventsTorn);
  reg.Print();
  eb.PrintStat();

  delete ana;
  return reader.eventsDropped == 0 && reader.eventsTorn == 0;
}

//^===================================================================== output

static void DumpEvents(const std::vector<Event> & evs, size_t maxPrint){
  for( size_t i = 0; i < evs.size() && i < maxPrint; i++ ){
    printf("Event %zu  multiplicity %zu\n", i, evs[i].size());
    for( size_t k = 0; k < evs[i].size(); k++ ){
      const BuiltHit & h = evs[i][k];
      printf("   sn %5u  digi %u  ch %2u  e %6u  e_s %6u  ts %" PRIu64 " ns  fine %u  flags 0x%02X\n",
             h.sn, h.digi, h.channel, h.energy, h.energy_short, h.timestamp,
             h.fine_timestamp, h.flagsHigh);
    }
  }
  if( evs.size() > maxPrint ) printf("... %zu more events\n", evs.size() - maxPrint);
}

static void Summary(const std::vector<Event> & evs){
  size_t hits = 0, maxMult = 0;
  for( size_t i = 0; i < evs.size(); i++ ){
    hits += evs[i].size();
    if( evs[i].size() > maxMult ) maxMult = evs[i].size();
  }
  printf("  %zu events, %zu hits, mean multiplicity %.3f, max %zu\n",
         evs.size(), hits, evs.empty() ? 0.0 : (double)hits/evs.size(), maxMult);
}

//^===================================================================== event ring

/// The ring in isolation: round-trip fidelity, then deliberate overrun.
static bool RingTest(){

  bool ok = true;
  printf("\n--- 7. event ring round trip ---\n");

  std::unique_ptr<EventRing> ring(new EventRing());   // ~8 MiB, never on the stack
  EventRing::Reader reader;
  reader.Rewind(*ring);

  //---- round trip: varying multiplicity, every field must survive
  std::vector<Event> sent;
  std::mt19937 rng(4242);
  std::uniform_int_distribution<int> multPick(1, 12);
  for( int i = 0; i < 5000; i++ ){
    Event ev;
    const int m = multPick(rng);
    for( int k = 0; k < m; k++ ){
      BuiltHit h;
      h.timestamp = (uint64_t)i * 1000 + k;
      h.sn = (uint16_t)(50000 + k);  h.energy = (uint16_t)(i + k);
      h.energy_short = (uint16_t)k;  h.fine_timestamp = (uint16_t)(k * 7);
      h.digi = (uint8_t)(k % 4);     h.channel = (uint8_t)k;
      h.flagsHigh = (uint8_t)(i & 0xFF);
      ev.push_back(h);
    }
    sent.push_back(ev);
    ring->Publish(ev);
  }

  std::vector<Event> got;
  Event scratch;
  while( reader.Next(*ring, scratch) ) got.push_back(scratch);

  bool same = SameEvents(sent, got, "ring round trip");
  printf("  %zu events in, %zu out, dropped %ld, torn %ld : %s\n",
         sent.size(), got.size(), reader.eventsDropped, reader.eventsTorn,
         (same && reader.eventsDropped == 0 && reader.eventsTorn == 0) ? "OK" : "FAILED");
  ok &= same && reader.eventsDropped == 0 && reader.eventsTorn == 0;

  //---- overrun: publish far more than the ring holds without draining
  printf("\n--- 8. event ring overrun accounting ---\n");
  std::unique_ptr<EventRing> ring2(new EventRing());
  EventRing::Reader r2;
  r2.Rewind(*ring2);

  const long nPublish = (long)EventRingSize * 3;      // 3x the index ring depth
  for( long i = 0; i < nPublish; i++ ){
    Event ev;
    BuiltHit h; h.timestamp = (uint64_t)i; h.sn = 1; h.energy = (uint16_t)i;
    h.energy_short = 0; h.fine_timestamp = 0; h.digi = 0; h.channel = 0; h.flagsHigh = 0;
    ev.push_back(h);
    ring2->Publish(ev);
  }

  long drained = 0;
  while( r2.Next(*ring2, scratch) ) drained++;

  /// Every published event is either read or accounted as dropped/torn. The hit ring is deeper
  /// than the index ring here (1 hit per event), so the index ring is what laps.
  const long accounted = drained + r2.eventsDropped + r2.eventsTorn;
  const bool balances  = (accounted == nPublish);
  const bool lapped    = (r2.eventsDropped > 0);
  printf("  published %ld, read %ld, dropped %ld, torn %ld\n",
         nPublish, drained, r2.eventsDropped, r2.eventsTorn);
  printf("  lap occurred %s, accounting %s\n",
         lapped ? "yes" : "NO (test did not exercise the path)",
         balances ? "OK" : "MISMATCH");
  ok &= lapped && balances;

  return ok;
}

/// The sink and the callback are two views of the same stream and must agree exactly. Drains the
/// ring as it goes so it never laps; any loss here would be a bug, not backpressure.
static bool SinkTest(uint64_t window){

  printf("\n--- 9. event sink vs onEvent ---\n");

  std::vector<Board> boards = MakeSynth(3, 16, 30000, 4000, 0.3, window, 31337);
  RewindBoards(boards);

  std::vector<DigiHitView> views;
  for( size_t b = 0; b < boards.size(); b++ ){
    DigiHitView v; v.sn = boards[b].sn; v.digiIndex = (uint8_t)b; v.ring = boards[b].ring.get();
    views.push_back(v);
  }

  std::unique_ptr<EventRing> ring(new EventRing());
  EventRing::Reader reader;
  reader.Rewind(*ring);

  OnlineEventBuilder eb(views);
  eb.SetTimeWindow(window);
  eb.SetGuardTime(window);
  eb.SetStallTimeout(1u << 30);
  eb.SetEventSink(ring.get());
  eb.Reset();

  std::vector<Event> viaCallback;
  eb.onEvent = [&viaCallback](const Event & e){ viaCallback.push_back(e); };

  std::vector<Event> viaRing;
  Event scratch;

  bool more = true;
  while( more ){
    more = false;
    for( size_t b = 0; b < boards.size(); b++ ){
      Board & bd = boards[b];
      const size_t n = std::min((size_t)500, bd.hits.size() - bd.pushed);
      for( size_t k = 0; k < n; k++ ) bd.ring->push(bd.hits[bd.pushed++]);
      if( bd.pushed < bd.hits.size() ) more = true;
    }
    eb.BuildEvents(false);
    while( reader.Next(*ring, scratch) ) viaRing.push_back(scratch);
  }
  eb.BuildEvents(true);
  while( reader.Next(*ring, scratch) ) viaRing.push_back(scratch);

  const bool same = SameEvents(viaCallback, viaRing, "sink vs callback");
  const bool ok   = same && reader.eventsDropped == 0 && reader.eventsTorn == 0;
  printf("  %zu via callback, %zu via ring, dropped %ld, torn %ld : %s\n",
         viaCallback.size(), viaRing.size(), reader.eventsDropped, reader.eventsTorn,
         ok ? "OK" : "FAILED");
  return ok;
}

//^===================================================================== selftest

/// The property that matters most in a real run: one board going quiet must not freeze the rest.
/// Board 1 stops after 4 us while board 0 keeps producing; once board 1 has been silent longer
/// than the stall timeout it must drop out of the build horizon and board 0's events must flow —
/// WITHOUT an isFinal flush, which is what would paper over a stall in a real run.
static bool StallTest(){

  printf("\n--- 6. a silent board must not freeze the builder ---\n");

  std::vector<Board> st(2);
  st[0].sn = 1;
  st[1].sn = 2;

  for( uint64_t t = 0; t < 100000; t += 1000 ){
    LeanHit h; h.timestamp = t; h.energy = 10; h.energy_short = 0;
    h.fine_timestamp = 0; h.channel = 0; h.flagsHigh = 0;
    st[0].hits.push_back(h);
  }
  for( uint64_t t = 0; t < 5000; t += 1000 ){
    LeanHit h; h.timestamp = t; h.energy = 20; h.energy_short = 0;
    h.fine_timestamp = 0; h.channel = 1; h.flagsHigh = 0;
    st[1].hits.push_back(h);
  }

  std::vector<DigiHitView> views;
  for( size_t b = 0; b < st.size(); b++ ){
    DigiHitView v; v.sn = st[b].sn; v.digiIndex = (uint8_t)b; v.ring = st[b].ring.get();
    views.push_back(v);
  }

  OnlineEventBuilder eb(views);
  eb.SetTimeWindow(100);
  eb.SetGuardTime(100);
  eb.SetStallTimeout(50);
  eb.Reset();

  uint64_t maxTs = 0;
  eb.onEvent = [&maxTs](const Event & e){
    for( size_t i = 0; i < e.size(); i++ ) if( e[i].timestamp > maxTs ) maxTs = e[i].timestamp;
  };

  //---- board 1 says everything it will ever say, up front
  while( st[1].pushed < st[1].hits.size() ) st[1].ring->push(st[1].hits[st[1].pushed++]);

  //---- board 0 keeps talking, in slices, long enough for board 1 to trip the stall timeout
  for( int round = 0; round < 8; round++ ){
    const size_t n = std::min((size_t)13, st[0].hits.size() - st[0].pushed);
    for( size_t k = 0; k < n; k++ ) st[0].ring->push(st[0].hits[st[0].pushed++]);
    eb.BuildEvents(false);            // note: never isFinal
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  eb.BuildEvents(false);

  /// Board 1's last hit is at 4000. If the stall handling works, board 0's events well past that
  /// have been built with no isFinal flush; if it does not, the build horizon is pinned at 4000 and
  /// maxTs never gets past it.
  const bool flowed  = maxTs > 50000;
  const bool stalled = eb.stallEvents > 0;

  printf("  board 1 silent after ts 4000; built up to ts %" PRIu64 ", stalls %ld, late %ld\n",
         maxTs, eb.stallEvents, eb.totalHitsLate);
  printf("  %s\n", (flowed && stalled) ? "OK" :
                   (!stalled ? "FAILED: the board was never declared stalled"
                             : "FAILED: builder froze behind the silent board"));

  return flowed && stalled;
}

static bool SelfTest(uint64_t window){

  bool ok = true;

  printf("\n--- 1. reference oracle, 4 boards ---\n");
  std::vector<Board> boards = MakeSynth(4, 16, 20000, 5000, 0.5, window, 12345);
  printf("  %zu hits across %zu boards\n", TotalHits(boards), boards.size());

  std::vector<Event> ref    = ReferenceBuild(boards, window);
  std::vector<Event> stream = StreamBuild(boards, window, 1000);
  Summary(ref);
  ok &= SameEvents(ref, stream, "streaming vs reference");
  if( ok ) printf("  OK\n");

  printf("\n--- 2. chunk invariance ---\n");
  const size_t chunks[] = {1, 17, 1000, 1000000};
  for( size_t c = 0; c < sizeof(chunks)/sizeof(chunks[0]); c++ ){
    std::vector<Event> s = StreamBuild(boards, window, chunks[c]);
    char label[64]; snprintf(label, sizeof(label), "chunk %zu vs reference", chunks[c]);
    bool good = SameEvents(ref, s, label);
    ok &= good;
    printf("  chunk %-8zu %s\n", chunks[c], good ? "OK" : "FAILED");
  }

  printf("\n--- 3. single board (merge is the identity) ---\n");
  std::vector<Board> one = MakeSynth(1, 64, 100000, 0, 0.5, window, 999);
  std::vector<Event> refOne = ReferenceBuild(one, window);
  std::vector<Event> strOne = StreamBuild(one, window, 137);
  Summary(refOne);
  bool good1 = SameEvents(refOne, strOne, "single board");
  ok &= good1;
  printf("  %s\n", good1 ? "OK" : "FAILED");

  printf("\n--- 4. timeWindow = 0 (one hit per event) ---\n");
  std::vector<Event> refZ = ReferenceBuild(one, 0);
  std::vector<Event> strZ = StreamBuild(one, 0, 137);
  bool good0 = SameEvents(refZ, strZ, "window 0");
  ok &= good0;
  printf("  %zu events for %zu hits, %s\n", strZ.size(), TotalHits(one),
         (good0 && strZ.size() == TotalHits(one)) ? "OK" : "FAILED");
  ok &= (strZ.size() == TotalHits(one));

  printf("\n--- 5. forced lap accounting ---\n");
  /// More hits than one ring can hold, all pushed before any drain, so the fast-forward path runs.
  std::vector<Board> big = MakeSynth(2, 16, 400000, 10000, 1.0, window, 777);
  const size_t pushed = TotalHits(big);
  printf("  %zu hits into rings of %zu\n", pushed, (size_t)LeanHitRingSize);
  long dropped = 0, late = 0, consumed = 0;
  std::vector<Event> lapped = LapBuild(big, window, dropped, late, consumed);
  size_t inEvents = 0;
  for( size_t i = 0; i < lapped.size(); i++ ) inEvents += lapped[i].size();
  printf("  in events %zu, dropped %ld, late %ld, consumed %ld\n",
         inEvents, dropped, late, consumed);
  const bool lapHappened = dropped > 0;
  const bool balances    = ((long)inEvents + dropped + late == (long)pushed) &&
                           ((long)inEvents + late == consumed);
  printf("  lap occurred %s, accounting %s\n",
         lapHappened ? "yes" : "NO (test did not exercise the path)",
         balances ? "OK" : "MISMATCH");
  ok &= lapHappened && balances;

  ok &= StallTest();
  ok &= RingTest();
  ok &= SinkTest(window);

  return ok;
}

//^=====================================================================

int main(int argc, char ** argv){

  enum Mode { SYNTH, REPLAY, SELFTEST, THREADED } mode = SELFTEST;
  std::string anaName;
  int dA = 0, cA = 0, dB = 0, cB = 1;

  int      nBoards = 4, nCh = 16;
  double   singlesHz = 20000, coincHz = 5000, seconds = 0.5;
  uint64_t window = 100;
  size_t   chunk = 1000;
  bool     dump = false;
  unsigned seed = 12345;
  std::vector<std::string> files;

  for( int i = 1; i < argc; i++ ){
    std::string a = argv[i];
    if      ( a == "--selftest" ) mode = SELFTEST;
    else if ( a == "--synth"    ) mode = SYNTH;
    else if ( a == "--threaded" ) mode = THREADED;
    else if ( a == "--dump"     ) dump = true;
    else if ( a == "--replay"   ){ mode = REPLAY;
                                   while( i+1 < argc && argv[i+1][0] != '-' ) files.push_back(argv[++i]); }
    else if ( a == "--boards"  && i+1 < argc ) nBoards   = atoi(argv[++i]);
    else if ( a == "--channels"&& i+1 < argc ) nCh       = atoi(argv[++i]);
    else if ( a == "--rate"    && i+1 < argc ) singlesHz = atof(argv[++i]);
    else if ( a == "--coinc"   && i+1 < argc ) coincHz   = atof(argv[++i]);
    else if ( a == "--seconds" && i+1 < argc ) seconds   = atof(argv[++i]);
    else if ( a == "--window"  && i+1 < argc ) window    = strtoull(argv[++i], NULL, 10);
    else if ( a == "--chunk"   && i+1 < argc ) chunk     = (size_t) strtoull(argv[++i], NULL, 10);
    else if ( a == "--seed"    && i+1 < argc ) seed      = (unsigned) strtoul(argv[++i], NULL, 10);
    else if ( a == "--analysis"&& i+1 < argc ) anaName   = argv[++i];
    else if ( a == "--chA"     && i+2 < argc ){ dA = atoi(argv[++i]); cA = atoi(argv[++i]); }
    else if ( a == "--chB"     && i+2 < argc ){ dB = atoi(argv[++i]); cB = atoi(argv[++i]); }
    else {
      printf("usage: %s [--selftest | --synth | --threaded | --replay f1.sol ...]\n"
             "          [--boards N] [--channels N] [--rate Hz] [--coinc Hz] [--seconds S]\n"
             "          [--window ns] [--chunk N] [--seed N] [--dump]\n"
             "          [--analysis <name>] [--chA <digi> <ch>] [--chB <digi> <ch>]\n", argv[0]);
      return a == "--help" ? 0 : 1;
    }
  }

  printf("LeanHit %zu bytes, ring %zu hits (%.1f MiB per board), window %" PRIu64 " ns\n",
         sizeof(LeanHit), (size_t)LeanHitRingSize,
         sizeof(LeanHit) * (double)LeanHitRingSize / 1048576.0, window);

  if( mode == SELFTEST ){
    const bool ok = SelfTest(window);
    printf("\n=========== SELFTEST %s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
  }

  std::vector<Board> boards;
  if( mode == REPLAY ){
    if( files.empty() ){ printf("--replay needs at least one .sol file\n"); return 1; }
    printf("reading:\n");
    boards = MakeReplay(files);
  }else{
    boards = MakeSynth(nBoards, nCh, singlesHz, coincHz, seconds, window, seed);
    printf("synth: %d boards x %d ch, %.0f Hz singles/board, %.0f Hz coincidences, %.2f s\n",
           nBoards, nCh, singlesHz, coincHz, seconds);
  }
  printf("  %zu hits total\n", TotalHits(boards));
  if( TotalHits(boards) == 0 ){ printf("no hits, nothing to do\n"); return 1; }

  if( !anaName.empty() ){
    return RunAnalysis(boards, window, anaName, dA, cA, dB, cB) ? 0 : 1;
  }

  if( mode == THREADED ){
    printf("\n--- threaded push while draining ---\n");
    ThreadedRun(boards, window);
    return 0;
  }

  OnlineEventBuilder stat((std::vector<DigiHitView>()));
  std::vector<Event> evs = StreamBuild(boards, window, chunk, &stat);

  printf("\nstreaming:\n");
  Summary(evs);
  stat.PrintStat();

  std::vector<Event> ref = ReferenceBuild(boards, window);
  printf("\nreference:\n");
  Summary(ref);
  printf("  agreement: %s\n", SameEvents(ref, evs, "streaming vs reference") ? "OK" : "MISMATCH");

  if( dump ) DumpEvents(evs, 20);

  return 0;
}
