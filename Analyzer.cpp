#include "Analyzer.h"

#include <QCoreApplication>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>

#include <algorithm>
#include <chrono>
#include <cmath>

//^-------------------------------------------------------------- connect() for non-QObjects
//^  QtHistRegistry is deliberately NOT a QObject (HistRegistry's vtable stays the only virtual
//^  base), so the plain connect() calls below resolve to this helper, anchored on the widget that
//^  owns the signal: the receiver-context semantics Qt needs come from that widget's lifetime.
//^  Inside Analyzer (a QObject) name lookup finds QObject::connect first, as always.
template <typename Sender, typename Signal, typename Context, typename Func>
static void connect(Sender * s, Signal sig, Context * ctx, Func && fn){
  QObject::connect(s, sig, ctx, std::forward<Func>(fn));
}

//^============================================================ QtHistRegistry

QtHistRegistry::QtHistRegistry(QWidget * root_, Digitizer2Gen ** digi_, unsigned int nDigi_,
                               const std::vector<int> & boardIndex_, const AnalyzerHooks & hooks_)
  : root(root_), digi(digi_), nDigi(nDigi_), boardIndex(boardIndex_), hooks(hooks_) {}

void * QtHistRegistry::WorkArea(){ return root; }

bool QtHistRegistry::OnGuiThread() const{
  return QCoreApplication::instance()
      && QCoreApplication::instance()->thread() == QThread::currentThread();
}

//^-------------------------------------------------------------- declarations

int QtHistRegistry::Hist1D(const char * name, const char * xLabel,
                           int nBin, double xMin, double xMax, void * placement){

  /// Histogram1D::Rebin silently clamps at 1000 bins; clamp here so the axis and our count array
  /// cannot disagree, and say so rather than mis-binning quietly.
  if( nBin > 1000 ){
    printf("Analyzer: histogram \"%s\" asked for %d bins, clamped to 1000\n", name, nBin);
    nBin = 1000;
  }
  if( nBin < 1 ) nBin = 1;

  std::unique_ptr<H1> h(new H1());
  h->name     = name;
  h->xLabel   = xLabel;
  h->nBin     = nBin;             ///< the declared binning; Fill1 works before the widget exists
  h->lo       = xMin;
  h->hi       = xMax;
  h->dx       = (xMax - xMin) / nBin;
  h->placement = placement;
  h->c.reset(new std::atomic<uint32_t>[nBin]);
  for( int i = 0; i < nBin; i++ ) h->c[i].store(0, std::memory_order_relaxed);

  h1.push_back(std::move(h));
  const size_t i = h1.size() - 1;
  if( OnGuiThread() ) CreateH1(i);
  else { std::lock_guard<std::mutex> g(gate); pendH1.push_back(i); }
  return (int)i;
}

int QtHistRegistry::Hist2D(const char * name, const char * xLabel, const char * yLabel,
                           int nX, double xMin, double xMax,
                           int nY, double yMin, double yMax, void * placement){

  /// Histogram2D stores xBin = requested + 2 guard bins and clamps the total at 1002, so the
  /// usable request is 1000. Same reasoning as above.
  if( nX > 1000 ){
    printf("Analyzer: histogram \"%s\" asked for %d x bins, clamped to 1000\n", name, nX);
    nX = 1000;
  }
  if( nY > 1000 ){
    printf("Analyzer: histogram \"%s\" asked for %d y bins, clamped to 1000\n", name, nY);
    nY = 1000;
  }
  if( nX < 1 ) nX = 1;
  if( nY < 1 ) nY = 1;

  std::unique_ptr<H2> h(new H2());
  h->name   = name;
  h->xLabel = xLabel;
  h->yLabel = yLabel;
  h->nXReq  = nX;
  h->nYReq  = nY;
  h->xLo    = xMin; h->xHi = xMax;
  h->yLo    = yMin; h->yHi = yMax;
  h->placement = placement;

  h2.push_back(std::move(h));
  const size_t i = h2.size() - 1;
  if( OnGuiThread() ) CreateH2(i);
  else { std::lock_guard<std::mutex> g(gate); pendH2.push_back(i); }
  return (int)i;
}

void * QtHistRegistry::ChannelParam(const char * label, int * digiPtr, int * chPtr, void * placement){
  ChanReq rq;
  rq.label    = label;
  rq.digi     = digiPtr;
  rq.ch       = chPtr;
  rq.placement = placement;
  chans.push_back(rq);
  const size_t i = chans.size() - 1;
  if( OnGuiThread() ){
    CreateChan(i);
    if( declareFinished ) WireChan(i);      // seeding reads the members, so it waits for LoadState
  }else{
    std::lock_guard<std::mutex> g(gate);
    pendChan.push_back(i);
  }
  return chans[i].line;                     // null while still queued
}

void * QtHistRegistry::BoolParam(const char * label, bool * value, void * placement){
  FlagReq rq;
  rq.label    = label;
  rq.value    = value;
  rq.placement = placement;
  flags.push_back(rq);
  const size_t i = flags.size() - 1;
  if( OnGuiThread() ){
    CreateFlag(i);
    if( declareFinished ) WireFlag(i);
  }else{
    std::lock_guard<std::mutex> g(gate);
    pendFlag.push_back(i);
  }
  return flags[i].cb;
}

void * QtHistRegistry::StatusControl(){        return hooks.claimStatus ? hooks.claimStatus() : nullptr; }
uint64_t QtHistRegistry::TimeWindow(){         return hooks.timeWindow    ? hooks.timeWindow()    : 0; }
void QtHistRegistry::SetTimeWindow(uint64_t ns){ if( hooks.setTimeWindow ) hooks.setTimeWindow(ns); }
double QtHistRegistry::GuardFactor(){          return hooks.guardFactor   ? hooks.guardFactor()   : 0; }
void QtHistRegistry::SetGuardFactor(double f){ if( hooks.setGuardFactor ) hooks.setGuardFactor(f); }
void QtHistRegistry::SetWindowTitle(const char * t){ if( hooks.setWindowTitle ) hooks.setWindowTitle(QString::fromUtf8(t)); }
void QtHistRegistry::ResizeWindow(int w, int h){     if( hooks.resizeWindow )   hooks.resizeWindow(w, h); }
bool QtHistRegistry::InfluxOut(){              return hooks.influxOut     ? hooks.influxOut()     : false; }
void QtHistRegistry::InfluxConfig(std::string & url, std::string & token, std::string & db){
  if( hooks.influxConfig ) hooks.influxConfig(url, token, db);
}

double QtHistRegistry::GateRates(int id, std::vector<std::pair<std::string,double>> & rates){
  if( id < 0 || id >= (int)h2.size() ) return 0;
  std::lock_guard<std::mutex> g(h2[id]->rateMtx);
  rates = h2[id]->rateSnap;
  return h2[id]->rateBaseS;
}

void * QtHistRegistry::View1(int id){
  return ( id >= 0 && id < (int)h1.size() && h1[id]->created ) ? (void*)h1[id]->w : nullptr;
}

void * QtHistRegistry::View2(int id){
  return ( id >= 0 && id < (int)h2.size() && h2[id]->created ) ? (void*)h2[id]->w : nullptr;
}

//^-------------------------------------------------------------- materialisation (GUI thread)

void QtHistRegistry::CreateH1(size_t i){
  H1 & h = *h1[i];
  if( h.created ) return;
  h.w = new Histogram1D(QString::fromStdString(h.name), QString::fromStdString(h.xLabel),
                        h.nBin, h.lo, h.hi, root);
  if( h.placement ) static_cast<QLayout *>(h.placement)->addWidget(h.w);
  /// A right-click rebin moves the widget's binning out from under counters sized to the old
  /// one. Resize on the analysis thread, so it cannot land in the middle of a ProcessEvent --
  /// the same fence the channel pickers use.
  const int id = (int)i;
  connect(h.w, &Histogram1D::ReBinned, h.w, [this, id](){
    hooks.fence([this, id](){ Resize1(id); });
  });
  h.created = true;
}

void QtHistRegistry::CreateH2(size_t i){
  H2 & h = *h2[i];
  if( h.created ) return;
  h.w = new Histogram2D(QString::fromStdString(h.name), QString::fromStdString(h.xLabel),
                        QString::fromStdString(h.yLabel), h.nXReq, h.xLo, h.xHi,
                        h.nYReq, h.yLo, h.yHi, root);
  if( h.placement ) static_cast<QLayout *>(h.placement)->addWidget(h.w);
  /// Geometry from the widget, never from nX/nY: they differ by the guard cells. See H2::g.
  h.g = h.w->GetCellGeom();
  const size_t n = (size_t)h.g.nx * (size_t)h.g.ny;
  h.c.reset(new std::atomic<uint32_t>[n > 0 ? n : 1]);
  for( size_t k = 0; k < n; k++ ) h.c[k].store(0, std::memory_order_relaxed);
  const int id = (int)i;
  connect(h.w, &Histogram2D::ReBinned, h.w, [this, id](){
    hooks.fence([this, id](){ Resize2(id); });
  });
  h.created = true;
}

void QtHistRegistry::CreateChan(size_t i){
  ChanReq & rq = chans[i];
  if( rq.line ) return;

  rq.line  = new QWidget(root);
  QGridLayout * lay = new QGridLayout(rq.line);
  lay->setContentsMargins(0,0,0,0);

  lay->addWidget(new QLabel(QString::fromStdString(rq.label), rq.line), 0, 0);

  rq.cbDigi = new RComboBox(rq.line);
  rq.cbCh   = new RComboBox(rq.line);

  /// Only the host knows which boards are real and how many channels each has — which is the whole
  /// reason ChannelParam exists rather than two IntParams.
  for( size_t v = 0; v < boardIndex.size(); v++ ){
    const int d = boardIndex[v];
    rq.cbDigi->addItem(QString("Digi-%1 (SN %2)").arg(d).arg(digi[d]->GetSerialNumber()), d);
  }
  lay->addWidget(rq.cbDigi, 0, 1);
  lay->addWidget(rq.cbCh,   0, 2);

  if( rq.placement ) static_cast<QLayout *>(rq.placement)->addWidget(rq.line);
}

void QtHistRegistry::CreateFlag(size_t i){
  FlagReq & rq = flags[i];
  if( rq.cb ) return;
  rq.cb = new QCheckBox(QString::fromStdString(rq.label), root);
  rq.cb->setChecked( rq.value ? *rq.value : false );
  if( rq.placement ) static_cast<QLayout *>(rq.placement)->addWidget(rq.cb);
}

void QtHistRegistry::WireChan(size_t i){
  ChanReq & rq = chans[i];
  if( rq.wired || rq.line == nullptr ) return;
  rq.wired = true;

  /// Copy the pointers out rather than capturing a reference into the vector.
  RComboBox * cbD = rq.cbDigi;
  RComboBox * cbC = rq.cbCh;
  int * dp = rq.digi;
  int * cp = rq.ch;

  auto refillChannels = [this, cbD, cbC](){
    const int d = cbD->currentData().toInt();
    cbC->clear();
    if( d >= 0 && d < (int)nDigi && digi[d] ){
      for( int c = 0; c < digi[d]->GetNChannels(); c++ ) cbC->addItem(QString::number(c), c);
    }
  };

  /// Seed the pickers from the analysis's own members BEFORE wiring, so a compiled-in default
  /// (e.g. chB = 4) reaches the GUI. Without this the combos sit at item 0 and the apply() at
  /// the bottom of this function writes that 0 back over the member -- the default never survives.
  if( dp ){
    const int s = cbD->findData(*dp);
    if( s >= 0 ) cbD->setCurrentIndex(s);
  }
  refillChannels();
  if( cp ){
    const int s = cbC->findData(*cp);
    cbC->setCurrentIndex(s >= 0 ? s : 0);
  }

  auto apply = [this, cbD, cbC, dp, cp](){
    if( !hooks.signalsActive() ) return;
    const int d = cbD->currentData().toInt();
    const int c = cbC->currentData().toInt();
    /// Runs ON the analysis thread, between ProcessEvent calls. That is the fence, and it is why
    /// the analysis can keep these as plain ints with no atomics.
    hooks.fence([dp, cp, d, c](){
      if( dp ) *dp = d;
      if( cp ) *cp = c;
    });
    /// Otherwise the plot silently mixes counts from the old and the new channel.
    ClearCounts();
  };

  connect(cbD, &QComboBox::currentIndexChanged, cbD, [refillChannels, apply](int){
    refillChannels();
    apply();
  });
  connect(cbC, &QComboBox::currentIndexChanged, cbC, [apply](int){ apply(); });
  apply();
}

void QtHistRegistry::WireFlag(size_t i){
  FlagReq & rq = flags[i];
  if( rq.wired || rq.cb == nullptr ) return;
  rq.wired = true;
  bool * v = rq.value;
  connect(rq.cb, &QCheckBox::toggled, rq.cb, [this, v](bool on){
    if( !hooks.signalsActive() ) return;
    hooks.fence([v, on](){ if( v ) *v = on; });
    ClearCounts();
  });
}

void QtHistRegistry::FinishDeclare(){
  declareFinished = true;
  for( size_t i = 0; i < chans.size(); i++ ) WireChan(i);
  for( size_t i = 0; i < flags.size();  i++ ) WireFlag(i);
}

void QtHistRegistry::DrainPending(){
  std::vector<size_t> a, b, c, d;
  {
    std::lock_guard<std::mutex> g(gate);
    a.swap(pendH1); b.swap(pendH2); c.swap(pendChan); d.swap(pendFlag);
  }
  /// Order matters only against FinishDeclare: parameters arriving after it are created AND wired
  /// in the same step; ones from before wait for the end of Declare.
  for( size_t i : a ) CreateH1(i);
  for( size_t i : b ) CreateH2(i);
  for( size_t i : c ){ CreateChan(i); if( declareFinished ) WireChan(i); }
  for( size_t i : d ){ CreateFlag(i); if( declareFinished ) WireFlag(i); }
}

//^-------------------------------------------------------------- rate ledger (v6, GUI thread)

void QtHistRegistry::SampleRates(){
  const double baseS = hooks.rateBase ? hooks.rateBase() : 0;
  if( baseS <= 0 ) return;
  const int    baseMs  = (int)( baseS * 1000.0 );
  const qint64 tMs     = (qint64)( std::chrono::duration_cast<std::chrono::milliseconds>
                                   ( std::chrono::steady_clock::now().time_since_epoch() ).count() );

  for( size_t i = 0; i < h2.size(); i++ ){
    H2 & h = *h2[i];
    if( !h.created ) continue;                 ///< queued behind a publish tick: nothing to sum yet

    //^- cumulative totals; the store's decrease detection handles every clear path, no bookkeeping
    h.store.Sample( QStringLiteral("all"), tMs,
                    h.total.load(std::memory_order_relaxed), baseMs + baseMs/2 );
    const int nc = h.w->GetCuts().size();
    for( int ci = 0; ci < nc; ci++ ){
      if( h.w->GetCuts()[ci].poly.isEmpty() ) continue;   // tombstone
      h.w->RecountCut(ci);                     ///< cell-sum inside the polygon; also revives the frozen cut number
      const Histogram2D::Cut & c = h.w->GetCuts()[ci];
      if( c.entries < 0 ) continue;            ///< deleted during the RecountCut
      h.store.Sample( c.name, tMs, (unsigned long) c.entries, baseMs + baseMs/2 );
    }

    //^- publish the {tag, Hz} snapshot the analysis thread reads. A key with fewer than two
    //^  samples is absent, never a faked zero.
    std::vector<std::pair<std::string,double>> rows;
    double r;
    if( h.store.Rate( QStringLiteral("all"), baseMs, r ) ) rows.push_back( { "all", r } );
    for( int ci = 0; ci < nc; ci++ ){
      const Histogram2D::Cut & c = h.w->GetCuts()[ci];
      if( c.poly.isEmpty() || c.entries < 0 ) continue;
      if( h.store.Rate( c.name, baseMs, r ) )
        rows.push_back( { c.name.toStdString(), r } );
    }
    std::lock_guard<std::mutex> g(h.rateMtx);
    h.rateSnap.swap( rows );
    h.rateBaseS = baseS;
  }
}

void QtHistRegistry::ResetRates(){
  for( size_t i = 0; i < h2.size(); i++ ){
    h2[i]->store.ResetHistory();               ///< sampling clock restarts with the run
    std::lock_guard<std::mutex> g(h2[i]->rateMtx);
    h2[i]->rateSnap.clear();
  }
}

//^-------------------------------------------------------------- filling (analysis thread)

void QtHistRegistry::Fill1(int id, double x){
  if( id < 0 || id >= (int)h1.size() ) return;
  H1 & h = *h1[id];
  /// total counts only in-range entries (what "Total Entry" shows); under/over are the rejects.
  /// NaN fails both range tests below and would reach the (int) cast, which is undefined for a
  /// non-finite value. An analysis computing a ratio or a log makes NaN easily, so check first.
  if( !std::isfinite(x) ){ h.over.fetch_add(1, std::memory_order_relaxed); return; }
  if( x <  h.lo ){ h.under.fetch_add(1, std::memory_order_relaxed); return; }
  if( x >= h.hi ){ h.over .fetch_add(1, std::memory_order_relaxed); return; }
  h.total.fetch_add(1, std::memory_order_relaxed);
  const int b = (int)((x - h.lo) / h.dx);
  if( b >= 0 && b < h.nBin ) h.c[b].fetch_add(1, std::memory_order_relaxed);
}

void QtHistRegistry::Fill2(int id, double x, double y){
  if( id < 0 || id >= (int)h2.size() ) return;
  H2 & h = *h2[id];
  h.total.fetch_add(1, std::memory_order_relaxed);
  if( !std::isfinite(x) || !std::isfinite(y)
      || x < h.xLo || x >= h.xHi || y < h.yLo || y >= h.yHi ){
    h.out.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  /// No widget yet means no guard-cell geometry: the run between an analysis-thread declaration
  /// and the next publish tick counts totals, not cells. Sub-tick data, deliberately lost rather
  /// than binned against an axis that does not exist yet.
  if( !h.created ) return;
  /// CoordToCell reproduces the widget's own cell mapping and rejects out-of-range and non-finite
  /// input, so the NaN guard and the geometry fix are the same call.
  int ix = 0, iy = 0;
  if( !Histogram2D::CoordToCell(h.g, x, y, &ix, &iy) ){
    h.out.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  h.c[(size_t)ix * h.g.ny + iy].fetch_add(1, std::memory_order_relaxed);
}

//^-------------------------------------------------------------- publishing (GUI thread)

void QtHistRegistry::Publish(){
  DrainPending();

  for( size_t i = 0; i < h1.size(); i++ ){
    H1 & h = *h1[i];
    if( !h.created ) continue;
    scratch.resize(h.nBin);
    for( int b = 0; b < h.nBin; b++ ) scratch[b] = h.c[b].load(std::memory_order_relaxed);
    h.w->SetBinContents(scratch.data(), h.nBin,
                        (unsigned long) h.total.load(std::memory_order_relaxed),
                        (unsigned long) h.under.load(std::memory_order_relaxed),
                        (unsigned long) h.over .load(std::memory_order_relaxed));
    h.w->UpdatePlot();
  }

  for( size_t i = 0; i < h2.size(); i++ ){
    H2 & h = *h2[i];
    if( !h.created ) continue;
    const size_t n = (size_t)h.g.nx * h.g.ny;
    scratch.resize(n);
    for( size_t k = 0; k < n; k++ ) scratch[k] = h.c[k].load(std::memory_order_relaxed);
    h.w->SetCellContents(scratch.data(), h.g.nx, h.g.ny,
                         (unsigned long) h.total.load(std::memory_order_relaxed),
                         0,
                         (unsigned long) h.out.load(std::memory_order_relaxed));
    h.w->UpdatePlot();
  }
}

void QtHistRegistry::Resize1(int id){
  if( id < 0 || id >= (int)h1.size() ) return;
  H1 & h = *h1[id];
  if( !h.created ) return;
  h.nBin = h.w->GetNBin();
  h.lo   = h.w->GetXMin();
  h.hi   = h.w->GetXMax();
  h.dx   = ( h.nBin > 0 ) ? (h.hi - h.lo) / h.nBin : 1;
  h.c.reset(new std::atomic<uint32_t>[h.nBin > 0 ? h.nBin : 1]);
  for( int b = 0; b < h.nBin; b++ ) h.c[b].store(0, std::memory_order_relaxed);
  h.total.store(0); h.under.store(0); h.over.store(0);
}

void QtHistRegistry::Resize2(int id){
  if( id < 0 || id >= (int)h2.size() ) return;
  H2 & h = *h2[id];
  if( !h.created ) return;
  h.g = h.w->GetCellGeom();
  const size_t n = (size_t)h.g.nx * h.g.ny;
  h.c.reset(new std::atomic<uint32_t>[n > 0 ? n : 1]);
  for( size_t k = 0; k < n; k++ ) h.c[k].store(0, std::memory_order_relaxed);
  h.total.store(0); h.out.store(0);
}

void QtHistRegistry::ClearCounts(){
  for( size_t i = 0; i < h1.size(); i++ ){
    H1 & h = *h1[i];
    if( h.c ) for( int b = 0; b < h.nBin; b++ ) h.c[b].store(0, std::memory_order_relaxed);
    h.total.store(0); h.under.store(0); h.over.store(0);
  }
  for( size_t i = 0; i < h2.size(); i++ ){
    H2 & h = *h2[i];
    if( h.c ){
      const size_t n = (size_t)h.g.nx * h.g.ny;
      for( size_t k = 0; k < n; k++ ) h.c[k].store(0, std::memory_order_relaxed);
    }
    h.total.store(0); h.out.store(0);
  }
}

void QtHistRegistry::Clear(){
  /// The widgets are the work area's children -- the window destroys that, not us. We only drop
  /// the id -> widget mappings so no stale pointer can be published after this point.
  h1.clear();
  h2.clear();
  chans.clear();
  flags.clear();
  std::lock_guard<std::mutex> g(gate);
  pendH1.clear(); pendH2.clear(); pendChan.clear(); pendFlag.clear();
}

//^============================================================ BuildWorker

void BuildWorker::Init(){
  timer = new QTimer(this);          // created here, so it lives on this thread
  connect(timer, &QTimer::timeout, this, &BuildWorker::Tick);
}

void BuildWorker::Start(int periodMs){
  eb->ResetStat();
  eb->Reset();
  timer->start(periodMs);
}

void BuildWorker::Stop(){
  timer->stop();
  eb->BuildEvents(true);             // flush the tail into the ring before the analysis drains
}

void BuildWorker::Tick(){
  /// Bounded, so a GUI thread blocking on one of our slots never waits long.
  eb->BuildEvents(false, 20000);
}

//^============================================================ AnaWorker

void AnaWorker::Init(){
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &AnaWorker::Tick);
}

void AnaWorker::SetAnalysis(Analysis * a, QtHistRegistry * r){
  ana = a;
  reg = r;
}

void AnaWorker::Start(int periodMs){
  reader.Rewind(*ring);
  if( ana ) ana->OnRunStart();
  timer->start(periodMs);
}

void AnaWorker::Stop(){
  timer->stop();
  Drain();                           // the builder already flushed; take what it left
  if( ana ) ana->OnRunStop();
}

void AnaWorker::Tick(){ Drain(); }

void AnaWorker::Drain(){
  if( ana == nullptr || reg == nullptr ) return;
  int n = 0;
  while( n < 20000 && reader.Next(*ring, scratch) ){
    ana->ProcessEvent(scratch.data(), (int)scratch.size(), *reg);
    n++;
  }
}

void AnaWorker::ApplyChannel(int * digiPtr, int * channel, int d, int c){
  if( digiPtr ) *digiPtr = d;
  if( channel ) *channel = c;
}

void AnaWorker::ApplyFlag(bool * value, bool v){
  if( value ) *value = v;
}

//^============================================================ SettingsKV over QJsonObject

namespace {

class JsonKV : public SettingsKV {
public:
  JsonKV( QJsonObject * o ) : obj(o) {}

  void Set(const char * key, double value) override { obj->insert(QString::fromUtf8(key), value); }
  bool Get(const char * key, double * value) override {
    const QJsonValue v = obj->value(QString::fromUtf8(key));
    if( !v.isDouble() ) return false;
    *value = v.toDouble();
    return true;
  }
  void SetStr(const char * key, const char * value) override {
    obj->insert(QString::fromUtf8(key), QString::fromUtf8(value));
  }
  bool GetStr(const char * key, std::string & out) override {
    const QJsonValue v = obj->value(QString::fromUtf8(key));
    if( !v.isString() ) return false;
    out = v.toString().toStdString();
    return true;
  }
private:
  QJsonObject * obj;
};

} // namespace

//^============================================================ Analyzer

Analyzer::Analyzer(Digitizer2Gen ** digi_, unsigned int nDigi_, QMainWindow * parent)
  : QMainWindow(parent), digi(digi_), nDigi(nDigi_){

  setWindowTitle("Online Analyzer");
  QScreen * screen = QGuiApplication::primaryScreen();
  QRect g = screen ? screen->geometry() : QRect(0,0,1200,800);
  setGeometry(0, 0, qMin(1200, g.width()-100), qMin(800, g.height()-100));

  BuildViewList();

  ring.reset(new EventRing());
  eb.reset(new OnlineEventBuilder(MakeViews()));
  eb->SetEventSink(ring.get());

  //^---- layout, ABI v4: one horizontal host bar across the top, work area everywhere else.
  //^     The bar -- selector, Enable, coincidence window, guard factor, Rescan/Rebuild/Reload,
  //^     compiler log -- is FIXED chrome: an analysis can drive these values through the
  //^     registry's setters, but never move or remove the widgets. Only the status label is
  //^     claimable, and it has no bar cell to begin with.
  QWidget * central = new QWidget(this);
  setCentralWidget(central);
  topLay = new QGridLayout(central);

  QGroupBox * cfgBox = new QGroupBox("Analyzer Control", central);
  cfgBox->setMaximumHeight(190);
  QGridLayout * cfg = new QGridLayout(cfgBox);
  topLay->addWidget(cfgBox, 0, 0, 1, 10);

  chkEnable = new QCheckBox("Enable analyzer", cfgBox);
  cfg->addWidget(chkEnable, 0, 0, 1, 1);

  //^---- "Influx out": host chrome next to Enable, gated on MainWindow's connection (SetInflux).
  //^     The WRITE lives in the analysis (ABI v5 hands it this flag plus url/token/database);
  //^     nothing here talks to the database itself.
  chkInflux = new QCheckBox("Influx out", cfgBox);
  chkInflux->setEnabled(false);
  chkInflux->setToolTip("Let the selected analysis push its data to InfluxDB"
                        " (Program Settings -> Database must be connected).");
  cfg->addWidget(chkInflux, 0, 1, 1, 2);
  connect(chkInflux, &QCheckBox::toggled, this, [this](bool on){
    if( !isSignalSlotActive ) return;
    influxOut.store(on, std::memory_order_relaxed);
  });

  cfg->addWidget(new QLabel("Analysis Selector", cfgBox), 1, 0);
  cbAnalysis = new RComboBox(cfgBox);
  cfg->addWidget(cbAnalysis, 1, 1);

  cfg->addWidget(new QLabel("Coincidence window [ns]", cfgBox), 2, 0);
  sbTimeWindow = new RSpinBox(cfgBox, 0);
  sbTimeWindow->setRange(0, 1e9);
  sbTimeWindow->setValue((double) eb->GetTimeWindow());
  cfg->addWidget(sbTimeWindow, 2, 1);

  sbGuardFactor = new RSpinBox(cfgBox, 0);
  sbGuardFactor->setRange(1, 1000);
  sbGuardFactor->setValue(5);
  sbGuardFactor->setSuffix(" x window");
  cfg->addWidget(new QLabel("Guard time", cfgBox), 3, 0);
  cfg->addWidget(sbGuardFactor, 3, 1);

  //^---- v6: the averaging window behind GateRates, shared semantics with the RateMonitor's
  //^     time base. Live-adjustable: the store widens its retention on demand, so raising it
  //^     mid-run needs no restart.
  sbRateBase = new RSpinBox(cfgBox, 0);
  sbRateBase->setRange(0.5, 3600);
  sbRateBase->setValue(5);
  sbRateBase->setSuffix(" s");
  sbRateBase->setToolTip("Averaging window for total / 2D-cut rates pushed to InfluxDB.\n"
                         "Points tile back-to-back at this period (RateMonitor semantics).");
  cfg->addWidget(new QLabel("Rate average", cfgBox), 4, 0);
  cfg->addWidget(sbRateBase, 4, 1);

  bnRescan  = new QPushButton("Rescan",  cfgBox);
  bnRebuild = new QPushButton("Rebuild", cfgBox);
  bnReload  = new QPushButton("Reload",  cfgBox);
  cfg->addWidget(bnRescan,  1, 2);           // row 0 cols 1-2 belong to the Enable span
  cfg->addWidget(bnRebuild, 2, 2);
  cfg->addWidget(bnReload,  3, 2);

  logPane = new QPlainTextEdit(cfgBox);
  logPane->setReadOnly(true);
  logPane->setPlaceholderText("compiler output");
  cfg->addWidget(logPane, 0, 3, 4, 4);
  cfg->setColumnStretch(3, 4);

  //^---- the vault: home for the status label until StatusControl() is claimed.
  ctrlVault = new QWidget(central);
  ctrlVault->setFixedSize(0, 0);

  lbStatus = new QLabel("idle", ctrlVault);
  lbStatus->hide();

  topLay->setRowStretch(1, 1);
  // topLay->setColumnStretch(1, 1);
  ResetWorkArea();

  //^---- threads
  buildThread = new QThread(this);
  anaThread   = new QThread(this);
  buildWorker = new BuildWorker(eb.get());
  anaWorker   = new AnaWorker(ring.get());
  buildWorker->moveToThread(buildThread);
  anaWorker->moveToThread(anaThread);
  connect(buildThread, &QThread::started, buildWorker, &BuildWorker::Init);
  connect(anaThread,   &QThread::started, anaWorker,   &AnaWorker::Init);
  buildThread->start();
  anaThread->start();

  replotTimer = new QTimer(this);          // GUI thread
  connect(replotTimer, &QTimer::timeout, this, [this](){
    if( registry && isVisible() ) registry->Publish();
    if( registry ) registry->SampleRates();   ///< the numbers are NOT visibility-gated: a hidden
                                              ///  (minimised) window keeps its rate points complete
    UpdateStatus();
  });

  //^---- wiring
  connect(chkEnable, &QCheckBox::toggled, this, [this](bool on){
    if( !isSignalSlotActive ) return;
    SetEnabled(on);
  });

  /// Guard time is factor x window, so moving either one moves the other's product. Idle-only:
  /// sbTimeWindow/sbGuardFactor are disabled while running, exactly like the old raw guard spin.
  connect(sbGuardFactor, &QDoubleSpinBox::valueChanged, this, [this](double f){
    if( !isSignalSlotActive ) return;
    eb->SetGuardTime((uint64_t)( f * (double) sbTimeWindow->value() ));
  });
  connect(sbTimeWindow, &QDoubleSpinBox::valueChanged, this, [this](double w){
    if( !isSignalSlotActive ) return;
    eb->SetGuardTime((uint64_t)( sbGuardFactor->value() * w ));
  });

  connect(cbAnalysis, &QComboBox::currentTextChanged, this, [this](const QString & name){
    if( !isSignalSlotActive ) return;
    const bool wasOn = chkEnable->isChecked();
    if( wasOn ) SetEnabled(false);
    SelectAnalysis(name);
    if( wasOn ) SetEnabled(true);
  });

  connect(bnRescan,  &QPushButton::clicked, this, [this](){ RescanPlugins(); });
  connect(bnRebuild, &QPushButton::clicked, this, [this](){ Rebuild(); });
  connect(bnReload,  &QPushButton::clicked, this, [this](){ Reload(); });

  /// Analyses live next to the sources they are built from, so the Rebuild button can just run
  /// their Makefile. applicationDirPath keeps this working when the DAQ is started from elsewhere.
  pluginDir = QCoreApplication::applicationDirPath() + "/analyzers/build";
  stateDir  = QCoreApplication::applicationDirPath() + "/analyzers/state";

  RescanPlugins();
  if( cbAnalysis->count() > 0 ) SelectAnalysis(cbAnalysis->currentText());
  UpdateStatus();
}

void Analyzer::closeEvent( QCloseEvent * ev ){
  /// Close == stop: toggling Enable off runs the whole shutdown (workers, ring, sampling clock)
  /// and leaves the checkbox honest for the next show. The analysis thread's OnRunStop still
  /// flushes the last Influx batch -- stopping the PUSH, not losing the final points.
  if( chkEnable && chkEnable->isChecked() ) chkEnable->setChecked(false);
  QMainWindow::closeEvent(ev);
}

QWidget * Analyzer::ResetWorkArea(){  if( workArea ){
    topLay->removeWidget(workArea);
    delete workArea;                              // takes the analysis' layout tree with it
  }
  workArea = new QWidget(this->centralWidget());
  workArea->setObjectName("workArea");            ///< the test hook: the analysis' whole canvas
  topLay->addWidget(workArea, 1, 0, 3, 10);
  return workArea;
}

Analyzer::~Analyzer(){

  /// Order matters: stop producing, stop consuming, join both threads, and only then let the
  /// builder and ring go. The builder holds pointers into digi[]'s hit rings.
  if( chkEnable && chkEnable->isChecked() ) SetEnabled(false);
  if( replotTimer ) replotTimer->stop();

  if( buildThread ){ buildThread->quit(); buildThread->wait(); }
  if( anaThread   ){ anaThread->quit();   anaThread->wait();   }
  delete buildWorker;
  delete anaWorker;

  /// The ABI teardown contract: the work area -- every widget the registry built and everything
  /// the analysis put inside -- dies BEFORE the analysis object, so a .so can never be called
  /// back by a dying widget. The bar outlives it as the window's own children.
  SaveAnalysisState();
  if( registry ){ registry->Clear(); delete registry; registry = nullptr; }
  if( workArea ){ topLay->removeWidget(workArea); delete workArea; workArea = nullptr; }
  plugin.Destroy(analysis);
  analysis = nullptr;
}

/// Real boards only. A dummy Digitizer2Gen exists to hold and round-trip a settings file without
/// hardware; it never produces data. Including one would be fatal rather than merely useless: the
/// build horizon is a minimum over boards, so a board that never delivers pins it at zero and NO
/// events are built at all until the stall timeout, after every reset.
void Analyzer::BuildViewList(){
  boardIndex.clear();
  nChannels.clear();
  for( unsigned int i = 0; i < nDigi; i++ ){
    if( digi[i] == nullptr ) continue;
    if( digi[i]->IsDummy() || !digi[i]->IsConnected() ) continue;
    boardIndex.push_back((int)i);
    nChannels.push_back((uint8_t) digi[i]->GetNChannels());
  }
}

std::vector<DigiHitView> Analyzer::MakeViews() const {
  std::vector<DigiHitView> v;
  for( size_t k = 0; k < boardIndex.size(); k++ ){
    const int i = boardIndex[k];
    DigiHitView d;
    d.sn        = (uint16_t) digi[i]->GetSerialNumber();
    d.digiIndex = (uint8_t) i;        // the GUI's board number, not the view position
    d.ring      = &digi[i]->hitRing;
    v.push_back(d);
  }
  return v;
}

//^============================================================ plugin management

void Analyzer::Log(const QString & text, bool isError){
  if( logPane == nullptr ){ printf("Analyzer: %s\n", text.toStdString().c_str()); return; }
  logPane->appendPlainText( (isError ? QString("ERROR: ") : QString()) + text );
}

void Analyzer::RescanPlugins(){

  found = AnalysisPlugin::Scan(pluginDir.toStdString());
  std::sort(found.begin(), found.end(),
            [](const AnalysisPlugin::Info & a, const AnalysisPlugin::Info & b){
              return a.name < b.name;                        ///< deterministic "first plugin"
            });

  const QString keep = cbAnalysis->currentText().isEmpty() ? currentName : cbAnalysis->currentText();
  isSignalSlotActive = false;
  cbAnalysis->clear();
  for( size_t i = 0; i < found.size(); i++ ){
    cbAnalysis->addItem(QString::fromStdString(found[i].name));
  }
  const int back = cbAnalysis->findText(keep);
  if( back >= 0 ) cbAnalysis->setCurrentIndex(back);
  isSignalSlotActive = true;

  if( found.empty() ){
    Log("no analyses in " + pluginDir + " -- build one with: make -C analyzers", true);
    return;
  }
  currentName = cbAnalysis->currentText().isEmpty()
                ? QString::fromStdString(found[0].name) : cbAnalysis->currentText();

  Log(QString("%1 analysis plugin(s) in %2, using %3")
        .arg(found.size()).arg(pluginDir).arg(currentName));
}

/// Compile through analyzers/Makefile rather than a g++ line here, so there is exactly one
/// definition of how an analysis is built and the command line and the button cannot drift apart.
void Analyzer::Rebuild(){

  if( builder && builder->state() != QProcess::NotRunning ){
    Log("a build is already running", true);
    return;
  }

  const QString name = cbAnalysis->currentText();
  if( name.isEmpty() ){ Log("nothing selected to rebuild", true); return; }

  if( builder == nullptr ){
    builder = new QProcess(this);
    builder->setProcessChannelMode(QProcess::MergedChannels);

    connect(builder, &QProcess::readyReadStandardOutput, this, [this](){
      Log(QString::fromLocal8Bit(builder->readAllStandardOutput()).trimmed());
    });

    connect(builder, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus){
      if( code == 0 ){
        Log("build OK -- press Reload to swap it in");
        bnReload->setEnabled(true);
      }else{
        /// Leave the running analysis alone. A typo mid-run must cost nothing.
        Log(QString("build FAILED (exit %1); the running analysis is unchanged").arg(code), true);
        bnReload->setEnabled(false);
      }
      bnRebuild->setEnabled(true);
    });
  }

  const QString srcDir = QFileInfo(pluginDir).dir().absolutePath();
  bnRebuild->setEnabled(false);
  bnReload->setEnabled(false);
  logPane->clear();
  Log("make -C " + srcDir + " build/" + name + ".so");
  builder->setWorkingDirectory(srcDir);
  builder->start("make", QStringList() << ("build/" + name + ".so"));
}

/// Swap the analysis without restarting the DAQ.
///
/// The heavy lifting already exists: toggling analysis off stops both workers, flushes and clears,
/// and SelectAnalysis() rebuilds the widgets and hands the new object to the analysis thread. This
/// is just those two, in the right order, around a fresh dlopen.
void Analyzer::Reload(){
  const QString name = cbAnalysis->currentText();
  if( name.isEmpty() ) return;

  const bool wasOn = chkEnable->isChecked();
  if( wasOn ){
    isSignalSlotActive = false;
    chkEnable->setChecked(false);
    isSignalSlotActive = true;
    SetEnabled(false);
  }

  RescanPlugins();          // pick up a .so that appeared since the window opened
  SelectAnalysis(name);

  if( wasOn && analysis ){
    isSignalSlotActive = false;
    chkEnable->setChecked(true);
    isSignalSlotActive = true;
    SetEnabled(true);
  }
  UpdateStatus();
}

void Analyzer::SaveAnalysisState(){
  if( analysis == nullptr ) return;
  const QString name = QString::fromStdString(plugin.DeclaredName());
  if( name.isEmpty() ) return;

  QJsonObject obj;
  JsonKV kv(&obj);
  analysis->SaveState(kv);

  QDir().mkpath(stateDir);
  QFile f(stateDir + "/" + name + ".json");
  if( !f.open(QIODevice::WriteOnly | QIODevice::Truncate) ){
    Log("cannot write " + f.fileName() + ": " + f.errorString(), true);
    return;
  }
  f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
  f.close();
}

void Analyzer::LoadAnalysisState(const QString & name){
  if( analysis == nullptr || name.isEmpty() ) return;

  QFile f(stateDir + "/" + name + ".json");
  if( !f.exists() ) return;                        ///< first run for this analysis: defaults win
  if( !f.open(QIODevice::ReadOnly) ){
    Log("cannot read " + f.fileName() + ": " + f.errorString(), true);
    return;
  }
  const QByteArray raw = f.readAll();
  f.close();

  QJsonParseError perr;
  const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
  if( perr.error != QJsonParseError::NoError || !doc.isObject() ){
    Log("state file " + f.fileName() + " is not valid JSON -- starting at defaults", true);
    return;
  }
  QJsonObject obj = doc.object();
  JsonKV kv(&obj);
  analysis->LoadState(kv);
}

void Analyzer::SelectAnalysis(const QString & name){

  /// ABI teardown order, part one: state out, mappings down, widgets (work area) gone, analysis
  /// last -- nothing of the old selection survives into the new one's Declare().
  SaveAnalysisState();
  if( registry ){ registry->Clear(); delete registry; registry = nullptr; }
  /// The bar never leaves, but the status label may sit inside the dying canvas: park it first.
  if( lbStatus->parentWidget() != ctrlVault ){ lbStatus->setParent(ctrlVault); lbStatus->hide(); }
  ResetWorkArea();
  setWindowTitle("Online Analyzer"); ///< dressing the previous analysis claimed is host property too
  plugin.Destroy(analysis);        // the plugin allocated it, so the plugin frees it
  analysis = nullptr;

  std::string path;
  for( size_t i = 0; i < found.size(); i++ ){
    if( QString::fromStdString(found[i].name) == name ){ path = found[i].path; break; }
  }
  if( path.empty() ){
    Log("no plugin called \"" + name + "\". Build one: make -C analyzers", true);
    return;
  }

  std::string err;
  analysis = plugin.Load(path, err);
  if( analysis == nullptr ){
    /// A bad or stale .so must never take the DAQ with it, so this is a log line and a disabled
    /// analysis, not an abort.
    Log(QString::fromStdString(err), true);
    return;
  }
  Log(QString("loaded %1 (declares \"%2\", load #%3)")
        .arg(QString::fromStdString(path))
        .arg(QString::fromStdString(plugin.DeclaredName()))
        .arg(plugin.LoadCount()));

  AnalyzerHooks hooks;
  hooks.fence = [this](const std::function<void()> & fn){
    QMetaObject::invokeMethod(anaWorker, fn, Qt::BlockingQueuedConnection);
  };
  hooks.signalsActive = [this](){ return isSignalSlotActive; };
  hooks.claimStatus      = [this](){ lbStatus->show(); return (QWidget*)lbStatus; };
  hooks.timeWindow    = [this](){ return (uint64_t) sbTimeWindow->value(); };
  hooks.setTimeWindow = [this](uint64_t ns){
    if( chkEnable->isChecked() ){ Log("SetTimeWindow ignored: analysis is running", true); return; }
    sbTimeWindow->setValue((double) ns);           ///< the valueChanged wiring updates the builder
  };
  hooks.guardFactor    = [this](){ return sbGuardFactor->value(); };
  hooks.setGuardFactor = [this](double f){
    if( chkEnable->isChecked() ){ Log("SetGuardFactor ignored: analysis is running", true); return; }
    sbGuardFactor->setValue(f);
  };
  hooks.setWindowTitle = [this](const QString & t){ setWindowTitle(t); };
  hooks.resizeWindow   = [this](int w, int h){ resize(w, h); };
  hooks.influxOut      = [this](){ return influxOut.load(std::memory_order_relaxed); };
  hooks.influxConfig   = [this](std::string & url, std::string & token, std::string & db){
    std::lock_guard<std::mutex> g(influxMtx);
    url = influxURL; token = influxToken; db = influxDatabase;
  };
  hooks.rateBase       = [this](){ return sbRateBase ? sbRateBase->value() : 5.0; };

  registry = new QtHistRegistry(workArea, digi, nDigi, boardIndex, hooks);

  AnalysisContext ctx;
  ctx.nBoards    = (int) boardIndex.size();
  ctx.nChannels  = nChannels.empty() ? nullptr : nChannels.data();
  ctx.timeWindow = eb->GetTimeWindow();

  analysis->Declare(*registry, ctx);   // GUI thread: this is where widgets get created
  LoadAnalysisState(QString::fromStdString(plugin.DeclaredName()));
  registry->FinishDeclare();           // pickers seed from the restored members, then fence-apply

  currentName = QString::fromStdString(plugin.DeclaredName());
  UpdateStatus();

  Analysis * a = analysis;
  QtHistRegistry * r = registry;
  QMetaObject::invokeMethod(anaWorker, [this, a, r](){ anaWorker->SetAnalysis(a, r); },
                            Qt::BlockingQueuedConnection);
}

void Analyzer::SetEnabled(bool on){

  if( on ){

    if( boardIndex.empty() ){
      lbStatus->setText("no connected digitizer");
      isSignalSlotActive = false; chkEnable->setChecked(false); isSignalSlotActive = true;
      return;
    }

    eb->SetTimeWindow((uint64_t) sbTimeWindow->value());
    eb->SetGuardTime((uint64_t)( sbGuardFactor->value() * sbTimeWindow->value() ));

    /// Everything is stopped here, so clearing is race-free: the fill flag has been false for the
    /// whole idle period, so no producer can be mid-push.
    for( size_t k = 0; k < boardIndex.size(); k++ ) digi[boardIndex[k]]->hitRing.clear();
    ring->Clear();
    if( registry ){ registry->ClearCounts(); registry->ResetRates(); }

    for( size_t k = 0; k < boardIndex.size(); k++ ){
      digi[boardIndex[k]]->fillHitRing.store(true, std::memory_order_relaxed);
    }

    QMetaObject::invokeMethod(buildWorker, [this](){ buildWorker->Start(20); },
                              Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(anaWorker,   [this](){ anaWorker->Start(20); },
                              Qt::BlockingQueuedConnection);
    replotTimer->start(500);

  }else{

    replotTimer->stop();

    /// Stop the builder and flush first, then let the analysis drain what that produced. Doing it
    /// the other way round builds the tail of the run and then throws it away.
    QMetaObject::invokeMethod(buildWorker, [this](){ buildWorker->Stop(); },
                              Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(anaWorker,   [this](){ anaWorker->Stop(); },
                              Qt::BlockingQueuedConnection);

    /// Flag off only -- do NOT clear the ring here. A DAQ thread that read fillHitRing as true a
    /// moment ago can still be inside push(), and clear() memsets the buffer and resets the write
    /// index underneath it. The enable path above does the clearing instead, where the flag has
    /// been false for the whole idle period and no producer can be mid-push.
    for( size_t k = 0; k < boardIndex.size(); k++ ){
      digi[boardIndex[k]]->fillHitRing.store(false, std::memory_order_relaxed);
    }
    ring->Clear();

    /// Run boundary for the rate ledger: AnaWorker's Stop already ran the analysis' OnRunStop
    /// flush on the FRESH snapshot, so the published view must not keep serving its last live
    /// values to an idle window -- the plugin scatter reads an empty snapshot as "run over".
    if( registry ) registry->ResetRates();

    if( registry ) registry->Publish();      // show the final state
  }

  sbTimeWindow->setEnabled(!on);             // no marshalling needed if they only change when idle
  sbGuardFactor->setEnabled(!on);
  cbAnalysis->setEnabled(!on);
  UpdateStatus();
}

void Analyzer::UpdateStatus(){
  if( eb == nullptr ) return;
  const bool on = chkEnable && chkEnable->isChecked();
  lbStatus->setText(QString("%1 | boards %2 | events %3 | hits dropped %4 | late %5 | "
                            "monotonicity %6 | ring dropped %7")
                    .arg(on ? "RUNNING" : "idle")
                    .arg(boardIndex.size())
                    .arg(eb->totalEventsBuilt.load(std::memory_order_relaxed))
                    .arg(eb->totalHitsDropped.load(std::memory_order_relaxed))
                    .arg(eb->totalHitsLate.load(std::memory_order_relaxed))
                    .arg(eb->monotonicityViolations.load(std::memory_order_relaxed))
                    .arg(anaWorker ? anaWorker->reader.eventsDropped : 0));
}

void Analyzer::SetInflux(const QString & url, const QString & token, const QString & database){
  {
    std::lock_guard<std::mutex> g(influxMtx);
    influxURL      = url.toStdString();
    influxToken    = token.toStdString();
    influxDatabase = database.toStdString();
  }
  const bool healthy = !url.isEmpty() && !database.isEmpty();
  if( chkInflux ){
    const QSignalBlocker guard(chkInflux);            ///< mirrors RateMonitor::SetInflux
    chkInflux->setEnabled(healthy);
    chkInflux->setChecked(healthy && chkInflux->isChecked());
    chkInflux->setToolTip(healthy
        ? "Let the selected analysis push its data to InfluxDB (one batched POST per write tick)."
        : "InfluxDB is not configured or not connected (Program Settings -> Database).");
    influxOut.store(chkInflux->isChecked(), std::memory_order_relaxed);
  }
}
