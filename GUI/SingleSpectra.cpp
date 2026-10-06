#include "SingleSpectra.h"

#include <QValueAxis>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QStandardItemModel>
#include <QLabel>
#include <QHBoxLayout>
#include <QRandomGenerator>
#include <QScreen>

SingleSpectra::SingleSpectra(DigiManager * digiManager, unsigned int nDigi, QString rawDataPath, QMainWindow * parent) : QMainWindow(parent){
  DebugPrint("%s", "SingleSpectra");
  this->digiManager = digiManager;
  this->nDigi = nDigi;
  this->settingPath = rawDataPath + "/HistogramSettings.txt";

  maxFillTimeinMilliSec = SingleHistogramFillingTime;

  isSignalSlotActive = true;

  // ApplySplit() sets these; splitRows < 1 after LoadSetting() means "no saved layout"
  splitRows = 0;
  splitCols = 0;
  replotDivider = 1;
  replotCounter = 0;
  suspendFilling = false;
  windowVisible = false;   // showEvent() flips it; the window is not up yet
  fillEnabled   = false;

  setWindowTitle("Single Histograms");

  //====== resize window if screen too small
  QScreen * screen = QGuiApplication::primaryScreen();
  QRect screenGeo = screen->geometry();
  if( screenGeo.width() < 1000 || screenGeo.height() < 800) {
    setGeometry(0, 0, screenGeo.width() - 100, screenGeo.height() - 100);
  }else{
    setGeometry(0, 0, 1000, 800);
  }

  QWidget * layoutWidget = new QWidget(this);
  setCentralWidget(layoutWidget);
  QVBoxLayout * layout = new QVBoxLayout(layoutWidget);
  layoutWidget->setLayout(layout);

  {//^========================
    QGroupBox * controlBox = new QGroupBox("Control", this);
    layout->addWidget(controlBox);
    QGridLayout * ctrlLayout = new QGridLayout(controlBox);
    controlBox->setLayout(ctrlLayout);

    ctrlLayout->addWidget(new QLabel("Split :", this), 0, 0);

    cbSplit = new RComboBox(this);
    cbSplit->addItem("1 x 1",  1*100 +  1);
    cbSplit->addItem("1 x 2",  1*100 +  2);
    cbSplit->addItem("2 x 2",  2*100 +  2);
    cbSplit->addItem("2 x 3",  2*100 +  3);
    cbSplit->addItem("3 x 3",  3*100 +  3);
    cbSplit->addItem("4 x 4",  4*100 +  4);
    ctrlLayout->addWidget(cbSplit, 0, 1);
    connect( cbSplit, &RComboBox::currentIndexChanged, this, [=](int index){
      if( !isSignalSlotActive ) return;
      int code = cbSplit->itemData(index).toInt();
      ApplySplit(code / 100, code % 100);
    });

    QPushButton * bnClearHist = new QPushButton("Clear All Hist.", this);
    ctrlLayout->addWidget(bnClearHist, 0, 2, 1, 2);
    connect(bnClearHist, &QPushButton::clicked, this, [=](){
      /// Counters first: the widgets are only a view of them now, so clearing a widget without
      /// clearing its counters would be undone by the very next publish.
      ClearAllCounts();
      for( unsigned int i = 0; i < nDigi; i++){
        for( int j = 0; j < digiManager->GetNChannels(i); j++){
          if( hist[i][j] ) hist[i][j]->Clear();
        }
        if( hist2D[i] ) hist2D[i]->Clear();
      }
    });


    chkIsFillHistogram = new QCheckBox("Fill Histograms", this);
    ctrlLayout->addWidget(chkIsFillHistogram, 0, 4, 1, 2);
    chkIsFillHistogram->setChecked(false);
    isFillingHistograms = false;
    /// The worker cannot read checkState() itself -- QWidget accessors are GUI-thread only -- so
    /// mirror it. There was no connection here at all before; the worker just read the widget.
    fillEnabled = false;
    connect(chkIsFillHistogram, &QCheckBox::toggled, this, [=](bool on){
      /// Resuming must not replay the backlog. While the box was off the ADC thread kept pushing
      /// into the ring, so lastFilledIndex sat wherever the last sync left it -- potentially a
      /// whole run behind the live end. The first pass after enabling then drained up to a full
      /// RingBufferSize per channel in one burst: "check the box and the histogram suddenly
      /// fills a lot of data". Jump the cursor to the live end first, under the same fence the
      /// pane/rebin paths use. Counts are NOT cleared -- the plot keeps what it had.
      if( on ){
        suspendFilling = true;
        WaitForFillToDrain();
        ClearInternalDataCount();
        suspendFilling = false;
      }
      fillEnabled = on;
    });

    //^---- replot throttle : the fill timer stays at maxFillTimeinMilliSec,
    //^     only the number of replots per fill is reduced.
    chkAutoRefresh = new QCheckBox("Auto Refresh", this);
    chkAutoRefresh->setChecked(true);
    chkAutoRefresh->setToolTip("Pick the replot period from the number and type of displayed plots.");
    ctrlLayout->addWidget(chkAutoRefresh, 0, 6);
    connect(chkAutoRefresh, &QCheckBox::stateChanged, this, [=](int){
      if( !isSignalSlotActive ) return;
      UpdateRefreshRate();
    });

    ctrlLayout->addWidget(new QLabel("Refresh [ms] :", this), 0, 7);

    sbRefresh = new RSpinBox(this, 0);
    sbRefresh->setRange(maxFillTimeinMilliSec, 5000);
    sbRefresh->setSingleStep(maxFillTimeinMilliSec);
    sbRefresh->setValue(maxFillTimeinMilliSec);
    sbRefresh->setEnabled(false); // Auto is on by default
    sbRefresh->SetToolTip();
    ctrlLayout->addWidget(sbRefresh, 0, 8);
    connect(sbRefresh, &RSpinBox::valueChanged, this, [=](double){
      if( !isSignalSlotActive ) return;
      UpdateRefreshRate();
    });

    lbEffRefresh = new QLabel(this);
    lbEffRefresh->setStyleSheet("color:blue;");
    lbEffRefresh->setToolTip("Actual replot period. It is quantised to multiples of the fill period.");
    ctrlLayout->addWidget(lbEffRefresh, 0, 9, 1, 2);

    QLabel * lbSettingPath = new QLabel( settingPath , this);
    ctrlLayout->addWidget(lbSettingPath, 1, 0, 1, 9);

    QPushButton * bnSaveButton = new QPushButton("Save Hist. Settings", this);
    ctrlLayout->addWidget(bnSaveButton, 1, 9, 1, 2);
    connect(bnSaveButton, &QPushButton::clicked, this, &SingleSpectra::SaveSetting);

  }

  {//^========================
    // null / false the full arrays: the pane code indexes them by combo position,
    // which is only bounded by MaxNumberOfDigitizer, not by nDigi.
    for( unsigned int i = 0; i < MaxNumberOfDigitizer; i++ ) {
      hist2D[i] = nullptr;
      hist2DVisibility[i] = false;
      for( int j = 0; j < MaxNumberOfChannel ; j++ ) {
        hist[i][j] = nullptr;
        histVisibility[i][j] = false;
      }
    }

    histBox = new QGroupBox("Histograms", this);
    layout->addWidget(histBox);
    histLayout = new QGridLayout(histBox);
    histBox->setLayout(histLayout);

    double eMax = 5000;
    double eMin = 0;
    double nBin = 200;

    for( unsigned int i = 0; i < nDigi; i++){
      for( int j = 0; j < digiManager->GetNChannels(i); j++){
        hist[i][j] = new Histogram1D("Digi-" + QString::number(digiManager->GetSerialNumber(i)) +", Ch-" +  QString::number(j), "Raw Energy [ch]", nBin, eMin, eMax);
        if( digiManager->GetFPGAType(i) == DPPType::PSD ){
          hist[i][j]->AddDataList("Short Energy", Qt::green);
        }
        connect(hist[i][j], &Histogram1D::AboutToRebin, this, &SingleSpectra::BeginHistogramEdit);
        connect(hist[i][j], &Histogram1D::ReBinned,     this, &SingleSpectra::EndHistogramEdit);
      }
      hist2D[i] = new Histogram2D("Digi-" + QString::number(digiManager->GetSerialNumber(i)), "Channel", "Raw Energy [ch]", digiManager->GetNChannels(i), 0, digiManager->GetNChannels(i), nBin, eMin, eMax);
      hist2D[i]->SetChannelMap(true, digiManager->GetNChannels(i) < 20  ? 1 : 4);
      hist2D[i]->Rebin(digiManager->GetNChannels(i), -0.5, digiManager->GetNChannels(i)+0.5, nBin, eMin, eMax);
      connect(hist2D[i], &Histogram2D::AboutToRebin, this, &SingleSpectra::BeginHistogramEdit);
      connect(hist2D[i], &Histogram2D::ReBinned,     this, &SingleSpectra::EndHistogramEdit);
    }

    BuildPanes();

    LoadSetting(); // may call ApplySplit() / SetPaneSource() from its PANEL section

    if( splitRows < 1 ){
      // no saved layout (first run, or a settings file from before the split grid)
      // : fall back to the old single 2D overview of digi-0
      ApplySplit(1, 1, false);
      suspendFilling = true;
      SetPaneSource(0, 0, digiManager->GetNChannels(0));
      suspendFilling = false;
    }else{
      // a saved layout, but none of its sources resolved - e.g. the digitizers were
      // swapped since. Do not leave the user with a grid of empty panes.
      bool anyShown = false;
      for( int p = 0; p < NumberOfPanes(); p++ ) if( pane[p].digiID >= 0 ){ anyShown = true; break; }
      if( !anyShown ) ApplySplit(splitRows, splitCols, true);
    }

    RefreshComboEnables();
    UpdateVisibilityFlags();
    UpdateRefreshRate();

    /// After LoadSetting(), because its Rebin() calls are what decide the final binning. The
    /// worker thread is not started until the end of this constructor, so no fence is needed.
    for( unsigned int i = 0; i < nDigi; i++ ){
      for( int ch = 0; ch < digiManager->GetNChannels(i); ch++ ) ResizeCounts1D(i, ch);
      ResizeCounts2D(i);
    }
  }

  layout->setStretch(0, 1);
  layout->setStretch(1, 6);

  ClearInternalDataCount();


  workerThread = new QThread(this);
  histWorker = new HistWorker(this);
  timer = new QTimer(this);

  histWorker->moveToThread(workerThread);

  // this is another way
  // timer = new QTimer();
  // timer->moveToThread(workerThread);
  // connect(this, &SingleSpectra::startWorkerTimer, timer, static_cast<void(QTimer::*)(int)>(&QTimer::start));
  // connect(this, &SingleSpectra::stopWorkerTimer, timer, &QTimer::stop);

  isFillingHistograms = false;
  connect(timer, &QTimer::timeout, histWorker, &HistWorker::FillHistograms);
  connect( histWorker, &HistWorker::workDone, this, &SingleSpectra::ReplotHistograms);

  workerThread->start();

}

SingleSpectra::~SingleSpectra(){
  DebugPrint("%s", "SingleSpectra");

  timer->stop();

  if( workerThread->isRunning() ){
    workerThread->quit();
    workerThread->wait();
  }

  SaveSetting();

  for( unsigned int i = 0; i < MaxNumberOfDigitizer; i++ ){
    for( int ch = 0; ch < MaxNumberOfChannel; ch++){
      delete hist[i][ch];
      hist[i][ch] = nullptr;
    }
    delete hist2D[i];
    hist2D[i] = nullptr;
  }
}

void SingleSpectra::ClearInternalDataCount(){
  DebugPrint("%s", "SingleSpectra");
  for( unsigned int i = 0; i < nDigi; i++){
    for( int ch = 0; ch < digiManager->GetNChannels(i) ; ch++) {
      lastFilledIndex[i][ch] = digiManager->GetRingBuffer(i, ch).index();
    }
  }
}

//^#======================================================== counters
//^ See the note on the H1Counts/H2Counts declarations: the worker touches only these, never a
//^ widget. Every function here except AddCount1D/AddCount2D is GUI thread only, and the resize
//^ ones additionally need the suspendFilling fence, because they reallocate under the worker.

void SingleSpectra::ResizeCounts1D(int d, int ch){
  if( d < 0 || d >= (int)MaxNumberOfDigitizer || ch < 0 || ch >= MaxNumberOfChannel ) return;
  Histogram1D * w = hist[d][ch];
  if( w == nullptr ) return;

  H1Counts & h = c1[d][ch];
  h.nBin    = w->GetNBin();
  h.lo      = w->GetXMin();
  h.hi      = w->GetXMax();
  h.dx      = ( h.nBin > 0 ) ? (h.hi - h.lo)/h.nBin : 1;
  h.nSeries = ( d < (int)nDigi && digiManager->GetFPGAType(d) == DPPType::PSD ) ? 2 : 1;

  for( int s = 0; s < 2; s++ ){
    if( s < h.nSeries && h.nBin > 0 ){
      h.c[s].reset(new std::atomic<uint32_t>[h.nBin]);
      for( int b = 0; b < h.nBin; b++ ) h.c[s][b].store(0, std::memory_order_relaxed);
    }else{
      h.c[s].reset();
    }
  }
  h.total.store(0); h.under.store(0); h.over.store(0);
}

void SingleSpectra::ResizeCounts2D(int d){
  if( d < 0 || d >= (int)MaxNumberOfDigitizer ) return;
  if( hist2D[d] == nullptr ) return;

  H2Counts & h = c2[d];
  h.g = hist2D[d]->GetCellGeom();
  const size_t n = (size_t)h.g.nx * h.g.ny;
  if( n > 0 ){
    h.c.reset(new std::atomic<uint32_t>[n]);
    for( size_t k = 0; k < n; k++ ) h.c[k].store(0, std::memory_order_relaxed);
  }else{
    h.c.reset();
  }
  h.total.store(0); h.out.store(0);
}

void SingleSpectra::ClearAllCounts(){
  for( unsigned int i = 0; i < MaxNumberOfDigitizer; i++ ){
    for( int ch = 0; ch < MaxNumberOfChannel; ch++ ){
      H1Counts & h = c1[i][ch];
      for( int s = 0; s < 2; s++ ){
        if( !h.c[s] ) continue;
        for( int b = 0; b < h.nBin; b++ ) h.c[s][b].store(0, std::memory_order_relaxed);
      }
      h.total.store(0); h.under.store(0); h.over.store(0);
    }
    H2Counts & g = c2[i];
    if( g.c ){
      const size_t n = (size_t)g.g.nx * g.g.ny;
      for( size_t k = 0; k < n; k++ ) g.c[k].store(0, std::memory_order_relaxed);
    }
    g.total.store(0); g.out.store(0);
  }
}

void SingleSpectra::AddCount1D(int d, int ch, int series, double v){
  H1Counts & h = c1[d][ch];
  if( series >= h.nSeries || !h.c[series] ) return;

  /// Series 0 owns the total/under/over counters, exactly as Histogram1D::Fill() does for ID 0,
  /// so the stat labels keep reading the same as before the port.
  if( series == 0 ){
    h.total.fetch_add(1, std::memory_order_relaxed);
    if( v <  h.lo ){ h.under.fetch_add(1, std::memory_order_relaxed); return; }
    if( v >= h.hi ){ h.over .fetch_add(1, std::memory_order_relaxed); return; }
  }else{
    if( v < h.lo || v >= h.hi ) return;
  }

  const int b = (int)((v - h.lo)/h.dx);
  if( b >= 0 && b < h.nBin ) h.c[series][b].fetch_add(1, std::memory_order_relaxed);
}

void SingleSpectra::AddCount2D(int d, int ch, double e){
  H2Counts & h = c2[d];
  if( !h.c ) return;
  h.total.fetch_add(1, std::memory_order_relaxed);
  int ix = 0, iy = 0;
  if( !Histogram2D::CoordToCell(h.g, (double)ch, e, &ix, &iy) ){
    h.out.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  h.c[(size_t)ix * h.g.ny + iy].fetch_add(1, std::memory_order_relaxed);
}

void SingleSpectra::PublishHist1D(int d, int ch){
  if( hist[d][ch] == nullptr ) return;
  H1Counts & h = c1[d][ch];
  if( h.nBin <= 0 ) return;

  scratch.resize(h.nBin);
  const unsigned long tot = (unsigned long) h.total.load(std::memory_order_relaxed);
  const unsigned long un  = (unsigned long) h.under.load(std::memory_order_relaxed);
  const unsigned long ov  = (unsigned long) h.over .load(std::memory_order_relaxed);

  /// Ascending series order matters: SetBinContents only resets yMax on series 0 and grows it
  /// from there, so the axis ends up covering both the long and the short gate.
  for( int s = 0; s < h.nSeries; s++ ){
    if( !h.c[s] ) continue;
    for( int b = 0; b < h.nBin; b++ ) scratch[b] = h.c[s][b].load(std::memory_order_relaxed);
    hist[d][ch]->SetBinContents(scratch.data(), h.nBin, tot, un, ov, s);
  }
}

void SingleSpectra::PublishHist2D(int d){
  if( hist2D[d] == nullptr ) return;
  H2Counts & h = c2[d];
  if( !h.c ) return;

  const size_t n = (size_t)h.g.nx * h.g.ny;
  scratch.resize(n);
  for( size_t k = 0; k < n; k++ ) scratch[k] = h.c[k].load(std::memory_order_relaxed);

  /// Histogram2D::Fill() counted only in-plot hits into the centre box, so subtract the rejects
  /// to keep that number meaning what it has always meant here.
  const unsigned long tot = (unsigned long) h.total.load(std::memory_order_relaxed);
  const unsigned long out = (unsigned long) h.out.load(std::memory_order_relaxed);
  hist2D[d]->SetCellContents(scratch.data(), h.g.nx, h.g.ny, tot > out ? tot - out : 0, 0, out);
}

//^#======================================================== the split grid

void SingleSpectra::BuildPanes(){
  DebugPrint("%s", "SingleSpectra");

  QFont smallFont = font();
  smallFont.setPointSize( qMax(7, smallFont.pointSize() - 1) );

  for( int p = 0; p < MaxNumberOfPane; p++ ){

    pane[p].digiID      = -1;
    pane[p].chID        = -1;
    pane[p].savedDigiID = -1;
    pane[p].savedChID   = -1;

    pane[p].box = new QWidget(histBox);
    pane[p].box->hide();

    pane[p].boxLayout = new QVBoxLayout(pane[p].box);
    pane[p].boxLayout->setContentsMargins(0, 0, 0, 0);
    pane[p].boxLayout->setSpacing(2);

    //---- header : the two source combos
    QWidget * header = new QWidget(pane[p].box);
    QHBoxLayout * headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);

    pane[p].cbDigi = new RComboBox(header);
    pane[p].cbDigi->setFont(smallFont);
    // do not let a long serial number force the whole 4x4 grid wide
    pane[p].cbDigi->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    pane[p].cbDigi->setMinimumContentsLength(7);
    pane[p].cbDigi->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for( unsigned int i = 0; i < nDigi; i++) pane[p].cbDigi->addItem("Digi-" + QString::number( digiManager->GetSerialNumber(i) ), i);
    headerLayout->addWidget(pane[p].cbDigi);

    pane[p].cbCh = new RComboBox(header);
    pane[p].cbCh->setFont(smallFont);
    pane[p].cbCh->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    pane[p].cbCh->setMinimumContentsLength(6);
    pane[p].cbCh->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerLayout->addWidget(pane[p].cbCh);

    pane[p].boxLayout->addWidget(header); // layout index 0

    //---- placeholder, occupies layout index 1 while the pane has no source
    pane[p].placeholder = new QLabel("- empty -", pane[p].box);
    pane[p].placeholder->setAlignment(Qt::AlignCenter);
    pane[p].placeholder->setStyleSheet("color:gray; border: 1px dashed gray;");
    pane[p].boxLayout->addWidget(pane[p].placeholder, 1);

    RebuildChannelCombo(p);

    connect( pane[p].cbDigi, &RComboBox::currentIndexChanged, this, [=](int){
      if( !isSignalSlotActive ) return;
      RebuildChannelCombo(p); // channel count differs between digitizers
      PaneSelectionChanged(p);
    });

    connect( pane[p].cbCh, &RComboBox::currentIndexChanged, this, [=](int){
      if( !isSignalSlotActive ) return;
      PaneSelectionChanged(p);
    });
  }
}

void SingleSpectra::RebuildChannelCombo(int p){
  if( p < 0 || p >= MaxNumberOfPane ) return;

  bool oldFlag = isSignalSlotActive;
  isSignalSlotActive = false;

  int d = pane[p].cbDigi->currentIndex();
  int wantCh = pane[p].cbCh->count() > 0 ? pane[p].cbCh->currentData().toInt() : -1;

  pane[p].cbCh->clear();
  pane[p].cbCh->addItem("None", -1);
  if( d >= 0 && d < (int)nDigi ){
    int nCh = digiManager->GetNChannels(d);
    pane[p].cbCh->addItem("All Ch", nCh); // the 2D overview of this digitizer
    for( int i = 0; i < nCh; i++) pane[p].cbCh->addItem("ch-" + QString::number(i), i);
  }

  int index = pane[p].cbCh->findData(wantCh);
  pane[p].cbCh->setCurrentIndex( index >= 0 ? index : 0 );

  isSignalSlotActive = oldFlag;
}

void SingleSpectra::WaitForFillToDrain(){
  // suspendFilling must already be set; FillHistograms() then backs off at its next entry,
  // and also at its next channel -- see the loop conditions there.
  int guard = 0;
  while( isFillingHistograms && guard++ < 200 ) QThread::msleep(1);
}

void SingleSpectra::BeginHistogramEdit(){
  suspendFilling = true;
  WaitForFillToDrain();
}

void SingleSpectra::EndHistogramEdit(){

  /// Re-sync the counters to the binning the widget has just moved to, before letting the worker
  /// back in. sender() identifies which plot rebinned -- the counters are per histogram, and one
  /// left on the old binning would be published against the new axis.
  QObject * s = sender();
  bool done = false;
  for( unsigned int i = 0; i < nDigi && !done; i++ ){
    if( s == hist2D[i] ){ ResizeCounts2D(i); break; }
    for( int ch = 0; ch < digiManager->GetNChannels(i); ch++ ){
      if( s == hist[i][ch] ){ ResizeCounts1D(i, ch); done = true; break; }
    }
  }

  suspendFilling = false;
}

void SingleSpectra::SetPaneSource(int p, int d, int ch){
  if( p < 0 || p >= MaxNumberOfPane ) return;

  //---- normalise the request
  if( d < 0 || d >= (int)nDigi ){
    d  = -1;
    ch = -1;
  }else{
    int nCh = digiManager->GetNChannels(d);
    if( ch < 0 || ch > nCh ) ch = -1;   // ch == nCh is the 2D overview
    if( ch < 0 ) d = -1;
  }

  //---- a Histogram1D/2D is the widget that owns its data, so it can only live in
  //---- one pane. The combos grey out taken sources, this guards the other callers.
  if( d >= 0 ){
    for( int q = 0; q < MaxNumberOfPane; q++ ){
      if( q == p ) continue;
      if( pane[q].digiID == d && pane[q].chID == ch ){
        d  = -1;
        ch = -1;
        break;
      }
    }
  }

  //---- unmount whatever sits at layout index 1 (a plot, or the placeholder)
  QLayoutItem * item = pane[p].boxLayout->takeAt(1);
  if( item ){
    QWidget * w = item->widget();
    if( w ){
      // stay a (hidden, un-laid-out) child of this box so it keeps an owner;
      // mounting it elsewhere re-parents it again
      w->setParent(pane[p].box);
      w->hide();
    }
    delete item; // the layout item, not the widget
  }

  //---- mount the new one
  QWidget * newWidget = nullptr;
  if( d >= 0 ){
    newWidget = ( ch == digiManager->GetNChannels(d) ) ? static_cast<QWidget*>(hist2D[d])
                                                  : static_cast<QWidget*>(hist[d][ch]);
  }
  if( newWidget == nullptr ){
    newWidget = pane[p].placeholder;
    d  = -1;
    ch = -1;
  }

  newWidget->setParent(pane[p].box);
  pane[p].boxLayout->insertWidget(1, newWidget, 1);
  newWidget->show();

  pane[p].digiID = d;
  pane[p].chID   = ch;
  if( d >= 0 ){
    pane[p].savedDigiID = d;
    pane[p].savedChID   = ch;
  }

  //---- reflect the result back into the combos (also shows a refused request)
  bool oldFlag = isSignalSlotActive;
  isSignalSlotActive = false;
  if( d >= 0 && pane[p].cbDigi->currentIndex() != d ){
    pane[p].cbDigi->setCurrentIndex(d);
    RebuildChannelCombo(p);
  }
  int index = pane[p].cbCh->findData(ch);
  pane[p].cbCh->setCurrentIndex( index >= 0 ? index : 0 );
  isSignalSlotActive = oldFlag;

  /// Here, at the single choke point for "what a pane shows", rather than only in the three
  /// callers that happen to do it today. These flags now gate the fill, so a caller that forgot
  /// would silently stop a channel accumulating -- a much worse failure than a redundant refresh.
  UpdateVisibilityFlags();
}

void SingleSpectra::PaneSelectionChanged(int p){
  suspendFilling = true;
  WaitForFillToDrain();

  SetPaneSource(p, pane[p].cbDigi->currentIndex(), pane[p].cbCh->currentData().toInt());

  suspendFilling = false;

  RefreshComboEnables();
  UpdateVisibilityFlags();
  UpdateRefreshRate();
}

void SingleSpectra::ApplySplit(int rows, int cols, bool autoAssign){
  DebugPrint("%s", "SingleSpectra");

  // clamp : cbSplit only offers sane values, a hand-edited settings file may not
  rows = qBound(1, rows, 4);
  cols = qBound(1, cols, 4);

  int nPane = rows * cols;

  suspendFilling = true;
  WaitForFillToDrain();

  //---- panes that go out of view release their source, so the channels they held
  //---- become selectable again. The selection itself is remembered in savedDigiID.
  for( int p = nPane; p < MaxNumberOfPane; p++ ) SetPaneSource(p, -1, -1);

  //---- rebuild the grid
  for( int p = 0; p < MaxNumberOfPane; p++ ){
    histLayout->removeWidget(pane[p].box);
    pane[p].box->hide();
  }
  for( int i = 0; i < 4; i++ ){
    histLayout->setRowStretch(i, 0);
    histLayout->setColumnStretch(i, 0);
  }
  for( int p = 0; p < nPane; p++ ){
    histLayout->addWidget(pane[p].box, p / cols, p % cols);
    pane[p].box->show();
  }
  for( int r = 0; r < rows; r++ ) histLayout->setRowStretch(r, 1);
  for( int c = 0; c < cols; c++ ) histLayout->setColumnStretch(c, 1);

  splitRows = rows;
  splitCols = cols;

  //---- give newly visible panes something to show
  if( autoAssign ){
    for( int p = 0; p < nPane; p++ ){
      if( pane[p].digiID >= 0 ) continue;

      int d  = pane[p].savedDigiID;
      int ch = pane[p].savedChID;

      SetPaneSource(p, d, ch);           // restores the remembered pick if still free
      if( pane[p].digiID >= 0 ) continue;

      // otherwise take the first unclaimed channel
      bool found = false;
      for( unsigned int i = 0; i < nDigi && !found; i++){
        for( int j = 0; j < digiManager->GetNChannels(i) && !found; j++){
          bool taken = false;
          for( int q = 0; q < MaxNumberOfPane && !taken; q++ ){
            if( q == p ) continue;
            if( pane[q].digiID == (int)i && pane[q].chID == j ) taken = true;
          }
          if( taken ) continue;
          SetPaneSource(p, i, j);
          found = true;
        }
      }
    }
  }

  suspendFilling = false;

  //---- keep cbSplit in step when the split came from a file rather than the combo
  bool oldFlag = isSignalSlotActive;
  isSignalSlotActive = false;
  int index = cbSplit->findData(rows * 100 + cols);
  if( index >= 0 ) cbSplit->setCurrentIndex(index);
  isSignalSlotActive = oldFlag;

  RefreshComboEnables();
  UpdateVisibilityFlags();
  UpdateRefreshRate();
}

void SingleSpectra::RefreshComboEnables(){
  // same idea as Scope::ProbeChange() : grey out what another pane already shows
  int nPane = NumberOfPanes();

  for( int p = 0; p < nPane; p++ ){
    QStandardItemModel * model = qobject_cast<QStandardItemModel*>(pane[p].cbCh->model());
    if( model == nullptr ) continue;

    int d = pane[p].cbDigi->currentIndex();

    for( int r = 0; r < pane[p].cbCh->count(); r++){
      int chData = pane[p].cbCh->itemData(r).toInt();

      bool taken = false;
      if( chData >= 0 && d >= 0 ){ // the "None" row is always selectable
        for( int q = 0; q < nPane && !taken; q++ ){
          if( q == p ) continue;
          if( pane[q].digiID == d && pane[q].chID == chData ) taken = true;
        }
      }
      model->item(r)->setEnabled(!taken);
    }
  }
}

void SingleSpectra::UpdateVisibilityFlags(){
  for( unsigned int i = 0; i < MaxNumberOfDigitizer; i++){
    hist2DVisibility[i] = false;
    for( int j = 0; j < MaxNumberOfChannel; j++) histVisibility[i][j] = false;
  }

  int nPane = NumberOfPanes();
  for( int p = 0; p < nPane; p++ ){
    int d  = pane[p].digiID;
    int ch = pane[p].chID;
    if( d < 0 || ch < 0 ) continue;
    if( ch == digiManager->GetNChannels(d) ){
      hist2DVisibility[d] = true;
    }else{
      histVisibility[d][ch] = true;
    }
  }
}

void SingleSpectra::UpdateRefreshRate(){
  int nPane = NumberOfPanes();

  int n1D = 0, n2D = 0;
  for( int p = 0; p < nPane; p++ ){
    int d  = pane[p].digiID;
    int ch = pane[p].chID;
    if( d < 0 || ch < 0 ) continue;
    if( ch == digiManager->GetNChannels(d) ) n2D++; else n1D++;
  }

  int cost = n1D + 2*n2D; // a colormap costs about twice a 1D graph to redraw
  int autoMs = maxFillTimeinMilliSec * qMax(1, (cost + 1)/2);

  bool isAuto = chkAutoRefresh->isChecked();
  int targetMs = isAuto ? autoMs : (int)sbRefresh->value();

  replotDivider = qMax(1, (int)qRound((double)targetMs / maxFillTimeinMilliSec));
  replotCounter = 0;

  bool oldFlag = isSignalSlotActive;
  isSignalSlotActive = false;
  sbRefresh->setEnabled(!isAuto);
  if( isAuto ) sbRefresh->setValue(autoMs);
  isSignalSlotActive = oldFlag;

  int effMs = replotDivider * maxFillTimeinMilliSec;
  lbEffRefresh->setText( QString("Replot: %1 ms (%2 plot%3)").arg(effMs).arg(n1D + n2D).arg(n1D + n2D == 1 ? "" : "s") );
}

void SingleSpectra::FillHistograms(){

  /// Atomic mirrors, not isVisible() and checkState(): this runs on the worker thread and QWidget
  /// accessors are not safe to call from one. The GUI writes them, see showEvent/hideEvent and
  /// the chkIsFillHistogram connection.
  if( !windowVisible ) return;
  if( !fillEnabled ) return;
  if( suspendFilling ) return;

  // Claim the fill, then re-check: the GUI thread sets suspendFilling and only then
  // waits for isFillingHistograms to clear, so with both seq_cst one of the two sees
  // the other and we never fill into a plot that is being re-parented.
  bool expected = false;
  if( !isFillingHistograms.compare_exchange_strong(expected, true) ) return;
  if( suspendFilling ){
    isFillingHistograms = false;
    return;
  }
  // printf("####################### SingleSpectra::%s\n", __func__);
  // qDebug() << __func__ << "| thread:" << QThread::currentThreadId();

  long totalFilled = 0;
  bool timeout = false;

  /// One pass is budgeted. Without this a single pass can drain 20 boards x 64 channels x a full
  /// ring while the timer keeps queueing the next one into this thread's event loop.
  QElapsedTimer budget;
  budget.start();

  /// suspendFilling is re-tested per channel, not only at entry. WaitForFillToDrain() gives up
  /// after 200 ms, and one pass can take far longer than that at rate -- so without this the GUI
  /// thread would sail past the fence and rebin under a live fill. Both loops fall through to the
  /// isFillingHistograms reset below, which is what the waiter is actually watching.
  for( int ID = 0; ID < (int)nDigi && !timeout && !suspendFilling; ID++){
    bool isPSD = digiManager->GetFPGAType(ID) == DPPType::PSD;
    for( int ch = 0; ch < digiManager->GetNChannels(ID) && !timeout && !suspendFilling; ch++){

      /// Only what is on screen. The 2D term is not redundant: hist2D[ID] is fed by every channel
      /// of the board, so a channel still has to be drained when its board's overview is mounted
      /// even though its own 1D plot is not.
      ///
      /// lastFilledIndex is deliberately left behind for a skipped channel. That is what makes the
      /// lap check below jump straight to recent data when the plot comes back, so a newly shown
      /// pane starts with up to a ring of history rather than empty.
      if( !histVisibility[ID][ch].load(std::memory_order_relaxed) &&
          !hist2DVisibility[ID].load(std::memory_order_relaxed) ) continue;

      long absIndex = digiManager->GetRingBuffer(ID, ch).index();
      long gap = absIndex - lastFilledIndex[ID][ch];

      if( gap <= 0 ) continue;

      // if gap is bigger than ring buffer, skip the old data
      if( gap > (long)digiManager->GetRingBuffer(ID, ch).size() ) {
        lastFilledIndex[ID][ch] = absIndex - digiManager->GetRingBuffer(ID, ch).size();
        gap = digiManager->GetRingBuffer(ID, ch).size();
      }

      long filled = 0;
      while( lastFilledIndex[ID][ch] < absIndex ){
        HitSummary hs = digiManager->GetRingBuffer(ID, ch).at(lastFilledIndex[ID][ch]);
        lastFilledIndex[ID][ch] ++;

        AddCount1D(ID, ch, 0, hs.energy);
        if( isPSD ) AddCount1D(ID, ch, 1, hs.energy_short);
        AddCount2D(ID, ch, hs.energy);
        filled++;

        if( (filled & 0xFF) == 0 && budget.elapsed() >= (qint64) maxFillTimeinMilliSec ){
          timeout = true;
          break;
        }
      }

      totalFilled += filled;
      // printf("Digi-%2d ch-%2d | event filled %ld / %ld\n", ID, ch, filled, gap);
    }
  }
  (void) totalFilled;

  isFillingHistograms = false;

}

void SingleSpectra::ReplotHistograms(){

  // qDebug() << __func__ << "| thread:" << QThread::currentThreadId();

  // The fill timer runs at maxFillTimeinMilliSec; redrawing every visible plot at that
  // rate saturates the GUI thread once the grid gets big, so only replot every
  // replotDivider-th fill. See UpdateRefreshRate().
  replotCounter++;
  if( replotDivider > 1 && (replotCounter % replotDivider) != 0 ) return;

  /// Counts -> widgets happens here and nowhere else. The fill worker only increments the atomic
  /// arrays; every QCustomPlot touch is on this thread. Only mounted panes are published, which
  /// is also the only set the worker bothered to fill.
  int nPane = NumberOfPanes();
  for( int p = 0; p < nPane; p++ ){
    int ID = pane[p].digiID;
    int ch = pane[p].chID;
    if( ID < 0 || ch < 0 ) continue;

    if( ch == digiManager->GetNChannels(ID) ){
      if( hist2D[ID] ){
        PublishHist2D(ID);
        hist2D[ID]->UpdatePlot();
      }
    }else{
      if( hist[ID][ch] ){
        PublishHist1D(ID, ch);
        hist[ID][ch]->UpdatePlot();
      }
    }
  }

}

void SingleSpectra::showEvent(QShowEvent * event){
  QMainWindow::showEvent(event);
  windowVisible = true;
}

void SingleSpectra::hideEvent(QHideEvent * event){
  QMainWindow::hideEvent(event);
  windowVisible = false;
}

void SingleSpectra::SaveSetting(){
  DebugPrint("%s", "SingleSpectra");

  QFile file(settingPath );

  if (!file.exists()) {
    // If the file does not exist, create it
    if (!file.open(QIODevice::WriteOnly)) {
      qWarning() << "Could not create file" << settingPath;
    } else {
      qDebug() << "File" << settingPath  << "created successfully";
      file.close();
    }
  }

  if( file.open(QIODevice::Text | QIODevice::WriteOnly) ){

    for( unsigned int i = 0; i < nDigi; i++){
      file.write(("======= " + QString::number(digiManager->GetSerialNumber(i)) + "\n").toStdString().c_str());
      for( int ch = 0; ch < digiManager->GetNChannels(i) ; ch++){
        QString a = QString::number(ch).rightJustified(2, ' ');
        QString b = QString::number(hist[i][ch]->GetNBin()).rightJustified(6, ' ');
        QString c = QString::number(hist[i][ch]->GetXMin()).rightJustified(6, ' ');
        QString d = QString::number(hist[i][ch]->GetXMax()).rightJustified(6, ' ');
        file.write( QString("%1 %2 %3 %4\n").arg(a).arg(b).arg(c).arg(d).toStdString().c_str() );
      }

      QString a = QString::number(digiManager->GetNChannels(i)).rightJustified(2, ' ');
      QString b = QString::number(hist2D[i]->GetXNBin()-2).rightJustified(6, ' ');
      QString c = QString::number(hist2D[i]->GetXMin()).rightJustified(6, ' ');
      QString d = QString::number(hist2D[i]->GetXMax()).rightJustified(6, ' ');
      QString e = QString::number(hist2D[i]->GetYNBin()-2).rightJustified(6, ' ');
      QString f = QString::number(hist2D[i]->GetYMin()).rightJustified(6, ' ');
      QString g = QString::number(hist2D[i]->GetYMax()).rightJustified(6, ' ');
      file.write( QString("%1 %2 %3 %4 %5 %6 %7\n").arg(a).arg(b).arg(c).arg(d).arg(e).arg(f).arg(g).toStdString().c_str() );
    }

    //^---- the split-grid layout. Sources are keyed by serial number, like the
    //^     sections above, so they survive a change of digitizer enumeration order.
    file.write("####### PANEL\n");
    file.write( QString("ROWS_COLS %1 %2\n").arg(splitRows, 4).arg(splitCols, 4).toStdString().c_str() );
    file.write( QString("REFRESH   %1 %2\n").arg(chkAutoRefresh->isChecked() ? 1 : 0, 4)
                                            .arg((int)sbRefresh->value(), 4).toStdString().c_str() );

    for( int p = 0; p < NumberOfPanes(); p++ ){
      int sn = pane[p].digiID >= 0 ? digiManager->GetSerialNumber(pane[p].digiID) : -1;
      file.write( QString("PANE      %1 %2 %3\n").arg(p, 4).arg(sn, 8).arg(pane[p].chID, 4).toStdString().c_str() );
    }

    file.write("##========== End of file\n");
    file.close();

    printf("Saved Histogram Settings to %s\n", settingPath.toStdString().c_str());
  }else{
    printf("%s|cannot open HistogramSettings.txt\n", __func__);
  }

}

void SingleSpectra::LoadSetting(){
  DebugPrint("%s", "SingleSpectra");

  QFile file(settingPath);

  if( file.open(QIODevice::Text | QIODevice::ReadOnly) ){

    QTextStream in(&file);
    QString line = in.readLine();

    int digiSN = 0;
    int digiID = -1;
    bool inPanelSection = false;

    while ( !line.isNull() ){
      if( line.contains("##========== ") ) break;
      if( line.contains("//") ){
        line = in.readLine(); // must advance, or this loop hangs the GUI
        continue;
      }

      if( line.contains("####### PANEL") ){
        inPanelSection = true;
        digiID = -1;
        line = in.readLine();
        continue;
      }

      if( line.contains("======= ") ){
        inPanelSection = false;
        digiSN = line.mid(7).toInt();

        digiID = -1;
        for( unsigned int i = 0; i < nDigi; i++){
          if( digiSN == (int)digiManager->GetSerialNumber(i) ) {
            digiID = i;
            break;
          }
        }
        line = in.readLine();
        continue;
      }

      QStringList list = line.split(QRegularExpression("\\s+"));
      list.removeAll("");

      if( inPanelSection ){
        //^---- the split-grid layout, written by SaveSetting()
        if( list.count() == 3 && list[0] == "ROWS_COLS" ){
          ApplySplit(list[1].toInt(), list[2].toInt(), false); // sources come from the PANE lines
        }

        if( list.count() == 3 && list[0] == "REFRESH" ){
          bool oldFlag = isSignalSlotActive;
          isSignalSlotActive = false;
          chkAutoRefresh->setChecked( list[1].toInt() != 0 );
          sbRefresh->setValue( list[2].toInt() );
          isSignalSlotActive = oldFlag;
        }

        if( list.count() == 4 && list[0] == "PANE" ){
          int p  = list[1].toInt();
          int sn = list[2].toInt();
          int ch = list[3].toInt();

          int d = -1;
          for( unsigned int i = 0; i < nDigi; i++){
            if( sn == (int)digiManager->GetSerialNumber(i) ) {
              d = i;
              break;
            }
          }

          if( 0 <= p && p < NumberOfPanes() ){
            suspendFilling = true;
            WaitForFillToDrain();
            SetPaneSource(p, d, ch);
            suspendFilling = false;
          }
        }

        line = in.readLine();
        continue;
      }

      if( digiID >= 0 && !list.isEmpty() ){

        QVector<float> data;
        for( int i = 0; i < list.count(); i++){
          data.push_back(list[i].toFloat());
        }

        if( 0 <= data[0] && data[0] < digiManager->GetNChannels(digiID) && data.size() >= 4 ){
          hist[digiID][int(data[0])]->Rebin(data[1], data[2], data[3]);
        }

        if( int(data[0]) == digiManager->GetNChannels(digiID) && data.size() == 7 ){
          hist2D[digiID]->Rebin(int(data[1]), data[2], data[3], int(data[4]), data[5], data[6]);
        }

      }

      line = in.readLine();
    }

  }else{

    printf("%s|cannot open HistogramSettings.txt\n", __func__);

  }

}

QVector<int> SingleSpectra::generateNonRepeatedCombination(int size) {
  QVector<int> combination;
  for (int i = 0; i < size; ++i) combination.append(i);

  for (int i = 0; i < size - 1; ++i) {
    int j = QRandomGenerator::global()->bounded(i, size);
    combination.swapItemsAt(i, j);
  }
  return combination;
}