#include "Analyzer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>

#include <cmath>

//^============================================================ QtHistRegistry

QtHistRegistry::QtHistRegistry(QWidget * plotParent_, QGridLayout * plotLayout_,
                               QWidget * ctrlParent_, QVBoxLayout * ctrlLayout_,
                               Digitizer2Gen ** digi_, const std::vector<int> & boardIndex_)
  : plotParent(plotParent_), plotLayout(plotLayout_),
    ctrlParent(ctrlParent_), ctrlLayout(ctrlLayout_),
    digi(digi_), boardIndex(boardIndex_) {}

int QtHistRegistry::Hist1D(const char * name, const char * xLabel,
                           int nBin, double xMin, double xMax, int row, int col){

  /// Histogram1D::Rebin silently clamps at 1000 bins; clamp here so the axis and our count array
  /// cannot disagree, and say so rather than mis-binning quietly.
  if( nBin > 1000 ){
    printf("Analyzer: histogram \"%s\" asked for %d bins, clamped to 1000\n", name, nBin);
    nBin = 1000;
  }
  if( nBin < 1 ) nBin = 1;

  std::unique_ptr<H1> h(new H1());
  h->w    = new Histogram1D(name, xLabel, nBin, xMin, xMax, plotParent);
  h->nBin = nBin;
  h->lo   = xMin;
  h->hi   = xMax;
  h->dx   = (xMax - xMin) / nBin;
  h->c.reset(new std::atomic<uint32_t>[nBin]);
  for( int i = 0; i < nBin; i++ ) h->c[i].store(0, std::memory_order_relaxed);

  plotLayout->addWidget(h->w, row, col);
  h1.push_back(std::move(h));
  return (int)h1.size() - 1;
}

int QtHistRegistry::Hist2D(const char * name, const char * xLabel, const char * yLabel,
                           int nX, double xMin, double xMax,
                           int nY, double yMin, double yMax, int row, int col){

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
  h->w   = new Histogram2D(name, xLabel, yLabel, nX, xMin, xMax, nY, yMin, yMax, plotParent);
  h->nX  = nX;  h->xLo = xMin;  h->xHi = xMax;  h->dx = (xMax - xMin) / nX;
  h->nY  = nY;  h->yLo = yMin;  h->yHi = yMax;  h->dy = (yMax - yMin) / nY;
  h->c.reset(new std::atomic<uint32_t>[(size_t)nX * nY]);
  for( size_t i = 0; i < (size_t)nX * nY; i++ ) h->c[i].store(0, std::memory_order_relaxed);

  plotLayout->addWidget(h->w, row, col);
  h2.push_back(std::move(h));
  return (int)h2.size() - 1;
}

void QtHistRegistry::ChannelParam(const char * label, int * digiPtr, int * chPtr){

  QWidget * line = new QWidget(ctrlParent);
  QGridLayout * lay = new QGridLayout(line);
  lay->setContentsMargins(0,0,0,0);

  lay->addWidget(new QLabel(label, line), 0, 0);

  RComboBox * cbD = new RComboBox(line);
  RComboBox * cbC = new RComboBox(line);

  /// Only the host knows which boards are real and how many channels each has — which is the whole
  /// reason ChannelParam exists rather than two IntParams.
  for( size_t v = 0; v < boardIndex.size(); v++ ){
    const int d = boardIndex[v];
    cbD->addItem(QString("Digi-%1 (SN %2)").arg(d).arg(digi[d]->GetSerialNumber()), d);
  }
  lay->addWidget(cbD, 0, 1);
  lay->addWidget(cbC, 0, 2);

  ctrlLayout->addWidget(line);
  chanWidget.push_back({cbD, cbC, digiPtr, chPtr});
}

void QtHistRegistry::BoolParam(const char * label, bool * value){
  QCheckBox * cb = new QCheckBox(label, ctrlParent);
  cb->setChecked(*value);
  ctrlLayout->addWidget(cb);
  flagWidget.push_back(cb);
  flagValue.push_back(value);
}

void QtHistRegistry::Fill1(int id, double x){
  if( id < 0 || id >= (int)h1.size() ) return;
  H1 & h = *h1[id];
  h.total.fetch_add(1, std::memory_order_relaxed);
  if( x <  h.lo ){ h.under.fetch_add(1, std::memory_order_relaxed); return; }
  if( x >= h.hi ){ h.over .fetch_add(1, std::memory_order_relaxed); return; }
  const int b = (int)((x - h.lo) / h.dx);
  if( b >= 0 && b < h.nBin ) h.c[b].fetch_add(1, std::memory_order_relaxed);
}

void QtHistRegistry::Fill2(int id, double x, double y){
  if( id < 0 || id >= (int)h2.size() ) return;
  H2 & h = *h2[id];
  h.total.fetch_add(1, std::memory_order_relaxed);
  if( x < h.xLo || x >= h.xHi || y < h.yLo || y >= h.yHi ){
    h.out.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const int ix = (int)((x - h.xLo) / h.dx);
  const int iy = (int)((y - h.yLo) / h.dy);
  if( ix >= 0 && ix < h.nX && iy >= 0 && iy < h.nY ){
    h.c[(size_t)ix * h.nY + iy].fetch_add(1, std::memory_order_relaxed);
  }
}

void QtHistRegistry::Publish(){

  for( size_t i = 0; i < h1.size(); i++ ){
    H1 & h = *h1[i];
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
    const size_t n = (size_t)h.nX * h.nY;
    scratch.resize(n);
    for( size_t k = 0; k < n; k++ ) scratch[k] = h.c[k].load(std::memory_order_relaxed);
    h.w->SetCellContents(scratch.data(), h.nX, h.nY,
                         (unsigned long) h.total.load(std::memory_order_relaxed),
                         0,
                         (unsigned long) h.out.load(std::memory_order_relaxed));
    h.w->UpdatePlot();
  }
}

void QtHistRegistry::ClearCounts(){
  for( size_t i = 0; i < h1.size(); i++ ){
    H1 & h = *h1[i];
    for( int b = 0; b < h.nBin; b++ ) h.c[b].store(0, std::memory_order_relaxed);
    h.total.store(0); h.under.store(0); h.over.store(0);
  }
  for( size_t i = 0; i < h2.size(); i++ ){
    H2 & h = *h2[i];
    const size_t n = (size_t)h.nX * h.nY;
    for( size_t k = 0; k < n; k++ ) h.c[k].store(0, std::memory_order_relaxed);
    h.total.store(0); h.out.store(0);
  }
}

void QtHistRegistry::Destroy(){
  for( size_t i = 0; i < h1.size(); i++ ){ plotLayout->removeWidget(h1[i]->w); delete h1[i]->w; }
  for( size_t i = 0; i < h2.size(); i++ ){ plotLayout->removeWidget(h2[i]->w); delete h2[i]->w; }
  h1.clear();
  h2.clear();
  for( size_t i = 0; i < chanWidget.size(); i++ ){
    QWidget * line = chanWidget[i].cbDigi->parentWidget();
    ctrlLayout->removeWidget(line);
    delete line;                                  // takes both combos with it
  }
  chanWidget.clear();
  for( size_t i = 0; i < flagWidget.size(); i++ ){
    ctrlLayout->removeWidget(flagWidget[i]);
    delete flagWidget[i];
  }
  flagWidget.clear();
  flagValue.clear();
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

  //^---- layout
  QWidget * central = new QWidget(this);
  setCentralWidget(central);
  QGridLayout * top = new QGridLayout(central);

  QGroupBox * cfgBox = new QGroupBox("Control", central);
  QGridLayout * cfg = new QGridLayout(cfgBox);
  top->addWidget(cfgBox, 0, 0);

  chkEnable = new QCheckBox("Enable event building + analysis", cfgBox);
  cfg->addWidget(chkEnable, 0, 0, 1, 2);

  cfg->addWidget(new QLabel("Analysis", cfgBox), 1, 0);
  cbAnalysis = new RComboBox(cfgBox);
  cfg->addWidget(cbAnalysis, 1, 1);

  QWidget * btnRow = new QWidget(cfgBox);
  QGridLayout * btnLay = new QGridLayout(btnRow);
  btnLay->setContentsMargins(0,0,0,0);
  bnRescan  = new QPushButton("Rescan",  btnRow);
  bnRebuild = new QPushButton("Rebuild", btnRow);
  bnReload  = new QPushButton("Reload",  btnRow);
  btnLay->addWidget(bnRescan,  0, 0);
  btnLay->addWidget(bnRebuild, 0, 1);
  btnLay->addWidget(bnReload,  0, 2);
  cfg->addWidget(btnRow, 4, 0, 1, 2);

  logPane = new QPlainTextEdit(cfgBox);
  logPane->setReadOnly(true);
  logPane->setMaximumHeight(140);
  logPane->setPlaceholderText("compiler output");
  cfg->addWidget(logPane, 5, 0, 1, 2);

  cfg->addWidget(new QLabel("Time window [ns]", cfgBox), 2, 0);
  sbTimeWindow = new RSpinBox(cfgBox, 0);
  sbTimeWindow->setRange(0, 1e9);
  sbTimeWindow->setValue((double) eb->GetTimeWindow());
  cfg->addWidget(sbTimeWindow, 2, 1);

  cfg->addWidget(new QLabel("Guard time [ns]", cfgBox), 3, 0);
  sbGuardTime = new RSpinBox(cfgBox, 0);
  sbGuardTime->setRange(0, 1e10);
  sbGuardTime->setValue((double) eb->GetGuardTime());
  cfg->addWidget(sbGuardTime, 3, 1);

  ctrlBox = new QGroupBox("Analysis settings", central);
  ctrlLayout = new QVBoxLayout(ctrlBox);
  top->addWidget(ctrlBox, 1, 0);

  lbStatus = new QLabel("idle", central);
  top->addWidget(lbStatus, 2, 0);

  plotBox = new QGroupBox("Histograms", central);
  plotLayout = new QGridLayout(plotBox);
  top->addWidget(plotBox, 0, 1, 3, 1);
  top->setColumnStretch(0, 0);
  top->setColumnStretch(1, 1);

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
    UpdateStatus();
  });

  //^---- wiring
  connect(chkEnable, &QCheckBox::toggled, this, [this](bool on){
    if( !isSignalSlotActive ) return;
    SetEnabled(on);
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

  RescanPlugins();
  if( cbAnalysis->count() > 0 ) SelectAnalysis(cbAnalysis->currentText());
  UpdateStatus();
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

  if( registry ){ registry->Destroy(); delete registry; registry = nullptr; }
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

  const QString keep = cbAnalysis->currentText();
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
  }else{
    Log(QString("%1 analysis plugin(s) in %2").arg(found.size()).arg(pluginDir));
  }
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

void Analyzer::SelectAnalysis(const QString & name){

  if( registry ){ registry->Destroy(); delete registry; registry = nullptr; }
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

  registry = new QtHistRegistry(plotBox, plotLayout, ctrlBox, ctrlLayout, digi, boardIndex);

  AnalysisContext ctx;
  ctx.nBoards    = (int) boardIndex.size();
  ctx.nChannels  = nChannels.empty() ? nullptr : nChannels.data();
  ctx.timeWindow = eb->GetTimeWindow();

  analysis->Declare(*registry, ctx);   // GUI thread: this is where widgets get created

  //^---- populate and wire the channel pickers the analysis asked for
  for( size_t i = 0; i < registry->chanWidget.size(); i++ ){

    /// Copy the pointers out rather than capturing a reference into the vector.
    RComboBox * cbD = registry->chanWidget[i].cbDigi;
    RComboBox * cbC = registry->chanWidget[i].cbCh;
    int * dp = registry->chanWidget[i].digi;
    int * cp = registry->chanWidget[i].ch;

    auto refillChannels = [this, cbD, cbC](){
      const int d = cbD->currentData().toInt();
      cbC->clear();
      if( d >= 0 && d < (int)nDigi && digi[d] ){
        for( int c = 0; c < digi[d]->GetNChannels(); c++ ) cbC->addItem(QString::number(c), c);
      }
    };
    refillChannels();

    auto apply = [this, cbD, cbC, dp, cp](){
      if( !isSignalSlotActive ) return;
      const int d = cbD->currentData().toInt();
      const int c = cbC->currentData().toInt();
      /// Runs ON the analysis thread, between ProcessEvent calls. That is the fence, and it is why
      /// the analysis can keep these as plain ints with no atomics.
      QMetaObject::invokeMethod(anaWorker, [dp, cp, d, c](){
        if( dp ) *dp = d;
        if( cp ) *cp = c;
      }, Qt::BlockingQueuedConnection);
      /// Otherwise the plot silently mixes counts from the old and the new channel.
      if( registry ) registry->ClearCounts();
    };

    connect(cbD, &QComboBox::currentIndexChanged, this, [refillChannels, apply](int){
      refillChannels();
      apply();
    });
    connect(cbC, &QComboBox::currentIndexChanged, this, [apply](int){ apply(); });
    apply();
  }

  for( size_t i = 0; i < registry->flagWidget.size(); i++ ){
    QCheckBox * cb = registry->flagWidget[i];
    bool * v = registry->flagValue[i];
    connect(cb, &QCheckBox::toggled, this, [this, v](bool on){
      if( !isSignalSlotActive ) return;
      QMetaObject::invokeMethod(anaWorker, [v, on](){ if( v ) *v = on; },
                                Qt::BlockingQueuedConnection);
      if( registry ) registry->ClearCounts();
    });
  }

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
    eb->SetGuardTime((uint64_t) sbGuardTime->value());

    /// Everything is stopped here, so clearing is race-free: the fill flag has been false for the
    /// whole idle period, so no producer can be mid-push.
    for( size_t k = 0; k < boardIndex.size(); k++ ) digi[boardIndex[k]]->hitRing.clear();
    ring->Clear();
    if( registry ) registry->ClearCounts();

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

    for( size_t k = 0; k < boardIndex.size(); k++ ){
      digi[boardIndex[k]]->fillHitRing.store(false, std::memory_order_relaxed);
      digi[boardIndex[k]]->hitRing.clear();
    }
    ring->Clear();

    if( registry ) registry->Publish();      // show the final state
  }

  sbTimeWindow->setEnabled(!on);             // no marshalling needed if they only change when idle
  sbGuardTime->setEnabled(!on);
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
                    .arg(eb->totalEventsBuilt)
                    .arg(eb->totalHitsDropped)
                    .arg(eb->totalHitsLate)
                    .arg(eb->monotonicityViolations)
                    .arg(anaWorker ? anaWorker->reader.eventsDropped : 0));
}
