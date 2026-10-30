#include "RateMonitor.h"

#include "SingleHistograms.h"
#include "ClassInfluxDB.h"
#include "CustomWidgets.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QCheckBox>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QShowEvent>
#include <QHideEvent>
#include <QAbstractItemView>
#include <QMenu>

RateMonitor::RateMonitor(Digitizer2Gen ** digi, unsigned int nDigi, SingleSpectra * ss, QWidget * parent)
  : QWidget(parent, Qt::Window), digi(digi), nDigi(nDigi), ss(ss){

  setWindowTitle("Rate Monitor");

  palette << QColor(0, 114, 189)   // blue
          << QColor(217, 83, 25)   // orange
          << QColor(237, 177, 32)  // yellow
          << QColor(126, 47, 142)  // purple
          << QColor(119, 172, 48)  // green
          << QColor(204, 121, 167) // pink
          << QColor(77, 190, 238)  // sky
          << QColor(165, 165, 165); // gray

  BuildUI();

  clock.start();

  // The timer is NOT started here: showEvent owns the run/stop line, so a window that was
  // constructed but never shown -- and every hidden/closed interval -- costs nothing.
  timer = new QTimer(this);
  timer->setObjectName("rateTimer");
  connect(timer, &QTimer::timeout, this, &RateMonitor::Sample);
}

RateMonitor::~RateMonitor(){
  if( timer ) timer->stop();
  // ss outlives us (SingleSpectra deletes us in its own dtor), so it is safe to stop the worker
  // from draining the channels we were tracking. on=false only stores a flag, never the cursor.
  for( const Trace & tr : traces ) if( tr.d >= 0 ) ss->SetMonitorSelected(tr.d, tr.ch, false);
}

void RateMonitor::DAQStarted(){
  acqOn = true;
  // One DAQ run, one time zero: the x clock restarts and the previous run's points are erased.
  // (SingleSpectra zeroes the STORE's clock and histories in the same hook, so store dt math
  // never straddles the reset either.) Hidden windows wait for showEvent to arm the timer.
  clock.restart();
  for( Trace & tr : traces ) tr.graph->data()->clear();
  if( isVisible() ){
    ss->SampleRates();
    RefreshStorePicker();
    timer->start(timeBaseMs);
  }
  plot->replot();
}

void RateMonitor::DAQStopped(){
  acqOn = false;
  timer->stop();   // the last frame of the run stays up until the next start
}

void RateMonitor::showEvent(QShowEvent * e){
  QWidget::showEvent(e);
  // The window does not sample outside an acquisition: opened before DAQ start (or after
  // stop) it sits idle with an empty picker; StartACQ's DAQStarted() picks up the run.
  if( !acqOn ) return;
  ss->SampleRates();      // keys exist to list the moment the window appears,
  RefreshStorePicker();   // not one time-base tick later
  timer->start(timeBaseMs);
}

void RateMonitor::hideEvent(QHideEvent * e){
  QWidget::hideEvent(e);
  timer->stop(); // closed (✕ hides us) means stopped: no sampling, no replot churn, no zombie
}

//^====================================================
void RateMonitor::BuildUI(){
  QVBoxLayout * main = new QVBoxLayout(this);

  //---- top control row: pick a channel, add it to the monitor
  QGridLayout * ctrl = new QGridLayout();
  main->addLayout(ctrl);

  ctrl->addWidget(new QLabel("Digitizer :", this), 0, 0);
  cbDigi = new RComboBox(this);
  cbDigi->blockSignals(true); // seed the channel combo ourselves; no premature DigiChanged
  for( unsigned int i = 0; i < nDigi; i++ )
    cbDigi->addItem(QString("Digi-%1  (SN %2)").arg(i).arg(digi[i]->GetSerialNumber()), (int)i);
  cbDigi->blockSignals(false);
  ctrl->addWidget(cbDigi, 0, 1);
  connect(cbDigi, &RComboBox::currentIndexChanged, this, &RateMonitor::DigiChanged);

  ctrl->addWidget(new QLabel("Channel :", this), 0, 2);
  cbCh = new RComboBox(this);
  ctrl->addWidget(cbCh, 0, 3);

  QPushButton * bnAdd = new QPushButton("Add to Monitor", this);
  bnAdd->setObjectName("bnAdd");
  bnAdd->setToolTip("Track the in-range total rate of this channel (raw counter, no gate).");
  ctrl->addWidget(bnAdd, 0, 4);
  connect(bnAdd, &QPushButton::clicked, this, &RateMonitor::AddChannel);

  ctrl->addWidget(new QLabel("Time average :", this), 0, 6);
  cmbTimeBase = new RComboBox(this);
  cmbTimeBase->setObjectName("cmbTimeBase");
  cmbTimeBase->blockSignals(true); // seed without firing; the timer does not exist yet either
  cmbTimeBase->addItem("1 s",   1000);
  cmbTimeBase->addItem("2 s",   2000);
  cmbTimeBase->addItem("5 s",   5000);
  cmbTimeBase->addItem("10 s",  10000);
  cmbTimeBase->addItem("30 s",  30000);
  cmbTimeBase->addItem("1 min", 60000);
  cmbTimeBase->addItem("5 min", 300000);
  int seed = cmbTimeBase->findData(ss->RateTimeBase());
  if( seed >= 0 ){
    cmbTimeBase->setCurrentIndex(seed);
    timeBaseMs = ss->RateTimeBase();
  }
  cmbTimeBase->blockSignals(false);
  cmbTimeBase->setToolTip("Shared with the main window's Time average control: the trailing "
                          "averaging window of EVERY rate -- histogram labels included.");
  ctrl->addWidget(cmbTimeBase, 0, 7);
  // one face of the global control: this window applies it locally AND propagates to the main
  // window (fromUser=true), which echoes back via SetTimeBase(ms,false) -- no loop.
  connect(cmbTimeBase, &RComboBox::currentIndexChanged, this, [=](int idx){
    const int ms = cmbTimeBase->itemData(idx).toInt();
    if( ms > 0 ) SetTimeBase(ms, true);
  });

  //---- second row: subscribe to any GATE that exists -- by display name. Histogram totals are
  //---- row 1's job (digitizer + channel + Add). Refreshed whenever a gate is born/renamed/dies
  //---- or a pane remounts, whether or not the DAQ is running.
  ctrl->addWidget(new QLabel("Gate :", this), 1, 0);
  cbStore = new RComboBox(this);
  cbStore->setObjectName("cbStore");
  cbStore->setMinimumWidth(260);
  cbStore->setToolTip("Every gate that exists, listed whether or not its histogram is mounted. "
                      "Adding one here starts its counting (monitor alone drives the fill). "
                      "Track a channel total with the row above.");
  ctrl->addWidget(cbStore, 1, 1, 1, 3);

  QPushButton * bnAddStore = new QPushButton("Add Selected", this);
  bnAddStore->setObjectName("bnAddStore");
  ctrl->addWidget(bnAddStore, 1, 4);
  connect(bnAddStore, &QPushButton::clicked, this, &RateMonitor::AddStoreKey);

  //---- InfluxDB output: exactly what the plot computes, batched into ONE write per tick.
  //     Enabled only with a healthy singleton connection; the preference persists through
  //     SingleSpectra (json "influxOut" + auto save).
  chkInflux = new QCheckBox("Influx out", this);
  chkInflux->setObjectName("chkInflux");
  chkInflux->setToolTip("Push every traced rate to InfluxDB once per time-average tick"
                        " (one batched POST, schema shown at right).");
  ctrl->addWidget(chkInflux, 0, 8);
  connect(chkInflux, &QCheckBox::toggled, this, [this](bool on){
    if( influxOut == on ) return;
    influxOut = on;
    if( ss ) ss->SetInfluxOut( on );        // persist via the settings file
  });

  lbInfluxSchema = new QLabel("MonitorRate,Bd=<serial>,Ch=<NN>,Src=<total|gN> value=<Hz>", this);
  lbInfluxSchema->setObjectName("lbInfluxSchema");
  lbInfluxSchema->setStyleSheet("color:gray; font-family:monospace;");
  lbInfluxSchema->setToolTip("Line protocol, one line per traced key per tick, batched into a\n"
                             "single write. Src carries the gate ID (rename-stable); value is\n"
                             "the time-averaged rate in Hz (server-side ingest timestamp).");
  ctrl->addWidget(lbInfluxSchema, 1, 5, 1, 4);

  //---- the monitored set, with a live rate readout per row
  QGroupBox * monBox = new QGroupBox("Monitored Channels", this);
  main->addWidget(monBox);
  QGridLayout * monLayout = new QGridLayout(monBox);
  monLayout->setContentsMargins(6, 6, 6, 6);

  lstMonitored = new QListWidget(this);
  lstMonitored->setObjectName("lstMonitored");
  lstMonitored->setMinimumWidth(260);
  monLayout->addWidget(lstMonitored, 0, 0, 3, 1);

  QPushButton * bnRemove = new QPushButton("Remove", this);
  bnRemove->setObjectName("bnRemove");
  monLayout->addWidget(bnRemove, 0, 1);
  connect(bnRemove, &QPushButton::clicked, this, &RateMonitor::RemoveChannel);

  QPushButton * bnClear = new QPushButton("Clear All", this);
  monLayout->addWidget(bnClear, 1, 1);
  connect(bnClear, &QPushButton::clicked, this, &RateMonitor::ClearAll);

  QLabel * lbHint = new QLabel("(add a channel or gate above to track its rate)", this);
  lbHint->setStyleSheet("color:gray;");
  monLayout->addWidget(lbHint, 2, 1);

  //---- the plot: a scatter of (time, rate), one trace per subscribed store key
  plot = new QCustomPlot(this);
  plot->xAxis->setLabel("Time [s]");
  plot->yAxis->setLabel(QString("Count Rate  [Hz]   (avg %1)").arg(TimeBaseText()));
  plot->legend->setVisible(true);
  QPen borderPen = plot->legend->borderPen();
  borderPen.setWidth(0);
  borderPen.setColor(Qt::transparent);
  plot->legend->setBorderPen(borderPen);
  plot->legend->setFont(QFont("Helvetica", 9));
  plot->xAxis->setRange(0, windowSec);
  plot->yAxis->setRange(0, 10);
  plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
  plot->setToolTip("Wheel / drag: Y axis only (X is the rolling window).\n"
                   "A user zoom holds the Y range; right-click Unzoom releases it.\n"
                   "Right-click also toggles Log Y.");
  //^---- wheel and drag touch the Y axis ONLY: the X axis is the rolling window owned by the
  //^     sample clock; panning/zooming it sideways would fight every tick's setRange anyway.
  plot->axisRect()->setRangeZoom(Qt::Vertical);
  plot->axisRect()->setRangeDrag(Qt::Vertical);
  //^---- right-click: Unzoom (back to auto mode, padded default range)
  plot->setContextMenuPolicy(Qt::CustomContextMenu);
  connect( plot, &QWidget::customContextMenuRequested, this, [this](const QPoint & pos){
    QMenu menu(plot);
    QAction * aLogY   = menu.addAction("Log Y");
    aLogY->setCheckable(true);
    aLogY->setChecked( plot->yAxis->scaleType() == QCPAxis::stLogarithmic );
    menu.addSeparator();
    QAction * aUnzoom = menu.addAction("Unzoom");
    QAction * sel = menu.exec( plot->mapToGlobal(pos) );
    if( sel == aUnzoom )      UnzoomY();
    else if( sel == aLogY )   SetLogY( aLogY->isChecked() );
  });
  connect( plot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &RateMonitor::onYRangeChanged );
  plot->replot();
  main->addWidget(plot, 1);

  // now that cbDigi holds data, seed the channel combo for the first digitizer
  DigiChanged(0);
  RefreshStorePicker();

  resize(900, 650);
}

void RateMonitor::DigiChanged(int index){
  (void)index;
  cbCh->blockSignals(true);
  cbCh->clear();
  int d = cbDigi->currentData().toInt();
  if( d >= 0 && d < (int)nDigi ){
    for( int ch = 0; ch < digi[d]->GetNChannels(); ch++ )
      cbCh->addItem(QString("ch-%1").arg(ch), ch);
  }
  cbCh->blockSignals(false);
}

void RateMonitor::AddChannel(){
  int d  = cbDigi->currentData().toInt();
  int ch = cbCh->currentData().toInt();
  if( d < 0 || ch < 0 ) return;
  if( d >= (int)nDigi || ch >= (int)digi[d]->GetNChannels() ) return;

  const QString key = ss->TotalRateKey(d, ch);
  if( key.isEmpty() || HasKey(key) ) return; // no histogram for this channel, or already traced

  HoldChannel(d, ch);
  AddTrace(key, d, ch, ChannelName(d, ch));
}

void RateMonitor::AddStoreKey(){
  const QString key = cbStore->currentData().toString();
  if( key.isEmpty() || HasKey(key) ) return;

  const int d  = ss->rateStore.OwnerD(key);
  const int ch = ss->rateStore.OwnerCh(key);
  if( d >= 0 && ch >= 0 ) HoldChannel(d, ch);

  AddTrace(key, d, ch, DisplayFor(key));
}

void RateMonitor::RemoveChannel(){
  QListWidgetItem * it = lstMonitored->currentItem();
  if( !it ) return;
  for( int i = 0; i < traces.size(); i++ ){
    if( traces[i].item == it ){
      const QString key = traces[i].key;
      const int d = traces[i].d, ch = traces[i].ch;
      RemoveTrace(key);
      ReleaseIfUnused(d, ch);
      break;
    }
  }
  RefreshStorePicker();
}

void RateMonitor::ClearAll(){
  QVector<Trace> copy = traces; // RemoveTrace mutates traces
  for( int i = copy.size() - 1; i >= 0; i-- ) RemoveTrace(copy[i].key);
  // Release only channels no trace still needs, with the FINAL trace list in hand.
  for( const Trace & tr : copy ) ReleaseIfUnused(tr.d, tr.ch);
  plot->replot();
  RefreshStorePicker();
}

void RateMonitor::SetInflux(InfluxDB * db, const QString & database, bool out){
  influx = db;
  influxDb = database;
  if( !chkInflux ) return;   // arrives before BuildUI only in odd construction orders
  const bool healthy = db && db->IsConnectionOK();
  const QSignalBlocker guard(chkInflux);
  chkInflux->setEnabled( healthy );
  chkInflux->setChecked( healthy && out );
  chkInflux->setToolTip( healthy
      ? "Push every traced rate to InfluxDB once per time-average tick (one batched POST)."
      : "InfluxDB is not configured or not connected (Program Settings -> Database)." );
  influxOut = chkInflux->isChecked();
  if( lbInfluxSchema ) lbInfluxSchema->setVisible( healthy );
}

void RateMonitor::SetLogY(bool on){
  plot->yAxis->setScaleType(on ? QCPAxis::stLogarithmic : QCPAxis::stLinear);
  if( on && plot->yAxis->range().lower <= 0 )
    plot->yAxis->setRange(1e-3, plot->yAxis->range().upper); // a log axis cannot show 0
  plot->replot();
}

//^ Auto mode's Y fit: hug the visible data, then add 10% headroom (max 5 -> 0..5.5).
//^ A linear axis never shows negative rates (minimum clamped to 0); a log axis keeps a
//^ positive floor instead. yAutoBusy keeps our own writes from looking like user action.
void RateMonitor::FitY(){
  yAutoBusy = true;
  plot->yAxis->rescale(true);
  const QCPRange yr = plot->yAxis->range();
  if( yr.size() < 1e-6 )
    plot->yAxis->setRange( 0, 10 );                              // nothing visible yet
  else if( plot->yAxis->scaleType() == QCPAxis::stLogarithmic )
    plot->yAxis->setRange( qMax( 1e-3, yr.lower ), yr.upper * 1.1 );
  else
    plot->yAxis->setRange( 0, yr.upper * 1.1 );
  yAutoBusy = false;
}

//^ Right-click Unzoom: give the axis back to auto control and land on the padded default NOW.
void RateMonitor::UnzoomY(){
  yFixed = false;   // back under auto control...
  FitY();           // ...and land on the padded default range NOW, not next tick
  plot->replot();
}

//^ Every Y range change that is NOT ours is the user zooming/panning:
//^  - the linear minimum stays at 0 (a rate axis has no negative side; log excepted), and
//^  - auto-rescale is switched OFF (Fixed Y), because the next Sample() tick would otherwise
//^    overwrite exactly the zoom-out the user just performed.
void RateMonitor::onYRangeChanged(const QCPRange & range){
  if( yAutoBusy ) return;
  yFixed = true;   // no checkbox: the zoom itself is the switch, Unzoom is the way back
  if( plot->yAxis->scaleType() == QCPAxis::stLinear && range.lower < 0. ){
    yAutoBusy = true;
    plot->yAxis->setRange( 0., range.upper );
    yAutoBusy = false;
  }
}

void RateMonitor::SetTimeBase(int ms, bool fromUser){
  // The "time average": the trailing window of EVERY rate AND this window's tick period. The
  // store answers counts-over-window / window-seconds [Hz] for that window; since the tick
  // period equals it, consecutive points tile back-to-back like MCS bins. A longer average
  // means more counts per point, so the Poisson scatter shrinks as 1/sqrt(N).
  if( ms <= 0 ) return;
  timeBaseMs = ms;

  // reflect into the combo WITHOUT re-firing the user path (that would echo back to main)
  int index = cmbTimeBase->findData(ms);
  if( index >= 0 ){
    const bool wasBlocked = cmbTimeBase->blockSignals(true);
    cmbTimeBase->setCurrentIndex(index);
    cmbTimeBase->blockSignals(wasBlocked);
  }

  if( timer->isActive() ) timer->start(ms); // hidden stays hidden; showEvent restarts
  plot->yAxis->setLabel(QString("Count Rate  [Hz]   (avg %1)").arg(TimeBaseText()));
  plot->replot();

  // the ONE source of truth is the main window; only a user edit here propagates to it.
  if( fromUser ) ss->SetRateTimeBase(ms);
}

QString RateMonitor::TimeBaseText() const{
  return ( timeBaseMs % 1000 == 0 ) ? QString("%1 s").arg(timeBaseMs / 1000)
                                    : QString("%1 ms").arg(timeBaseMs);
}

//^====================================================
bool RateMonitor::HasKey(const QString & key) const{
  for( const Trace & tr : traces ) if( tr.key == key ) return true;
  return false;
}

void RateMonitor::HoldChannel(int d, int ch){
  if( d < 0 || ch < 0 ) return;
  ss->SetMonitorSelected(d, ch, true);
}

void RateMonitor::ReleaseIfUnused(int d, int ch){
  if( d < 0 || ch < 0 ) return;
  for( const Trace & tr : traces ) if( tr.d == d && tr.ch == ch ) return; // still needed
  ss->SetMonitorSelected(d, ch, false);
}

void RateMonitor::AddTrace(const QString & key, int d, int ch, const QString & name){
  QColor c = palette[ traces.size() % palette.size() ];

  QCPGraph * g = plot->addGraph();
  g->setName(name);
  // CAEN-MCS look: a solid line connecting the samples, with a small marker on each one.
  // The line is drawn with the graph pen; the markers use the scatter style (all properties).
  g->setLineStyle(QCPGraph::lsLine);
  g->setPen(QPen(c, 1.5));
  QCPScatterStyle style(QCPScatterStyle::ssDisc, QPen(c, 1.0), QBrush(c), 4.0);
  g->setScatterStyle(style);

  QListWidgetItem * item = new QListWidgetItem(name + "  --  (gathering...)", lstMonitored);

  Trace tr;
  tr.key = key; tr.d = d; tr.ch = ch; tr.graph = g; tr.item = item;
  traces.append(tr);

  RefreshStorePicker();
  plot->replot();
}

void RateMonitor::RemoveTrace(const QString & key){
  for( int i = 0; i < traces.size(); i++ ){
    if( traces[i].key == key ){
      plot->removeGraph(traces[i].graph);
      delete traces[i].item; // also pulls the row out of the list
      traces.removeAt(i);
      break;
    }
  }
  plot->replot();
}

//^====================================================
//^ The picker lists GATES THAT EXIST: keys are registered when a gate is drawn or loaded,
//^ whether or not its histogram is on screen -- an unmounted channel's gates stay listed.
//^ COMPUTATION follows OBSERVATION instead: counts are produced (fill worker + sampling)
//^ only when someone watches -- a mounted pane (histogram labels) or a trace added here
//^ (HoldChannel starts the drain; catch-up from the ring shows as one honest burst that
//^ settles within a window). Histogram TOTALS are deliberately absent: row 1 is their face.
void RateMonitor::RefreshStorePicker(){
  const QString sel = cbStore->currentData().toString();
  cbStore->blockSignals(true);
  cbStore->clear();
  const QStringList keys = ss->rateStore.Keys();
  for( const QString & k : keys ){
    if( !k.contains(QLatin1String("/g")) ) continue;  // totals go via row 1 (GateKey == ".../gN")
    if( HasKey(k) )         continue; // already a trace on screen
    cbStore->addItem(DisplayFor(k), k);
  }
  const int idx = cbStore->findData(sel);
  if( idx >= 0 ) cbStore->setCurrentIndex(idx);
  cbStore->blockSignals(false);
}

QString RateMonitor::DisplayFor(const QString & key) const{
  const QString l = ss->rateStore.Label(key);
  return l.isEmpty() ? key : l;
}

//^====================================================
void RateMonitor::Sample(){
  if( !acqOn ) return;   // DAQ stopped between arming and this queued tick: drop it
  // Pump FIRST so the picker rebuild below -- and the trace reads -- see THIS tick's keys;
  // refreshing before the pump cost a tick of latency on every newly appearing key.
  // We are part of the sampling clock: the pump keeps the store's right edge as fresh as
  // this tick, independent of the histogram replot period. It runs EVEN WITH ZERO TRACES:
  // the picker lists live keys, and keys only exist once sampled -- without the pump the
  // store stays empty until ACQ starts the replot clock, the picker shows nothing, and the
  // first trace could never be added before Start.
  ss->SampleRates();

  // The picker tracks the store's live contents, not just dialog-open / add / remove: gates
  // restored by a settings reload (the reopen path) or created after this dialog opened show
  // up within one tick. Skipped while the popup is open -- rebuilding it under the cursor
  // would close the list mid-read. RefreshStorePicker preserves the current selection.
  if( !cbStore->view()->isVisible() ) RefreshStorePicker();

  if( traces.isEmpty() ) return; // nothing to plot; skip the expire walk and the replot churn

  // Auto-expire traces whose key has LEFT the store (its gate was deleted -- Drop is the only
  // thing that removes keys, and it runs exactly when the histogram drops the gate). Two ticks
  // are allowed to pass first so a replace-in-place (a settings reload that clears and restores
  // the same gate id within one edit) does not evict a trace the user never removed.
  for( int i = traces.size() - 1; i >= 0; i-- ){
    if( ss->rateStore.Exists(traces[i].key) ){ traces[i].miss = 0; continue; }
    if( ++traces[i].miss < 2 ) continue;
    const int d = traces[i].d, ch = traces[i].ch;
    RemoveTrace(traces[i].key);
    ReleaseIfUnused(d, ch);   // the drain-hold dies with the trace
  }

  const double t = clock.elapsed() / 1000.0;
  QVector<QByteArray> pushLines;   // this tick's InfluxDB batch (empty unless we append)

  for( Trace & tr : traces ){
    double r = 0.0;
    if( ss->rateStore.Rate(tr.key, timeBaseMs, r) ){
      // Hz: counts accumulated over the trailing time-average window divided by that window's
      // actual elapsed time (before a full window exists it degrades to elapsed-so-far, never a
      // fake zero). The window = the shared "Time average" control; the tick period equals it,
      // so consecutive points tile back-to-back like MCS bins. Poisson scatter still shrinks
      // as 1/sqrt(N) with a longer average -- that is its whole purpose.
      tr.graph->addData(t, r);
      tr.item->setText(DisplayFor(tr.key) + "  --  " + QString::number(r, 'f', 1) + " Hz");
      const int gp = tr.key.indexOf( "/g" );
      const QString src = ( gp >= 0 ) ? tr.key.mid(gp+1) : QString("total");
      const QString bd  = ( tr.d >= 0 && tr.d < (int)nDigi && digi[tr.d] )
                            ? QString::number( digi[tr.d]->GetSerialNumber() ) : QString("0");
      pushLines.append( QString("MonitorRate,Bd=%1,Ch=%2,Src=%3 value=%4")
                          .arg(bd)
                          .arg( tr.ch, 2, 10, QChar('0') )
                          .arg(src)
                          .arg( QString::number(r, 'f', 3) ).toUtf8() );
    }else{
      // fewer than two samples inside the window yet: honest placeholder, not a fake zero.
      tr.item->setText(DisplayFor(tr.key) + "  --  (gathering...)");
    }
  }

  //---- batched push: everything this tick computed travels in ONE write -- and nothing
  //     unmeasured fakes a zero. Synchronous curl on the GUI thread, exactly like the
  //     scalar writers (tick period >= 1 s); should sub-second bases ever reach here,
  //     move to SingleSpectra's one-shot writer-thread pattern.
  if( influxOut && influx && influx->IsConnectionOK() && !pushLines.isEmpty() ){
    influx->ClearDataPointsBuffer();
    for( const QByteArray & line : pushLines ) influx->AddDataPoint( line.constData() );
    influx->WriteData( influxDb.toStdString() );
  }

  // trim the rolling window, then pin the x range to the last windowSec and auto-scale the y axis
  const double tMin = t - windowSec;
  for( Trace & tr : traces ) tr.graph->data()->removeBefore(tMin);

  double xlo = t - windowSec, xhi = t;
  if( xlo < 0 ){ xhi = windowSec; xlo = 0; }
  plot->xAxis->setRange(xlo, xhi);

  if( !yFixed )
    FitY();     // auto mode: hug the data with 10% headroom every sample
  // user-owned Y range (zoomed): leave it untouched so the pan/zoom survives

  plot->replot();
}

QString RateMonitor::ChannelName(int d, int ch) const{
  return QString("Digi-%1 ch-%2").arg(digi[d]->GetSerialNumber()).arg(ch);
}
