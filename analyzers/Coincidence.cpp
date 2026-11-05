#include "../Analysis.h"
#include "../ClassInfluxDB.h"
#include "RateScatter.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <chrono>
#include <cstdio>
#include <string>

/// Monotonic seconds for the scatter's x clock; only differences hit the widget.
static double SteadySec(){
  return std::chrono::duration<double>( std::chrono::steady_clock::now().time_since_epoch() ).count();
}

/// Coincidence between two channels chosen at runtime.
///
/// Detector-agnostic on purpose: because the two channels are picked from the GUI rather than
/// compiled in, this works on any setup and is a reasonable first thing to look at on real beam.
///
/// It is also the showcase for ABI v2: the analysis owns its layout. The host hands over a root
/// area (WorkArea) and this file arranges everything inside it -- a splitter with the rate
/// scatter and the channel pickers on the left, the E(A) x E(B) matrix dominating on the right.
/// Only the histograms and pickers are host-built widgets, because only they own thread fences
/// and board knowledge; the scatter (RateScatter.h) is plugin-built from the v6 GateRates ledger.
///
/// The multiplicity and tB-tA plots cost almost nothing and are what actually tell you whether the
/// event builder's time window is set sensibly: dt should sit inside the window, and multiplicity
/// should broaden as the window is opened up.

class Coincidence : public Analysis {

  //^---- set by the host through ChannelParam; only touched on the analysis thread. SaveState/
  //^     LoadState run on the GUI thread while the workers are parked, same as Declare().
  int digiA = 0, chA = 0;
  int digiB = 0, chB = 4;

  int hEE    = -1;
  // int hTDiff = -1;
  double wNS = 5000;

  //^---- GUI side. Null under a headless driver (no work area) -- every use is guarded, so the
  //^     class still runs filled-but-unplotted through the console test driver.
  HistRegistry * reg_ = nullptr;
  QWidget     * root_  = nullptr;
  QVBoxLayout * addLay_ = nullptr;
  QPushButton * bnAdd_  = nullptr;

  //^---- InfluxDB output (ABI v5/v6), analysis thread only. Own client instance: the host's
  //^     InfluxDB lives in the executable and may never be called from here, so ClassInfluxDB.cpp
  //^     is compiled into this .so. One batched POST per rate-average window (host-paced); the
  //^     write is synchronous curl bounded by ClassInfluxDB's timeouts (~4 s worst case) and the
  //^     event ring absorbs the stall -- losing events, never raw hits, is the ring's whole purpose.
  static constexpr double kInfluxPeriodS = 2.0;   ///< write-clock default until the host's window is known
  double      influxBaseS = kInfluxPeriodS;       ///< live averaging window, from GateRates' return
  InfluxDB    influx;
  std::string influxCfgKey;              ///< url\ntoken\ndb last SetURL/SetToken'd
  std::chrono::steady_clock::time_point influxLast{};     ///< end of the last WRITTEN window
  std::chrono::steady_clock::time_point influxNextTry{};  ///< retry clock, once per window at most
  unsigned long long influxN = 0;
  unsigned influxSnA = 0, influxSnB = 0;
  double influxEA = 0, influxEB = 0, influxDT = 0;

public:

  void Declare(HistRegistry & reg, const AnalysisContext & ctx) override {

    reg_ = &reg;

    /// Span the coincidence window both ways, so a window that is too wide or too narrow shows up
    /// as a peak that is lost in a flat background or clipped at the edges.
    wNS = (double)(ctx.timeWindow > 0 ? ctx.timeWindow : 5000);

    /// Declared once; the returned containers are placed below when a work area exists. GUI
    /// thread here, so the widgets are live on return.
    void * pA = reg.ChannelParam("Channel A", &digiA, &chA, nullptr);
    void * pB = reg.ChannelParam("Channel B", &digiB, &chB, nullptr);

    hEE = reg.Hist2D("E(A) vs E(B)", "E(A)", "E(B)", 256, 0, 30000, 256, 0, 30000, nullptr);

    root_ = static_cast<QWidget *>( reg.WorkArea() );
    if( root_ == nullptr ) return;      // headless: declared, nothing to arrange

    //^---- the canvas: [ rate scatter + Channels | E(A) vs E(B) ], 1:2 -- the matrix dominates,
    //^     its rates live on the left, one point cloud per 2D cut grows as cuts are drawn.
    QSplitter * split = new QSplitter(Qt::Horizontal, root_);
    QVBoxLayout * rl = new QVBoxLayout(root_);
    rl->setContentsMargins(4, 4, 4, 4);
    rl->addWidget(split);

    QSplitter * left  = new QSplitter(Qt::Vertical, split);
    QSplitter * right = new QSplitter(Qt::Vertical, split);
    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 2);

    //^---- Rate scatter: {tag, Hz} points straight out of the host's v6 GateRates ledger --
    //^     "all" (the total) always, one further series per live 2D cut the moment the store
    //^     has two samples for it, dimmed and frozen once the cut is deleted. The timer is
    //^     PARENTED to the widget: work-area teardown kills it BEFORE the analysis object dies
    //^     (ABI order), so the lambda can never fire on a half-dead `this` -- the same trick as
    //^     the Add-button connect below. Polling is not visibility-gated; the host only ever
    //^     publishes a snapshot while the run is enabled, so an idle window simply sees empty
    //^     snapshots, which the widget treats as the run boundary.
    RateScatter * scatter = new RateScatter(left);
    scatter->setMinimumHeight(180);
    scatter->setToolTip("Mouse wheel: zoom the time window (5 s .. 1 h)\nDouble-click: back to 10 min");
    QTimer * rt = new QTimer(scatter);
    QObject::connect(rt, &QTimer::timeout, scatter, [this, scatter](){
      std::vector<std::pair<std::string,double>> rows;
      if( reg_ ) reg_->GateRates(hEE, rows);
      scatter->Sample( SteadySec(), rows );
    });
    rt->start(500);
    left->addWidget(scatter);

    if( QWidget * ee = static_cast<QWidget *>( reg.View2(hEE) ) ) right->addWidget(ee);

    QGroupBox * cfg = new QGroupBox("Channels", left);
    QGridLayout * cg = new QGridLayout(cfg);
    cg->setContentsMargins(6, 6, 6, 6);
    if( QWidget * a = static_cast<QWidget *>( pA ) ) cg->addWidget(a, 0, 0);
    if( QWidget * b = static_cast<QWidget *>( pB ) ) cg->addWidget(b, 1, 0);
    cg->setRowStretch(2, 1);
    left->addWidget(cfg);
    left->setStretchFactor(0, 1);           // the scatter grows, the channel box stays compact
    left->setStretchFactor(1, 0);

    // QGroupBox * addBox = new QGroupBox("Add-ons", right);
    // addLay_ = new QVBoxLayout(addBox);
    // bnAdd_  = new QPushButton("Add  tB - tA", addBox);
    // addLay_->addWidget(bnAdd_);
    // addLay_->addStretch(1);
    // right->addWidget(addBox);

    // /// The context object is the work area: the ABI teardown destroys it BEFORE the analysis, so
    // /// this lambda can never fire on a half-dead window.
    // QObject::connect(bnAdd_, &QPushButton::clicked, root_, [this](){ AddTDiff(); });
  }

  /// Runtime declaration -- the point of the exercise. From a GUI callback the widget exists the
  /// moment Hist1D() returns, so the placement is immediate; the same call from ProcessEvent would
  /// return a valid id with the plot appearing at the next publish tick.
  // void AddTDiff(){
  //   if( hTDiff >= 0 || reg_ == nullptr ) return;
  //   hTDiff = reg_->Hist1D("tB - tA", "ns", 200, -wNS, wNS, nullptr);
  //   if( QWidget * w = static_cast<QWidget *>( reg_->View1(hTDiff) ) ){
  //     w->setMinimumHeight(160);
  //     addLay_->insertWidget(addLay_->count() - 1, w);   // above the trailing stretch
  //     addLay_->setStretch(addLay_->count() - 2, 1);
  //   }
  //   if( bnAdd_ ) bnAdd_->setEnabled(false);
  // }

  void ProcessEvent(const BuiltHit * hits, int n, HistRegistry & reg) override {

    const BuiltHit * a = nullptr;
    const BuiltHit * b = nullptr;

    for( int i = 0; i < n; i++ ){
      if( hits[i].digi == digiA && hits[i].channel == chA ) a = &hits[i];
      if( hits[i].digi == digiB && hits[i].channel == chB ) b = &hits[i];
    }

    if( a == nullptr || b == nullptr ){ InfluxClock(reg); return; }

    reg.Fill2(hEE, a->energy, b->energy);
    /// Signed difference, so cast out of uint64 before subtracting. Fill1(-1) is a no-op, so an
    /// unadded tB-tA costs one compare per coincidence.
    // reg.Fill1(hTDiff, (double)b->timestamp - (double)a->timestamp);

    influxN++;
    influxSnA = a->sn; influxSnB = b->sn;
    influxEA += a->energy; influxEB += b->energy;
    influxDT += (double)b->timestamp - (double)a->timestamp;
    InfluxClock(reg);
  }

  //^-------------------------------------------------------------- InfluxDB output (ABI v5/v6)
  //^
  //^ Two measurements per write tick, both tagged by the A/B channel pair:
  //^   Coincidence,...,            count,span,meanEA,meanEB,meanDT   (plugin's exact per-event stats)
  //^   CoincidenceRate,...,Cut=<tag>  rate=Hz                        (host RateStore, adjustable base)
  //^ The Hz is NOT computed here: the host samples the cumulative matrix total and each live
  //^ 2D cut's cell-sum into its RateStore -- the same trailing-window dN/dt behind every
  //^ RateMonitor trace -- and GateRates() hands back the latest snapshot plus the window it
  //^ averages over; the plugin only paces its write clock to that window, so consecutive points
  //^ tile back-to-back. No coincidences while events flow still writes: count=0 and a 0 Hz
  //^ store row are a real flat line; an EMPTY snapshot (fewer than two samples) is not, and no
  //^ rate rows appear until the host has one.

  void InfluxClock(HistRegistry & reg){
    if( !reg.InfluxOut() ){
      /// Manual pause: drop the partial window rather than mix counts from either side of it.
      influxN = 0; influxEA = 0; influxEB = 0; influxDT = 0;
      influxLast = influxNextTry = std::chrono::steady_clock::now();
      return;
    }
    const auto now = std::chrono::steady_clock::now();
    if( now < influxNextTry ) return;           ///< one attempt per rate-average window
    influxNextTry = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>
                              (std::chrono::duration<double>(influxBaseS));
    if( InfluxFlush(reg, now) ) influxLast = now;   ///< data window advances only on an accepted write
  }

  /// Returns true = the point was accepted. A refused flush keeps accumulating; the span=
  /// field tells the reader the window was longer than one base because of a DB gap.
  bool InfluxFlush(HistRegistry & reg, std::chrono::steady_clock::time_point now){
    std::string url, token, db;
    reg.InfluxConfig(url, token, db);
    if( db.empty() ) return false;              ///< no database configured (checkbox beats an unset db)

    const std::string key = url + "\n" + token + "\n" + db;
    if( key != influxCfgKey ){                  ///< first write, or Program Settings changed it
      influxCfgKey = key;
      influx.SetURL(url);
      influx.SetToken(token);
    }
    if( !influx.IsConnectionOK() && !influx.TestingConnection() ) return false;   ///< re-probed once per attempt, bounded

    std::vector<std::pair<std::string,double>> rates;
    const double base = reg.GateRates(hEE, rates);
    if( base > 0 ){
      influxBaseS = base;
      influxNextTry = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>
                                (std::chrono::duration<double>(base));   ///< re-pace to the live setting
    }
    if( influxN == 0 && rates.empty() ) return false;   ///< nothing measured, nothing meaningful to say

    std::chrono::duration<double> span = ( influxLast.time_since_epoch().count() == 0 )
                                       ? std::chrono::duration<double>(influxBaseS)
                                       : (now - influxLast);
    if( span.count() <= 0 ) span = std::chrono::duration<double>(influxBaseS);

    char line[512];
    const double n = (double)influxN;
    std::snprintf(line, sizeof(line),
        "Coincidence,BdA=%u,ChA=%02d,BdB=%u,ChB=%02d "
        "count=%llu,span=%.1f,meanEA=%.1f,meanEB=%.1f,meanDT=%.1f",
        influxSnA, chA, influxSnB, chB,
        influxN, span.count(), n > 0 ? influxEA / n : 0.0,
        n > 0 ? influxEB / n : 0.0, n > 0 ? influxDT / n : 0.0);
    influx.AddDataPoint(line);

    for( size_t i = 0; i < rates.size(); i++ ){
      std::string tag = rates[i].first;
      for( size_t k = 0; k < tag.size(); k++ )
        if( tag[k] == ' ' || tag[k] == ',' || tag[k] == '=' ) tag[k] = '-';   ///< line-protocol tag rules
      std::snprintf(line, sizeof(line),
          "CoincidenceRate,BdA=%u,ChA=%02d,BdB=%u,ChB=%02d,Cut=%s rate=%.3f",
          influxSnA, chA, influxSnB, chB, tag.c_str(), rates[i].second);
      influx.AddDataPoint(line);
    }

    influx.WriteData(db);
    /// Drop the buffer either way, exactly like the scalar writers: no resends, and the buffer
    /// can never grow while the DB is unreachable.
    influx.ClearDataPointsBuffer();

    if( influx.IsWriteOK() ){
      influxN = 0; influxEA = 0; influxEB = 0; influxDT = 0;
      return true;
    }
    influx.TestingConnection();   ///< force a re-probe next attempt (connection may have died mid-write)
    return false;
  }

  void OnRunStart() override {
    influxLast = influxNextTry = std::chrono::steady_clock::now();
    influxN = 0; influxEA = 0; influxEB = 0; influxDT = 0;
  }

  /// Flush the partial batch the run ended on, after AnaWorker's final drain. A zero-count
  /// window at a boundary says nothing new; the store's last published point already stands.
  void OnRunStop() override {
    if( reg_ == nullptr || !reg_->InfluxOut() ) return;
    if( influxN > 0 ) InfluxFlush(*reg_, std::chrono::steady_clock::now());
  }

  void SaveState(SettingsKV & kv) override {
    kv.Set("digiA", digiA); kv.Set("chA", chA);
    kv.Set("digiB", digiB); kv.Set("chB", chB);
    // kv.Set("tDiff", hTDiff >= 0 ? 1.0 : 0.0);
  }

  void LoadState(SettingsKV & kv) override {
    double v = 0;
    if( kv.Get("digiA", &v) ) digiA = (int)v;
    if( kv.Get("chA",   &v) ) chA   = (int)v;
    if( kv.Get("digiB", &v) ) digiB = (int)v;
    if( kv.Get("chB",   &v) ) chB   = (int)v;
    // if( kv.Get("tDiff", &v) && v != 0.0 ) AddTDiff();   ///< restore the plot the user added
  }
};

ANALYSIS_ENTRY(Coincidence, "Coincidence");
