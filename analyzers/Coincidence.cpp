#include "../Analysis.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

/// Coincidence between two channels chosen at runtime.
///
/// Detector-agnostic on purpose: because the two channels are picked from the GUI rather than
/// compiled in, this works on any setup and is a reasonable first thing to look at on real beam.
///
/// It is also the showcase for ABI v2: the analysis owns its layout. The host hands over a root
/// area (WorkArea) and this file arranges everything inside it -- a splitter with the E(A) x E(B)
/// matrix dominating, the channel pickers beside it, and an "Add tB - tA" button that creates a
/// histogram DURING the run through the registry's analysis-thread path. Only the histograms and
/// pickers are host-built widgets, because only they own thread fences and board knowledge.
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

    //^---- the canvas: [ E(A)vsE(B) | Channels / Add-ons ], 3:1
    QSplitter * split = new QSplitter(Qt::Horizontal, root_);
    QVBoxLayout * rl = new QVBoxLayout(root_);
    rl->setContentsMargins(4, 4, 4, 4);
    rl->addWidget(split);

    QSplitter * left  = new QSplitter(Qt::Vertical, split);
    QSplitter * right = new QSplitter(Qt::Vertical, split);
    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 3);          // E(A) vs E(B) dominates; pickers/add-ons beside
    split->setStretchFactor(1, 1);

    if( QWidget * ee = static_cast<QWidget *>( reg.View2(hEE) ) ) left->addWidget(ee);

    QGroupBox * cfg = new QGroupBox("Channels", left);
    QGridLayout * cg = new QGridLayout(cfg);
    cg->setContentsMargins(6, 6, 6, 6);
    if( QWidget * a = static_cast<QWidget *>( pA ) ) cg->addWidget(a, 0, 0);
    if( QWidget * b = static_cast<QWidget *>( pB ) ) cg->addWidget(b, 1, 0);
    cg->setRowStretch(2, 1);
    left->addWidget(cfg);
    left->setStretchFactor(0, 1);           // the matrix grows, the channel box stays compact
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

    if( a == nullptr || b == nullptr ) return;

    reg.Fill2(hEE, a->energy, b->energy);
    /// Signed difference, so cast out of uint64 before subtracting. Fill1(-1) is a no-op, so an
    /// unadded tB-tA costs one compare per coincidence.
    // reg.Fill1(hTDiff, (double)b->timestamp - (double)a->timestamp);
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
