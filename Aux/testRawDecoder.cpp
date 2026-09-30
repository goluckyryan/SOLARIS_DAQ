//^####################################################
// testRawDecoder : validate RawDecoder.h against a live digitizer
//
// Uses the board's internal test pulse to generate a known, periodic,
// fixed-amplitude stimulus, then checks that RawDecoder agrees with the
// firmware's own dpppha decode and with the known pulse period.
//
// This tool NEVER calls ProgramBoard() / ProgramChannels(). It saves the
// handful of registers it needs, writes only those, and restores them.
// Channels 4..63 are never read and never written.
//
// Usage: ./testRawDecoder [url]
//   default url: dig2://192.168.0.100/
//
// Exit codes:
//   0  all checks passed, board restored
//   1  a check failed (board still restored)
//   2  aborted during save, nothing was written
//   3  restore incomplete -- manual fix list printed
//####################################################

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <csignal>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#include <CAEN_FELib.h>

#include "../ClassDigitizer2Gen.h"
#include "../RawDecoder.h"

//^==================================================== globals

static volatile sig_atomic_t gStop = 0;
static uint64_t gHandle = 0;
static unsigned short gTick2ns = 8;

static void SigHandler(int){ gStop = 1; }   // flag only, no FELib calls here

static double Now(){
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

//^==================================================== path helpers
//
// The "/ch/0..63/par/X" range syntax is WRITE-ONLY on this firmware: reading it
// returns an error, not a value. Save/restore must never construct it. These two
// helpers are the only way paths are built, and both refuse a ".." anywhere.

static std::string GuardPath(const std::string& p){
  if( p.find("..") != std::string::npos ){
    printf("\n*** INTERNAL ERROR: range path '%s' constructed for save/restore.\n", p.c_str());
    printf("*** The /ch/a..b/ form is write-only; reading it cannot round-trip. Aborting.\n");
    exit(2);
  }
  return p;
}

static std::string ChPath(int ch, const char * name){
  return GuardPath("/ch/" + std::to_string(ch) + "/par/" + name);
}

static std::string DigPath(const char * name){
  return GuardPath(std::string("/par/") + name);
}

//^==================================================== direct FELib access
//
// Digitizer2Gen::ReadValue returns a std::string and signals failure by returning
// one of ~15 CAEN error descriptions -- there is no sentinel to test for. WriteValue
// additionally clamps into the Reg min/max and mutates the object's settings cache.
// For save/restore both are unacceptable, so we talk to FELib directly: int codes,
// no clamping, and the digitizer object's cache is left exactly as we found it.

static int FeGet(const std::string& path, std::string& out){
  char buf[256] = {0};
  int ret = CAEN_FELib_GetValue(gHandle, path.c_str(), buf);
  if( ret == CAEN_FELib_Success ) out = buf;
  return ret;
}

static int FeSet(const std::string& path, const std::string& val){
  return CAEN_FELib_SetValue(gHandle, path.c_str(), val.c_str());
}

//^==================================================== save / restore

struct SavedReg {
  std::string path;
  std::string value;     // value read during Phase A
  std::string applied;   // value we wrote in Phase B ("" = saved but not written)
};

class BoardStateGuard {
public:
  BoardStateGuard() : restored_(false), restoreOk_(true) {}
  ~BoardStateGuard(){ if( !restored_ ) Restore(); }

  /// Phase A. Double-read: a register that disagrees with itself is volatile and
  /// must not be restored. Any failure here aborts before a single write happened.
  bool Save(const std::string& path){
    std::string a, b;
    int r1 = FeGet(path, a);
    if( r1 != CAEN_FELib_Success ){
      printf("  [SAVE FAIL] %s : CAEN_FELib error %d\n", path.c_str(), r1);
      return false;
    }
    int r2 = FeGet(path, b);
    if( r2 != CAEN_FELib_Success ){
      printf("  [SAVE FAIL] %s : second read gave CAEN_FELib error %d\n", path.c_str(), r2);
      return false;
    }
    if( a != b ){
      printf("  [SAVE FAIL] %s : volatile, read '%s' then '%s'\n", path.c_str(), a.c_str(), b.c_str());
      return false;
    }
    regs_.push_back({path, a, ""});
    return true;
  }

  /// Phase B. Only registers already saved may be written.
  bool Apply(const std::string& path, const std::string& value){
    for( auto &r : regs_ ){
      if( r.path == path ){
        int ret = FeSet(path, value);
        if( ret != CAEN_FELib_Success ){
          printf("  [APPLY FAIL] %s = %s : CAEN_FELib error %d\n", path.c_str(), value.c_str(), ret);
          return false;
        }
        r.applied = value;
        return true;
      }
    }
    printf("  [APPLY FAIL] %s was never saved -- refusing to write it.\n", path.c_str());
    return false;
  }

  void DumpRescueFile(const char * fname) const {
    FILE * f = fopen(fname, "w");
    if( !f ) return;
    fprintf(f, "# testRawDecoder rescue file -- original board state before any write.\n");
    fprintf(f, "# If this process died hard, these are the values to put back.\n");
    for( auto &r : regs_ ) fprintf(f, "%s = %s\n", r.path.c_str(), r.value.c_str());
    fclose(f);
    printf("  Rescue file written: %s\n", fname);
  }

  /// Phase C. Reverse apply order, which also resolves the ChPreTriggerT /
  /// ChRecordLengthT ordering hazard (pre-trigger must never exceed record length,
  /// even transiently). Then read everything back and prove it took.
  bool Restore(){
    restored_ = true;
    printf("\n--------------------------------------------\n");
    printf("  Restoring board state\n");
    printf("--------------------------------------------\n");

    int nWritten = 0;
    for( auto it = regs_.rbegin(); it != regs_.rend(); ++it ){
      if( it->applied.empty() ) continue;          // saved but never modified
      int ret = FeSet(it->path, it->value);
      if( ret != CAEN_FELib_Success ){
        printf("  [RESTORE ERROR] %s = %s : CAEN_FELib error %d\n",
               it->path.c_str(), it->value.c_str(), ret);
        restoreOk_ = false;
      }
      nWritten++;
    }

    // Verify by read-back, including the ones we never touched.
    int nBad = 0;
    for( auto &r : regs_ ){
      std::string now;
      int ret = FeGet(r.path, now);
      if( ret != CAEN_FELib_Success ){
        printf("  [VERIFY ERROR] %s : CAEN_FELib error %d\n", r.path.c_str(), ret);
        restoreOk_ = false; nBad++;
        continue;
      }
      if( now != r.value ){
        printf("  [MISMATCH] %s : want '%s', got '%s'\n", r.path.c_str(), r.value.c_str(), now.c_str());
        restoreOk_ = false; nBad++;
      }
    }

    printf("  Rewrote %d register(s), verified %zu, %d mismatch(es).\n",
           nWritten, regs_.size(), nBad);

    if( !restoreOk_ ){
      printf("\n*** BOARD NOT FULLY RESTORED. Fix by hand:\n");
      for( auto &r : regs_ ){
        std::string now;
        if( FeGet(r.path, now) == CAEN_FELib_Success && now == r.value ) continue;
        printf("    %s = %s\n", r.path.c_str(), r.value.c_str());
      }
      printf("\n");
    }else{
      printf("  Board state restored and verified.\n");
    }
    return restoreOk_;
  }

  bool RestoreOk() const { return restoreOk_; }

private:
  std::vector<SavedReg> regs_;
  bool restored_;
  bool restoreOk_;
};

//^==================================================== acquisition helpers

struct RefHit {          // from the dpppha (decoded) endpoint
  uint8_t  ch;
  uint16_t energy;
  uint64_t ts_ns;        // ALREADY scaled by tick2ns inside ReadData()
  uint16_t fine;
};

static void CollectRef(Digitizer2Gen& digi, double seconds, size_t maxHits,
                       std::vector<RefHit>& out){
  digi.StartACQ();
  double t0 = Now();
  while( Now() - t0 < seconds && out.size() < maxHits && !gStop ){
    if( digi.ReadData() == CAEN_FELib_Success ){
      out.push_back({digi.hit->channel, digi.hit->energy,
                     digi.hit->timestamp, digi.hit->fine_timestamp});
    }else{
      usleep(1000);
    }
  }
  digi.StopACQ();
}

static void CollectRaw(Digitizer2Gen& digi, double seconds, size_t maxBytes,
                       std::vector<std::vector<uint8_t>>& blobs){
  digi.StartACQ();
  double t0 = Now();
  size_t total = 0;
  while( Now() - t0 < seconds && total < maxBytes && !gStop ){
    if( digi.ReadData() == CAEN_FELib_Success && digi.hit->dataSize > 0 ){
      blobs.emplace_back(digi.hit->data, digi.hit->data + digi.hit->dataSize);
      total += digi.hit->dataSize;
    }else{
      usleep(1000);
    }
  }
  digi.StopACQ();
}

/// Decode a set of blobs into one flat hit list.
static void DecodeAll(const std::vector<std::vector<uint8_t>>& blobs,
                      const std::string& dppType, bool wf,
                      std::vector<RawDecoder::DecodedHit>& out,
                      std::vector<RawDecoder::StatUpdate>* stats = nullptr){
  RawDecoder dec;
  for( auto &b : blobs ){
    dec.LoadBlob(b.data(), b.size(), dppType, wf);
    RawDecoder::DecodedHit h;
    while( dec.Next(h) ) out.push_back(h);
    if( stats ) for( auto &s : dec.GetStatUpdates() ) stats->push_back(s);
  }
}

/// Compare every field EXCEPT the waveform vectors.
static bool SameCore(const RawDecoder::DecodedHit& a, const RawDecoder::DecodedHit& b){
  return a.channel             == b.channel
      && a.timestamp           == b.timestamp
      && a.fine_timestamp      == b.fine_timestamp
      && a.energy              == b.energy
      && a.energy_short        == b.energy_short
      && a.flags_low_priority  == b.flags_low_priority
      && a.flags_high_priority == b.flags_high_priority
      && a.board_fail          == b.board_fail
      && a.aggCounter          == b.aggCounter
      && a.flush               == b.flush;
}

//^==================================================== independent aggregate walker
//
// Deliberately a second implementation of the framing only -- it knows nothing about
// events. If it and RawDecoder disagree about where aggregates start and end, one of
// them is wrong, and the differential tests below say which.

struct AggInfo { size_t wordOffset; uint32_t nWords; uint32_t aggCounter; bool isSpecial; };

static std::vector<AggInfo> WalkAggregates(const uint8_t * data, size_t dataSize){
  std::vector<AggInfo> out;
  size_t n = dataSize / 8;
  size_t pos = 0;
  while( pos < n ){
    uint64_t w;
    memcpy(&w, data + pos*8, 8);
    w = __builtin_bswap64(w);
    uint8_t fmt = (w >> 60) & 0xF;
    if( fmt == 0x2 ){
      uint32_t nw = w & 0xFFFFFFFF;
      if( nw == 0 || pos + nw > n ) break;
      out.push_back({pos, nw, (uint32_t)((w >> 32) & 0x00FFFFFF), false});
      pos += nw;
    }else if( fmt == 0x3 ){
      uint32_t nw = w & 0xFFFFFFFF;
      if( nw == 0 || pos + nw > n ) break;
      out.push_back({pos, nw, 0, true});
      pos += nw;
    }else{
      pos++;
    }
  }
  return out;
}

//^==================================================== statistics helper

struct Stat {
  size_t n = 0; double sum = 0, sum2 = 0;
  double lo = 1e18, hi = -1e18;
  void Add(double x){ n++; sum += x; sum2 += x*x; lo = std::min(lo,x); hi = std::max(hi,x); }
  double Mean()  const { return n ? sum/n : 0; }
  double Sigma() const { if(n<2) return 0; double m=Mean(); double v=sum2/n-m*m; return v>0?sqrt(v):0; }
};

//^####################################################
//   Check 1 : raw decode vs the firmware's own dpppha decode
//####################################################
static bool Check1(const std::vector<RefHit>& ref,
                   const std::vector<RawDecoder::DecodedHit>& raw,
                   int nCh){
  printf("\n============================================\n");
  printf("  Check 1: raw decode vs dpppha reference\n");
  printf("============================================\n");
  bool ok = true;

  if( ref.empty() || raw.empty() ){
    printf("  [FAIL] ref=%zu hits, raw=%zu hits -- need both.\n", ref.size(), raw.size());
    return false;
  }

  std::map<int,Stat> refE, rawE;
  std::map<int,size_t> refN, rawN;
  for( auto &h : ref ){ refE[h.ch].Add(h.energy); refN[h.ch]++; }
  for( auto &h : raw ){
    if( h.channel >= nCh ){
      printf("  [FAIL] decoded channel %u >= nChannels %d\n", h.channel, nCh);
      ok = false;
      continue;
    }
    rawE[h.channel].Add(h.energy); rawN[h.channel]++;
  }

  printf("  %-5s | %-28s | %-28s\n", "ch", "dpppha (n, mean+-sigma)", "raw (n, mean+-sigma)");
  printf("  ------+------------------------------+------------------------------\n");

  std::vector<int> chans;
  for( auto &kv : refN ) chans.push_back(kv.first);
  for( auto &kv : rawN ) if( std::find(chans.begin(),chans.end(),kv.first)==chans.end() ) chans.push_back(kv.first);
  std::sort(chans.begin(), chans.end());

  for( int ch : chans ){
    printf("  %-5d | %7zu  %8.1f +- %-8.1f | %7zu  %8.1f +- %-8.1f\n", ch,
           refN[ch], refE[ch].Mean(), refE[ch].Sigma(),
           rawN[ch], rawE[ch].Mean(), rawE[ch].Sigma());

    if( refN[ch] == 0 || rawN[ch] == 0 ){
      printf("         -> [FAIL] channel present in one decode but not the other\n");
      ok = false;
      continue;
    }
    // Energy mean must agree. The two runs are separate acquisitions of the same
    // stimulus, so counts differ by livetime -- means must not.
    double dm = fabs(refE[ch].Mean() - rawE[ch].Mean());
    double tol = std::max(5.0, 0.02 * refE[ch].Mean());
    if( dm > tol ){
      printf("         -> [FAIL] energy mean differs by %.1f (tol %.1f)\n", dm, tol);
      ok = false;
    }
    // Fixed-amplitude square wave -> the peak must be narrow.
    double frac = refE[ch].Mean() > 0 ? rawE[ch].Sigma()/rawE[ch].Mean() : 1.0;
    if( frac > 0.10 ){
      printf("         -> [WARN] raw energy peak is broad: sigma/mean = %.3f\n", frac);
    }
  }

  printf("  %s\n", ok ? "[PASS] raw and dpppha agree" : "[FAIL] see above");
  return ok;
}

//^####################################################
//   Check 2 : timestamps against the known pulse period
//####################################################
static bool Check2(const std::vector<RawDecoder::DecodedHit>& raw,
                   uint64_t periodTicks){
  printf("\n============================================\n");
  printf("  Check 2: timestamps (period = %lu ticks = %lu ns)\n",
         periodTicks, periodTicks * gTick2ns);
  printf("============================================\n");
  bool ok = true;

  std::map<int, std::vector<uint64_t>> perCh;
  for( auto &h : raw ) perCh[h.channel].push_back(h.timestamp);

  if( perCh.empty() ){ printf("  [FAIL] no hits\n"); return false; }

  for( auto &kv : perCh ){
    int ch = kv.first;
    auto &ts = kv.second;
    if( ts.size() < 3 ){ printf("  ch%02d: only %zu hits, skipped\n", ch, ts.size()); continue; }

    // --- monotonicity (hits arrive in time order within a channel)
    size_t nonMono = 0;
    for( size_t i = 1; i < ts.size(); i++ ) if( ts[i] <= ts[i-1] ) nonMono++;

    // --- period: every delta should be an exact multiple of periodTicks
    size_t nExact = 0, nOff = 0;
    std::map<uint64_t,size_t> deltaHist;
    std::map<uint64_t,size_t> remHist;
    for( size_t i = 1; i < ts.size(); i++ ){
      if( ts[i] <= ts[i-1] ) continue;
      uint64_t d = ts[i] - ts[i-1];
      deltaHist[d]++;
      uint64_t rem = d % periodTicks;
      if( rem == 0 ) nExact++; else { nOff++; remHist[rem]++; }
    }

    uint64_t domDelta = 0; size_t domN = 0;
    for( auto &d : deltaHist ) if( d.second > domN ){ domN = d.second; domDelta = d.first; }

    printf("  ch%02d: %zu hits, non-monotonic %zu, dominant delta %lu ticks (%zu/%zu), exact-multiple %zu, off %zu\n",
           ch, ts.size(), nonMono, domDelta, domN, deltaHist.size() ? ts.size()-1 : 0, nExact, nOff);

    if( nonMono > 0 ){
      printf("        -> [FAIL] timestamps not monotonic\n");
      ok = false;
    }
    if( domDelta != periodTicks ){
      printf("        -> [FAIL] dominant delta %lu != expected period %lu\n", domDelta, periodTicks);
      ok = false;
    }
    if( nOff > 0 ){
      printf("        -> [FAIL] %zu delta(s) are not multiples of the period. Top remainders:\n", nOff);
      int shown = 0;
      for( auto &r : remHist ){
        printf("             remainder %lu ticks x%zu\n", r.first, r.second);
        if( ++shown >= 5 ) break;
      }
      ok = false;
    }
  }

  // --- cross-channel: all enabled channels fire off the same global trigger,
  //     so pairwise offsets must be constant for the whole run.
  if( perCh.size() >= 2 ){
    auto it = perCh.begin();
    int chA = it->first; auto &tsA = it->second;
    ++it;
    for( ; it != perCh.end(); ++it ){
      int chB = it->first; auto &tsB = it->second;
      size_t n = std::min(tsA.size(), tsB.size());
      if( n < 3 ) continue;
      // Pair by index after aligning on the first common pulse.
      std::map<int64_t,size_t> offHist;
      for( size_t i = 0; i < n; i++ ) offHist[(int64_t)tsB[i] - (int64_t)tsA[i]]++;
      int64_t domOff = 0; size_t domN = 0;
      for( auto &o : offHist ) if( o.second > domN ){ domN = o.second; domOff = o.first; }
      double frac = (double)domN / n;
      printf("  ch%02d-ch%02d offset: %ld ticks constant for %.1f%% of %zu pairs\n",
             chB, chA, domOff, 100*frac, n);
      if( frac < 0.95 ){
        printf("        -> [FAIL] offset not constant -- timestamps or pairing are wrong\n");
        ok = false;
      }
    }
  }

  printf("  %s\n", ok ? "[PASS] timestamps consistent with the test pulse" : "[FAIL] see above");
  return ok;
}

//^####################################################
//   Check 3 : decoder accounting and differential framing tests
//####################################################
static bool Check3(const std::vector<std::vector<uint8_t>>& blobs,
                   const std::string& dppType,
                   const std::vector<RawDecoder::DecodedHit>& full,
                   const std::vector<RawDecoder::StatUpdate>& stats,
                   int nCh){
  printf("\n============================================\n");
  printf("  Check 3: decoder accounting\n");
  printf("============================================\n");
  bool ok = true;

  //--- aggCounter continuity -------------------------------------------------
  {
    bool first = true; uint32_t prev = 0; size_t nJump = 0, nSeen = 0;
    for( auto &h : full ){
      if( first ){ prev = h.aggCounter; first = false; nSeen = 1; continue; }
      if( h.aggCounter == prev ) continue;
      nSeen++;
      uint32_t expect = (prev + 1) & 0x00FFFFFF;
      if( h.aggCounter != expect ) nJump++;
      prev = h.aggCounter;
    }
    printf("  aggCounter: %zu distinct values, %zu discontinuity(ies)\n", nSeen, nJump);
    if( nJump > 0 ){
      printf("        -> [WARN] gaps mean aggregates were dropped between ReadData calls,\n");
      printf("                  which is a readout-rate symptom, not necessarily a decoder bug.\n");
    }
  }

  //--- differential test A: truncate at every aggregate boundary --------------
  // One ReadData call returns one aggregate, so a single blob has nothing to cut.
  // Concatenating blobs builds the multi-aggregate buffer that ParseBlob's outer
  // loop is actually written for -- without this the test passes vacuously.
  {
    size_t nTested = 0, nBad = 0, nCombos = 0;
    const size_t kPerCombo = 8;
    for( size_t start = 0; start + kPerCombo <= blobs.size() && nCombos < 10; start += kPerCombo ){
      std::vector<uint8_t> combo;
      for( size_t i = start; i < start + kPerCombo; i++ )
        combo.insert(combo.end(), blobs[i].begin(), blobs[i].end());

      auto aggs = WalkAggregates(combo.data(), combo.size());
      if( aggs.size() < 2 ) continue;
      nCombos++;

      std::vector<RawDecoder::DecodedHit> whole;
      DecodeAll({combo}, dppType, false, whole);

      for( size_t k = 1; k < aggs.size(); k++ ){
        size_t cutBytes = aggs[k].wordOffset * 8;
        std::vector<RawDecoder::DecodedHit> prefix;
        DecodeAll({std::vector<uint8_t>(combo.begin(), combo.begin()+cutBytes)}, dppType, false, prefix);
        nTested++;
        if( prefix.size() > whole.size() ){ nBad++; continue; }
        bool same = true;
        for( size_t i = 0; i < prefix.size(); i++ ) if( !SameCore(prefix[i], whole[i]) ){ same = false; break; }
        if( !same ) nBad++;
      }

      // Also cut one word short of a boundary: lands mid-aggregate and must be
      // absorbed by the nAggWords > remaining guard rather than over-reading.
      if( aggs.size() >= 2 ){
        size_t cutBytes = (aggs[1].wordOffset - 1) * 8;
        std::vector<RawDecoder::DecodedHit> partial;
        DecodeAll({std::vector<uint8_t>(combo.begin(), combo.begin()+cutBytes)}, dppType, false, partial);
        nTested++;
        if( partial.size() > whole.size() ) nBad++;
      }
    }
    printf("  prefix truncation: %zu multi-aggregate buffer(s), %zu cut point(s) tested, %zu mismatch(es)\n",
           nCombos, nTested, nBad);
    if( nTested == 0 ){
      printf("        -> [FAIL] no cut points -- the truncation test did not actually run\n");
      ok = false;
    }
    if( nBad > 0 ){
      printf("        -> [FAIL] a truncated blob does not decode to a prefix of the whole\n");
      ok = false;
    }
  }

  //--- differential test B: each aggregate in isolation -----------------------
  {
    size_t nTested = 0, nBad = 0;
    for( size_t bi = 0; bi < blobs.size() && bi < 20; bi++ ){
      auto &b = blobs[bi];
      auto aggs = WalkAggregates(b.data(), b.size());
      if( aggs.empty() ) continue;

      std::vector<RawDecoder::DecodedHit> whole, pieced;
      DecodeAll({b}, dppType, false, whole);
      for( auto &a : aggs ){
        std::vector<uint8_t> one(b.begin() + a.wordOffset*8,
                                 b.begin() + (a.wordOffset + a.nWords)*8);
        DecodeAll({one}, dppType, false, pieced);
      }
      nTested++;
      if( pieced.size() != whole.size() ){ nBad++; continue; }
      for( size_t i = 0; i < whole.size(); i++ ) if( !SameCore(whole[i], pieced[i]) ){ nBad++; break; }
    }
    printf("  aggregate isolation: %zu blob(s) tested, %zu mismatch(es)\n", nTested, nBad);
    if( nBad > 0 ){
      printf("        -> [FAIL] sum of per-aggregate decodes != whole-blob decode\n");
      ok = false;
    }
  }

  //--- stat events ------------------------------------------------------------
  {
    printf("  stat events: %zu\n", stats.size());
    if( stats.empty() ){
      printf("        -> [FAIL] EnStatEvents=True but no stat updates were decoded\n");
      ok = false;
    }else{
      std::map<int,uint32_t> lastSaved, lastTrig;
      std::map<int,size_t> nBackwards;
      for( auto &s : stats ){
        if( s.channel >= nCh ){
          printf("        -> [FAIL] stat event channel %u >= nChannels %d\n", s.channel, nCh);
          ok = false;
          continue;
        }
        if( lastSaved.count(s.channel) && s.savedEventCount < lastSaved[s.channel] ) nBackwards[s.channel]++;
        lastSaved[s.channel] = s.savedEventCount;
        lastTrig[s.channel]  = s.triggerCount;
      }
      std::map<int,size_t> decoded;
      for( auto &h : full ) decoded[h.channel]++;
      size_t nIdle = 0;
      for( auto &kv : lastSaved ){
        int ch = kv.first;
        if( kv.second == 0 && lastTrig[ch] == 0 && decoded[ch] == 0 ){ nIdle++; continue; }
        printf("    ch%02d: final savedEventCount=%u triggerCount=%u, decoded hits=%zu, counter went backwards %zu time(s)\n",
               ch, kv.second, lastTrig[ch], decoded[ch], nBackwards[ch]);
        if( nBackwards[ch] > 0 ){
          printf("        -> [FAIL] savedEventCount is cumulative and must never decrease\n");
          ok = false;
        }
        // Stat events are asynchronous snapshots, so exact equality is not expected.
        // But the final cumulative count must be at least the number of hits we decoded.
        if( decoded[ch] > 0 && kv.second + 0 < decoded[ch] ){
          printf("        -> [WARN] final savedEventCount %u < decoded hits %zu\n", kv.second, decoded[ch]);
        }
      }
      if( nIdle ) printf("    (%zu idle channel(s) with all-zero counters not listed)\n", nIdle);
    }
  }

  printf("  %s\n", ok ? "[PASS] accounting consistent" : "[FAIL] see above");
  return ok;
}

//^####################################################
//   Check 4 : the waveform branch (never executed before this test)
//####################################################
static bool Check4(const std::vector<std::vector<uint8_t>>& blobs,
                   const std::string& dppType,
                   int wfChannel, size_t expectSamples){
  printf("\n============================================\n");
  printf("  Check 4: waveform branch (decodeWaveform = true)\n");
  printf("============================================\n");
  bool ok = true;

  std::vector<RawDecoder::DecodedHit> noWf, withWf;
  DecodeAll(blobs, dppType, false, noWf);
  DecodeAll(blobs, dppType, true,  withWf);

  //--- skip-path equivalence --------------------------------------------------
  // The skip branch and the decode branch must advance `pos` identically, or every
  // hit after the first waveform differs between the two modes.
  printf("  decoded %zu hits without waveform, %zu with\n", noWf.size(), withWf.size());
  if( noWf.size() != withWf.size() ){
    printf("        -> [FAIL] hit counts differ: the skip and decode paths disagree on word count\n");
    ok = false;
  }else{
    size_t nDiff = 0, firstDiff = 0;
    for( size_t i = 0; i < noWf.size(); i++ ){
      if( !SameCore(noWf[i], withWf[i]) ){ if(!nDiff) firstDiff = i; nDiff++; }
    }
    printf("  skip/decode equivalence: %zu of %zu hits differ in a non-waveform field\n", nDiff, noWf.size());
    if( nDiff > 0 ){
      printf("        -> [FAIL] first divergence at hit %zu: ch %u vs %u, ts %lu vs %lu, E %u vs %u\n",
             firstDiff, noWf[firstDiff].channel, withWf[firstDiff].channel,
             noWf[firstDiff].timestamp, withWf[firstDiff].timestamp,
             noWf[firstDiff].energy, withWf[firstDiff].energy);
      ok = false;
    }
  }

  //--- waveform content -------------------------------------------------------
  size_t nWave = 0, nLenBad = 0;
  size_t nStepFound = 0, nStepMissing = 0;
  Stat edgePos, amplitude;
  const RawDecoder::DecodedHit * sample = nullptr;

  for( auto &h : withWf ){
    if( !h.hasWaveform ) continue;
    nWave++;
    if( !sample ) sample = &h;

    if( h.traceLenght != h.analog_probes_0.size() ||
        h.traceLenght != h.digital_probes_3.size() ){
      printf("        -> [FAIL] traceLenght %zu but AP0 has %zu and DP3 has %zu samples\n",
             h.traceLenght, h.analog_probes_0.size(), h.digital_probes_3.size());
      ok = false;
      break;
    }
    if( expectSamples && h.channel == wfChannel && h.traceLenght != expectSamples ) nLenBad++;

    for( int i = 0; i < 2; i++ ) if( h.analog_probes_type[i] > 7 ){
      printf("        -> [FAIL] analog probe %d type %u out of range\n", i, h.analog_probes_type[i]);
      ok = false;
    }
    for( int i = 0; i < 4; i++ ) if( h.digital_probes_type[i] > 15 ){
      printf("        -> [FAIL] digital probe %d type %u out of range\n", i, h.digital_probes_type[i]);
      ok = false;
    }

    // The source is a square wave, so AP0 must show one step: flat, edge, flat.
    // This is what proves the probe unpacking is right rather than merely not crashing.
    if( h.traceLenght >= 8 ){
      int32_t lo = h.analog_probes_0[0], hi = h.analog_probes_0[0];
      for( auto v : h.analog_probes_0 ){ lo = std::min(lo,v); hi = std::max(hi,v); }
      int32_t amp = hi - lo;
      amplitude.Add(amp);
      if( amp < 50 ){ nStepMissing++; continue; }
      int32_t mid = lo + amp/2;
      int nCross = 0; size_t where = 0;
      bool above = h.analog_probes_0[0] > mid;
      for( size_t i = 1; i < h.traceLenght; i++ ){
        bool a = h.analog_probes_0[i] > mid;
        if( a != above ){ nCross++; if(nCross==1) where = i; above = a; }
      }
      if( nCross >= 1 ){ nStepFound++; edgePos.Add(where); }
      else nStepMissing++;
    }
  }

  printf("  hits with a waveform: %zu\n", nWave);
  if( nWave == 0 ){
    printf("        -> [FAIL] no waveforms decoded -- WaveSaving/WaveDataSource not in effect?\n");
    return false;
  }
  if( expectSamples ){
    printf("  ch%02d trace length: expected %zu samples, wrong on %zu hit(s)\n",
           wfChannel, expectSamples, nLenBad);
    if( nLenBad > 0 ){
      printf("        -> [FAIL] traceLenght does not match ChRecordLengthT/tick2ns\n");
      ok = false;
    }
  }
  printf("  square-wave step: found on %zu, absent on %zu; amplitude %.0f +- %.0f ADC; edge at sample %.1f +- %.1f\n",
         nStepFound, nStepMissing, amplitude.Mean(), amplitude.Sigma(), edgePos.Mean(), edgePos.Sigma());
  if( nStepFound == 0 ){
    printf("        -> [FAIL] no step in any analog probe: the probe unpacking is not producing the square wave\n");
    ok = false;
  }else if( nStepMissing > nStepFound ){
    printf("        -> [WARN] most traces show no step\n");
  }

  if( sample ){
    printf("  first waveform: ch%02d, %zu samples, downSampling=%u, thr=%u, AP types {%u,%u}, DP types {%u,%u,%u,%u}\n",
           sample->channel, sample->traceLenght, sample->downSampling, sample->trigger_threashold,
           sample->analog_probes_type[0], sample->analog_probes_type[1],
           sample->digital_probes_type[0], sample->digital_probes_type[1],
           sample->digital_probes_type[2], sample->digital_probes_type[3]);
    // Print the whole trace when it is short -- the step is the evidence, and it
    // sits past the pre-trigger, so a truncated dump shows nothing but baseline.
    size_t nShow = std::min(sample->traceLenght, (size_t)40);
    printf("    %zu of %zu samples (AP0 AP1 | DP0 DP1 DP2 DP3):\n", nShow, sample->traceLenght);
    for( size_t i = 0; i < nShow; i++ ){
      printf("      [%2zu] %7d %7d |  %u %u %u %u\n", i,
             sample->analog_probes_0[i], sample->analog_probes_1[i],
             sample->digital_probes_0[i], sample->digital_probes_1[i],
             sample->digital_probes_2[i], sample->digital_probes_3[i]);
    }
  }

  printf("  %s\n", ok ? "[PASS] waveform branch decodes correctly" : "[FAIL] see above");
  return ok;
}

//^####################################################
//   Check 5 : .raw round-trip
//####################################################
static uint64_t Fnv1a(const uint8_t * p, size_t n){
  uint64_t h = 1469598103934665603ULL;
  for( size_t i = 0; i < n; i++ ){ h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}

static bool Check5(Digitizer2Gen& digi, const std::string& dppType){
  printf("\n============================================\n");
  printf("  Check 5: .raw round-trip\n");
  printf("============================================\n");
  bool ok = true;

  const char * base = "/tmp/testRawDecoder_rt";
  char fname[256];
  snprintf(fname, sizeof(fname), "%s_%03d.raw", base, 0);
  chmod(fname, S_IWUSR | S_IRUSR);
  remove(fname);

  std::vector<std::vector<uint8_t>> mem;

  digi.SetDataFormat(DataFormat::Raw);
  digi.StartACQ();
  digi.OpenOutFile(base);
  double t0 = Now();
  while( Now() - t0 < 5.0 && mem.size() < 200 && !gStop ){
    if( digi.ReadData() == CAEN_FELib_Success && digi.hit->dataSize > 0 ){
      mem.emplace_back(digi.hit->data, digi.hit->data + digi.hit->dataSize);
      digi.SaveDataToFile();
    }else{
      usleep(1000);
    }
  }
  digi.CloseOutFile();
  digi.StopACQ();
  printf("  wrote %zu blob(s) to %s\n", mem.size(), fname);

  if( mem.empty() ){ printf("  [FAIL] nothing captured\n"); return false; }

  //--- read back, hash the BYTES first: this separates an I/O bug from a decoder bug
  std::vector<std::vector<uint8_t>> disk;
  FILE * f = fopen(fname, "rb");
  if( !f ){ printf("  [FAIL] cannot reopen %s\n", fname); return false; }

  unsigned short ident;
  size_t blobSize;
  std::vector<uint8_t> buf(20*1024*1024);
  while( fread(&ident, 2, 1, f) == 1 ){
    if( (ident & 0xAA00) != 0xAA00 ) break;
    if( fread(&blobSize, 8, 1, f) != 1 ) break;
    if( blobSize > buf.size() ) break;
    if( fread(buf.data(), blobSize, 1, f) != 1 ) break;
    disk.emplace_back(buf.begin(), buf.begin() + blobSize);
  }
  fclose(f);

  printf("  read back %zu blob(s)\n", disk.size());
  if( disk.size() != mem.size() ){
    printf("  [FAIL] blob count differs\n");
    ok = false;
  }

  size_t nHashBad = 0;
  for( size_t i = 0; i < std::min(mem.size(), disk.size()); i++ ){
    if( mem[i].size() != disk[i].size() ||
        Fnv1a(mem[i].data(), mem[i].size()) != Fnv1a(disk[i].data(), disk[i].size()) ) nHashBad++;
  }
  printf("  byte hash: %zu blob(s) differ\n", nHashBad);
  if( nHashBad > 0 ){
    printf("        -> [FAIL] the file does not contain the bytes we captured (I/O bug, not a decoder bug)\n");
    ok = false;
  }

  std::vector<RawDecoder::DecodedHit> hMem, hDisk;
  DecodeAll(mem,  dppType, false, hMem);
  DecodeAll(disk, dppType, false, hDisk);
  printf("  decode: %zu hits from memory, %zu from file\n", hMem.size(), hDisk.size());
  if( hMem.size() != hDisk.size() ){
    printf("        -> [FAIL] hit counts differ\n");
    ok = false;
  }else{
    size_t nDiff = 0;
    for( size_t i = 0; i < hMem.size(); i++ ) if( !SameCore(hMem[i], hDisk[i]) ) nDiff++;
    if( nDiff ){ printf("        -> [FAIL] %zu hit(s) decode differently from file\n", nDiff); ok = false; }
  }

  chmod(fname, S_IWUSR | S_IRUSR);
  remove(fname);

  printf("  %s\n", ok ? "[PASS] round-trip is byte- and decode-exact" : "[FAIL] see above");
  return ok;
}

//^####################################################
//   main
//####################################################
int main(int argc, char* argv[]){

  const char * url = (argc > 1) ? argv[1] : "dig2://192.168.0.100/";

  printf("############################################\n");
  printf("  testRawDecoder -- RawDecoder.h vs hardware\n");
  printf("  url : %s\n", url);
  printf("  This tool does NOT call ProgramBoard/ProgramChannels.\n");
  printf("  It saves, writes, and restores only the registers it needs.\n");
  printf("############################################\n\n");

  signal(SIGINT,  SigHandler);
  signal(SIGTERM, SigHandler);
  /// Without this, piping the output into head/less kills us on the first write
  /// after the reader exits -- mid-run, with the board still in test-pulse config
  /// and the restore never executed. Learned the hard way.
  signal(SIGPIPE, SIG_IGN);

  Digitizer2Gen digi;
  if( digi.OpenDigitizer(url) < 0 || !digi.IsConnected() ){
    printf("Cannot connect to %s\n", url);
    return 2;
  }
  gHandle  = digi.GetHandle();
  gTick2ns = digi.GetTick2ns();
  int nCh  = digi.GetNChannels();
  std::string dppType = digi.GetFPGAType();

  printf("Connected: %s, SN %u, %s, %d channels, tick = %u ns\n\n",
         digi.GetModelName().c_str(), digi.GetSerialNumber(),
         dppType.c_str(), nCh, gTick2ns);

  const int  kChLo = 0, kChHi = 3;          // only these channels are touched
  const int  kWfCh = 0;                     // only this one records a waveform
  const long kPulsePeriodNs = 1000000;      // 1 kHz
  const long kRecordLenNs   = 256;
  const long kPreTrigNs     = 64;

  if( kChHi >= nCh ){ printf("Board has only %d channels.\n", nCh); return 2; }

  BoardStateGuard guard;

  //^==================== Phase A : save ====================
  printf("--------------------------------------------\n");
  printf("  Phase A: saving original board state\n");
  printf("--------------------------------------------\n");

  const char * boardRegs[] = { "GlobalTriggerSource", "TestPulsePeriod", "TestPulseWidth",
                               "TestPulseLowLevel", "TestPulseHighLevel",
                               "EnStatEvents", "EnDataReduction", "StartSource" };
  const char * chRegs[]    = { "ChEnable", "EventTriggerSource", "WaveTriggerSource", "WaveDataSource" };
  const char * wfChRegs[]  = { "WaveSaving", "ChPreTriggerT", "ChRecordLengthT" };

  bool saveOk = true;
  for( auto r : boardRegs ) saveOk &= guard.Save(DigPath(r));
  for( int ch = kChLo; ch <= kChHi; ch++ )
    for( auto r : chRegs ) saveOk &= guard.Save(ChPath(ch, r));
  for( auto r : wfChRegs ) saveOk &= guard.Save(ChPath(kWfCh, r));

  if( !saveOk ){
    printf("\n*** Could not read the original state. NOTHING HAS BEEN WRITTEN.\n");
    printf("*** The board is untouched. Aborting.\n");
    digi.CloseDigitizer();
    return 2;
  }
  printf("  Saved all registers cleanly (double-read, no volatiles).\n");

  /// Deliberately the current directory, not /tmp: a sandboxed or private /tmp
  /// silently swallows the one file that makes a hard kill recoverable.
  char rescue[128];
  snprintf(rescue, sizeof(rescue), "./rawdecoder_rescue_%d.txt", (int)getpid());
  guard.DumpRescueFile(rescue);

  //^==================== Phase B : apply ====================
  printf("\n--------------------------------------------\n");
  printf("  Phase B: applying test-pulse configuration\n");
  printf("--------------------------------------------\n");

  bool applyOk = true;
  // GlobalTriggerSource=TestPulse makes the pulse FIRE THE TRIGGER.
  applyOk &= guard.Apply(DigPath("GlobalTriggerSource"), "SwTrg | TestPulse");
  applyOk &= guard.Apply(DigPath("TestPulsePeriod"),     std::to_string(kPulsePeriodNs));
  applyOk &= guard.Apply(DigPath("TestPulseWidth"),      "1000");
  applyOk &= guard.Apply(DigPath("TestPulseLowLevel"),   "0");
  applyOk &= guard.Apply(DigPath("TestPulseHighLevel"),  "10000");
  applyOk &= guard.Apply(DigPath("EnStatEvents"),        "True");
  applyOk &= guard.Apply(DigPath("EnDataReduction"),     "False");
  applyOk &= guard.Apply(DigPath("StartSource"),         "SWcmd");

  for( int ch = kChLo; ch <= kChHi; ch++ ){
    applyOk &= guard.Apply(ChPath(ch, "ChEnable"),           "True");
    applyOk &= guard.Apply(ChPath(ch, "EventTriggerSource"), "GlobalTriggerSource");
    applyOk &= guard.Apply(ChPath(ch, "WaveTriggerSource"),  "GlobalTriggerSource");
    // ...and WaveDataSource=SquareWave is what makes the channel DIGITIZE it.
    // Without this the channel triggers on schedule but records noise.
    applyOk &= guard.Apply(ChPath(ch, "WaveDataSource"),     "SquareWave");
  }
  applyOk &= guard.Apply(ChPath(kWfCh, "WaveSaving"),      "Always");
  applyOk &= guard.Apply(ChPath(kWfCh, "ChRecordLengthT"), std::to_string(kRecordLenNs));
  applyOk &= guard.Apply(ChPath(kWfCh, "ChPreTriggerT"),   std::to_string(kPreTrigNs));

  if( !applyOk ){
    printf("\n*** Configuration failed. Restoring and aborting.\n");
    bool r = guard.Restore();
    digi.CloseDigitizer();
    return r ? 1 : 3;
  }

  // Report what the board actually accepted -- the requested value is not always
  // the granted one, and every later expectation is derived from the readback.
  std::string rbPeriod, rbRecLen, rbPreTrig, rbGts, rbWds;
  FeGet(DigPath("TestPulsePeriod"), rbPeriod);
  FeGet(ChPath(kWfCh,"ChRecordLengthT"), rbRecLen);
  FeGet(ChPath(kWfCh,"ChPreTriggerT"), rbPreTrig);
  FeGet(DigPath("GlobalTriggerSource"), rbGts);
  FeGet(ChPath(kWfCh,"WaveDataSource"), rbWds);
  printf("  readback: GlobalTriggerSource='%s' TestPulsePeriod=%s ns  ch%d WaveDataSource='%s' RecLen=%s ns PreTrig=%s ns\n",
         rbGts.c_str(), rbPeriod.c_str(), kWfCh, rbWds.c_str(), rbRecLen.c_str(), rbPreTrig.c_str());

  long periodNs = atol(rbPeriod.c_str());
  if( periodNs <= 0 ) periodNs = kPulsePeriodNs;
  uint64_t periodTicks = (uint64_t)(periodNs / gTick2ns);
  size_t expectSamples = (size_t)(atol(rbRecLen.c_str()) / gTick2ns);
  printf("  derived : period = %lu ticks, ch%d trace = %zu samples\n\n",
         periodTicks, kWfCh, expectSamples);

  //^==================== acquisition ====================
  bool pass1=false, pass2=false, pass3=false, pass4=false, pass5=false;

  printf("--------------------------------------------\n");
  printf("  Acquiring reference (dpppha, MiniWithFineTime)\n");
  printf("--------------------------------------------\n");
  std::vector<RefHit> ref;
  digi.SetDataFormat(DataFormat::MiniWithFineTime);
  CollectRef(digi, 10.0, 200000, ref);
  printf("  %zu reference hits\n", ref.size());

  printf("\n--------------------------------------------\n");
  printf("  Acquiring raw blobs (/endpoint/raw)\n");
  printf("--------------------------------------------\n");
  std::vector<std::vector<uint8_t>> blobs;
  digi.SetDataFormat(DataFormat::Raw);
  CollectRaw(digi, 10.0, 64UL*1024*1024, blobs);
  size_t totalBytes = 0;
  for( auto &b : blobs ) totalBytes += b.size();
  printf("  %zu blob(s), %.2f MB\n", blobs.size(), totalBytes/1024.0/1024.0);

  if( gStop ) printf("\n  (interrupted by signal -- running the checks on what we have)\n");

  std::vector<RawDecoder::DecodedHit> rawHits;
  std::vector<RawDecoder::StatUpdate> rawStats;
  DecodeAll(blobs, dppType, false, rawHits, &rawStats);
  printf("  decoded %zu hit(s), %zu stat update(s)\n", rawHits.size(), rawStats.size());

  //^==================== checks ====================
  pass1 = Check1(ref, rawHits, nCh);
  pass2 = Check2(rawHits, periodTicks);
  pass3 = Check3(blobs, dppType, rawHits, rawStats, nCh);
  pass4 = Check4(blobs, dppType, kWfCh, expectSamples);
  pass5 = gStop ? true : Check5(digi, dppType);
  if( gStop ) printf("\n  Check 5 skipped (interrupted).\n");

  //^==================== Phase C : restore ====================
  bool restored = guard.Restore();

  printf("\n############################################\n");
  printf("  Summary\n");
  printf("############################################\n");
  printf("  Check 1  raw vs dpppha        : %s\n", pass1 ? "PASS" : "FAIL");
  printf("  Check 2  timestamps           : %s\n", pass2 ? "PASS" : "FAIL");
  printf("  Check 3  decoder accounting   : %s\n", pass3 ? "PASS" : "FAIL");
  printf("  Check 4  waveform branch      : %s\n", pass4 ? "PASS" : "FAIL");
  printf("  Check 5  .raw round-trip      : %s\n", pass5 ? "PASS" : "FAIL");
  printf("  Board restored                : %s\n", restored ? "yes" : "NO -- SEE FIX LIST ABOVE");
  printf("############################################\n");

  if( restored ){ remove(rescue); }

  digi.CloseDigitizer();

  if( !restored ) return 3;
  return (pass1 && pass2 && pass3 && pass4 && pass5) ? 0 : 1;
}
