#include "RateMonitor.h"

#include "SingleSpectra.h"
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

void RateMonitor::showEvent(QShowEvent * e){
  QWidget::showEvent(e);
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

  chkLogY = new QCheckBox("Log Y", this);
  ctrl->addWidget(chkLogY, 0, 8);
  connect(chkLogY, &QCheckBox::toggled, this, &RateMonitor::SetLogY);

  chkFixedY = new QCheckBox("Fixed Y", this);
  ctrl->addWidget(chkFixedY, 0, 9);
  chkFixedY->setToolTip("Freeze the Y auto-rescale. Drag to pan, scroll wheel to zoom the Y range.");
  connect(chkFixedY, &QCheckBox::toggled, this, &RateMonitor::SetFixedY);

  //---- second row: subscribe to anything the RateStore already knows -- histogram totals and
  //---- every live 1D gate, by display name. Refreshed whenever it could have changed.
  ctrl->addWidget(new QLabel("Histogram / gate :", this), 1, 0);
  cbStore = new RComboBox(this);
  cbStore->setObjectName("cbStore");
  cbStore->setMinimumWidth(260);
  cbStore->setToolTip("Live rate channels: mounted/monitored histogram totals and their gates.");
  ctrl->addWidget(cbStore, 1, 1, 1, 3);

  QPushButton * bnAddStore = new QPushButton("Add Selected", this);
  bnAddStore->setObjectName("bnAddStore");
  ctrl->addWidget(bnAddStore, 1, 4);
  connect(bnAddStore, &QPushButton::clicked, this, &RateMonitor::AddStoreKey);

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
  plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom); // used to pan/zoom in Fixed Y mode
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

void RateMonitor::SetLogY(bool on){
  plot->yAxis->setScaleType(on ? QCPAxis::stLogarithmic : QCPAxis::stLinear);
  if( on && plot->yAxis->range().lower <= 0 )
    plot->yAxis->setRange(1e-3, plot->yAxis->range().upper); // a log axis cannot show 0
  plot->replot();
}

void RateMonitor::SetFixedY(bool on){
  if( !on )
    plot->yAxis->rescale(true); // resume auto-rescale immediately
  // on: keep the current (last auto-scaled) range as the starting fixed range;
  // the user then adjusts it by dragging / scrolling
  plot->replot();
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
void RateMonitor::RefreshStorePicker(){
  const QString sel = cbStore->currentData().toString();
  cbStore->blockSignals(true);
  cbStore->clear();
  const QStringList keys = ss->rateStore.Keys();
  for( const QString & k : keys ){
    if( HasKey(k) )         continue; // already a trace on screen
    if( !ss->RateChannelLive(k) ) continue; // stale: owner drained by nobody
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
  // The picker tracks the store's live contents, not just dialog-open / add / remove: gates
  // restored by a settings reload (the reopen path) or created after this dialog opened show
  // up within one tick. Skipped while the popup is open -- rebuilding it under the cursor
  // would close the list mid-read. RefreshStorePicker preserves the current selection.
  if( !cbStore->view()->isVisible() ) RefreshStorePicker();

  if( traces.isEmpty() ) return; // nothing to plot; skip the pump and the replot churn

  // We are part of the sampling clock: pump the store first so the window's right edge is as
  // fresh as this tick, independent of the histogram replot period.
  ss->SampleRates();

  const double t = clock.elapsed() / 1000.0;

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
    }else{
      // fewer than two samples inside the window yet: honest placeholder, not a fake zero.
      tr.item->setText(DisplayFor(tr.key) + "  --  (gathering...)");
    }
  }

  // trim the rolling window, then pin the x range to the last windowSec and auto-scale the y axis
  const double tMin = t - windowSec;
  for( Trace & tr : traces ) tr.graph->data()->removeBefore(tMin);

  double xlo = t - windowSec, xhi = t;
  if( xlo < 0 ){ xhi = windowSec; xlo = 0; }
  plot->xAxis->setRange(xlo, xhi);

  if( !chkFixedY->isChecked() ){
    // auto mode: rescale the Y axis to the visible data every sample
    plot->yAxis->rescale(true);
    QCPRange yr = plot->yAxis->range();
    if( yr.size() < 1e-6 )
      plot->yAxis->setRange(0, 10);
    else if( chkLogY->isChecked() && yr.lower <= 0 )
      plot->yAxis->setRange(1e-3, yr.upper); // a log axis cannot show 0
  }
  // fixed mode: leave the Y range untouched so the user's pan/zoom survives

  plot->replot();
}

QString RateMonitor::ChannelName(int d, int ch) const{
  return QString("Digi-%1 ch-%2").arg(digi[d]->GetSerialNumber()).arg(ch);
}
