#include "../Analysis.h"

#include <cstdio>

#include <QGridLayout>
#include <QWidget>

/// MUSIC-style bar-array monitor, ported from FSUDAQ's MUSICAnalyzer.
///
/// The physics: strips of a position-sensitive detector, split into two banks (FSUDAQ called them
/// Left/Right halves of a DSSD-like array). Per event each bank gets a channel-vs-energy matrix,
/// the L+R per-strip sum spectrum shows where the energy lands across the whole array, and
/// multiplicity says whether the event builder's window is doing what you think.
///
/// Geometry here is a DEFAULT, not a fossil: even-numbered boards are Left, odd are Right, strip
/// = channel. FSUDAQ hardcoded a per-board channel->strip table (mapping[4][16]) and a gain table
/// per (ch,bd); the gain table lives ON in SaveState/LoadState, so matching constants found at
/// the bench survive reloads without recompiling. The strip map is in MapStrip(), one line.
///
/// It also shows the v4 split: everything the analysis draws lives in the WorkArea; the
/// Analyzer bar (selector, enable, window, guard) is fixed host chrome above it. Nothing in
/// this file touches host plumbing -- the pure-plot layout is the default case.

class MUSIC : public Analysis {

  static const int kStrips = 16;

  //^---- gain matching, [board][channel]; identity by default, persisted as one KV string.
  //^     Only the analysis thread reads it during ProcessEvent; SaveState/LoadState ride the
  //^     parked-thread window like every other member.
  int    nBoards_ = 1;
  double gain_[8][16] = { { 1. } };

  int hLeft = -1, hRight = -1, hSum = -1, hMulti = -1;

  HistRegistry * reg_ = nullptr;
  QWidget      * root_ = nullptr;

public:

  void Declare(HistRegistry & reg, const AnalysisContext & ctx) override {

    reg_ = &reg;
    nBoards_ = ctx.nBoards > 0 ? ctx.nBoards : 1;

    hLeft  = reg.Hist2D("Left",         "strip", "energy", kStrips, 0, kStrips, 200, 0, 30000, nullptr);
    hRight = reg.Hist2D("Right",        "strip", "energy", kStrips, 0, kStrips, 200, 0, 30000, nullptr);
    hSum   = reg.Hist2D("Left + Right", "strip", "energy", kStrips, 0, kStrips, 200, 0, 30000, nullptr);
    hMulti = reg.Hist1D("Multiplicity", "hits / event", 65, 0, 65, nullptr);

    root_ = static_cast<QWidget *>( reg.WorkArea() );
    if( root_ == nullptr ) return;              // headless: declared, nothing to arrange

    //^---- [ L | R ] over [ L+R | Multiplicity ], filling the whole work area
    QGridLayout * plots = new QGridLayout(root_);
    plots->setContentsMargins(4, 4, 4, 4);
    if( QWidget * w = static_cast<QWidget *>( reg.View2(hLeft) ) )  plots->addWidget(w, 0, 0);
    if( QWidget * w = static_cast<QWidget *>( reg.View2(hRight) ) ) plots->addWidget(w, 0, 1);
    if( QWidget * w = static_cast<QWidget *>( reg.View2(hSum) ) )   plots->addWidget(w, 1, 0);
    if( QWidget * w = static_cast<QWidget *>( reg.View1(hMulti) ) ) plots->addWidget(w, 1, 1);

    reg.SetWindowTitle("MUSIC - bar array monitor");
    reg.ResizeWindow(1200, 900);
  }

  /// strip index for a hit; -1 to ignore it. The FSUDAQ per-board mapping table lived here.
  int MapStrip(const BuiltHit & h) const {
    (void)h;
    return h.channel;                       // bench remap goes here
  }
  bool IsLeft(const BuiltHit & h) const { return ( h.digi % 2 ) == 0; }

  void ProcessEvent(const BuiltHit * hits, int n, HistRegistry & reg) override {

    reg.Fill1(hMulti, (double)n);

    /// Per-event strip totals, so the third matrix shows WHERE the whole array's energy of one
    /// event lands -- banks add up in the same strip index, which is the point of the plot.
    double sum[kStrips] = { 0 };

    for( int i = 0; i < n; i++ ){
      const BuiltHit & h = hits[i];
      const int strip = MapStrip(h);
      if( strip < 0 || strip >= kStrips ) continue;

      double e = (double) h.energy;
      const int bd = (int) h.digi < 8 ? (int) h.digi : 7;
      const int ch = h.channel < 16 ? h.channel : 15;
      e *= gain_[bd][ch];

      reg.Fill2( IsLeft(h) ? hLeft : hRight, strip + 0.5, e );
      sum[strip] += e;
    }

    for( int k = 0; k < kStrips; k++ ){
      if( sum[k] > 0 ) reg.Fill2(hSum, k + 0.5, sum[k]);
    }
  }

  void SaveState(SettingsKV & kv) override {
    std::string flat;
    for( int b = 0; b < 8; b++ ){
      for( int c = 0; c < 16; c++ ){
        char buf[32];
        snprintf(buf, sizeof buf, "%s%.5f", (b || c) ? "," : "", gain_[b][c]);
        flat += buf;
      }
    }
    kv.SetStr("gain", flat.c_str());
  }

  void LoadState(SettingsKV & kv) override {
    std::string flat;
    if( !kv.GetStr("gain", flat) ) return;
    size_t pos = 0;
    for( int b = 0; b < 8 && pos != std::string::npos; b++ ){
      for( int c = 0; c < 16; c++ ){
        const size_t next = flat.find(',', pos);
        const std::string tok = flat.substr(pos, next == std::string::npos
                                               ? std::string::npos : next - pos);
        try { gain_[b][c] = std::stod(tok); }
        catch( ... ){ gain_[b][c] = 1.0; }
        if( next == std::string::npos ){ pos = std::string::npos; break; }
        pos = next + 1;
      }
    }
  }

};

ANALYSIS_ENTRY(MUSIC, "MUSIC");
