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

  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &RateMonitor::Sample);
  timer->start(timeBaseMs);
}

RateMonitor::~RateMonitor(){
  if( timer ) timer->stop();
  // ss outlives us (SingleSpectra deletes us in its own dtor), so it is safe to stop the worker
  // from draining the channels we were tracking. on=false only stores a flag, never the cursor.
  for( const Trace & tr : traces ) ss->SetMonitorSelected(tr.d, tr.ch, false);
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
  ctrl->addWidget(bnAdd, 0, 4);
  connect(bnAdd, &QPushButton::clicked, this, &RateMonitor::AddChannel);

  ctrl->addWidget(new QLabel("Time base :", this), 0, 6);
  cmbTimeBase = new RComboBox(this);
  cmbTimeBase->blockSignals(true); // seed without firing SetTimeBase (timer does not exist yet)
  cmbTimeBase->addItem("1 s",   1000);
  cmbTimeBase->addItem("2 s",   2000);
  cmbTimeBase->addItem("5 s",   5000);
  cmbTimeBase->addItem("10 s",  10000);
  cmbTimeBase->addItem("30 s",  30000);
  cmbTimeBase->addItem("1 min", 60000);
  cmbTimeBase->addItem("5 min", 300000);
  cmbTimeBase->blockSignals(false);
  cmbTimeBase->setToolTip("Counting interval (integration time). Longer = smoother rate, updates less often.");
  ctrl->addWidget(cmbTimeBase, 0, 7);
  connect(cmbTimeBase, &RComboBox::currentIndexChanged, this, &RateMonitor::SetTimeBase);

  chkLogY = new QCheckBox("Log Y", this);
  ctrl->addWidget(chkLogY, 0, 8);
  connect(chkLogY, &QCheckBox::toggled, this, &RateMonitor::SetLogY);

  chkFixedY = new QCheckBox("Fixed Y", this);
  ctrl->addWidget(chkFixedY, 0, 9);
  chkFixedY->setToolTip("Freeze the Y auto-rescale. Drag to pan, scroll wheel to zoom the Y range.");
  connect(chkFixedY, &QCheckBox::toggled, this, &RateMonitor::SetFixedY);

  //---- the monitored set, with a live rate readout per row
  QGroupBox * monBox = new QGroupBox("Monitored Channels", this);
  main->addWidget(monBox);
  QGridLayout * monLayout = new QGridLayout(monBox);
  monLayout->setContentsMargins(6, 6, 6, 6);

  lstMonitored = new QListWidget(this);
  lstMonitored->setMinimumWidth(260);
  monLayout->addWidget(lstMonitored, 0, 0, 3, 1);

  QPushButton * bnRemove = new QPushButton("Remove", this);
  monLayout->addWidget(bnRemove, 0, 1);
  connect(bnRemove, &QPushButton::clicked, this, &RateMonitor::RemoveChannel);

  QPushButton * bnClear = new QPushButton("Clear All", this);
  monLayout->addWidget(bnClear, 1, 1);
  connect(bnClear, &QPushButton::clicked, this, &RateMonitor::ClearAll);

  QLabel * lbHint = new QLabel("(add a channel above to track its count rate)", this);
  lbHint->setStyleSheet("color:gray;");
  monLayout->addWidget(lbHint, 2, 1);

  //---- the plot: a scatter of (time, rate), one trace per monitored channel
  plot = new QCustomPlot(this);
  plot->xAxis->setLabel("Time [s]");
  plot->yAxis->setLabel("Count Rate  [/s]");
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
  if( IsMonitored(d, ch) ) return; // already tracked

  AddTrace(d, ch);
  ss->SetMonitorSelected(d, ch, true);
}

void RateMonitor::RemoveChannel(){
  QListWidgetItem * it = lstMonitored->currentItem();
  if( !it ) return;
  for( int i = 0; i < traces.size(); i++ ){
    if( traces[i].item == it ){
      ss->SetMonitorSelected(traces[i].d, traces[i].ch, false);
      RemoveTrace(traces[i].d, traces[i].ch);
      break;
    }
  }
}

void RateMonitor::ClearAll(){
  for( int i = traces.size() - 1; i >= 0; i-- ){
    ss->SetMonitorSelected(traces[i].d, traces[i].ch, false);
    RemoveTrace(traces[i].d, traces[i].ch, false);
  }
  plot->replot();
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

void RateMonitor::SetTimeBase(int index){
  // The MCS "time base": the timer interval. rate = dN/dt between firings, so a longer interval
  // integrates more counts and the Poisson scatter shrinks as 1/sqrt(N). No smoothing is applied;
  // every point is still a genuine count over its bin.
  const int ms = cmbTimeBase->itemData(index).toInt();
  if( ms <= 0 ) return;
  timeBaseMs = ms;
  timer->start(ms);
  plot->yAxis->setLabel(QString("Count Rate  [/s]   (bin %1 s)").arg(ms / 1000));
  plot->replot();
}

//^====================================================
bool RateMonitor::IsMonitored(int d, int ch) const{
  for( const Trace & tr : traces ) if( tr.d == d && tr.ch == ch ) return true;
  return false;
}

void RateMonitor::AddTrace(int d, int ch){
  QColor c = palette[ traces.size() % palette.size() ];

  QCPGraph * g = plot->addGraph();
  g->setName(ChannelName(d, ch));
  // CAEN-MCS look: a solid line connecting the samples, with a small marker on each one.
  // The line is drawn with the graph pen; the markers use the scatter style (all properties).
  g->setLineStyle(QCPGraph::lsLine);
  g->setPen(QPen(c, 1.5));
  QCPScatterStyle style(QCPScatterStyle::ssDisc, QPen(c, 1.0), QBrush(c), 4.0);
  g->setScatterStyle(style);

  QListWidgetItem * item = new QListWidgetItem(ChannelName(d, ch) + "  --  (starting...)", lstMonitored);

  Trace tr;
  tr.d = d; tr.ch = ch; tr.graph = g; tr.item = item;
  tr.lastTotal = 0; tr.lastTime = 0; tr.hasPrev = false;
  traces.append(tr);

  plot->replot();
}

void RateMonitor::RemoveTrace(int d, int ch, bool replotNow){
  for( int i = 0; i < traces.size(); i++ ){
    if( traces[i].d == d && traces[i].ch == ch ){
      plot->removeGraph(traces[i].graph);
      delete traces[i].item; // also pulls the row out of the list
      traces.removeAt(i);
      break;
    }
  }
  if( replotNow ) plot->replot();
}

//^====================================================
void RateMonitor::Sample(){
  if( traces.isEmpty() ) return; // nothing to plot; skip the replot churn

  const double t = clock.elapsed() / 1000.0;

  for( Trace & tr : traces ){
    const unsigned long tot = ss->GetTotal1D(tr.d, tr.ch);

    // First sample just seeds the baseline; a rate needs two points.
    if( !tr.hasPrev ){
      tr.hasPrev = true;
      tr.lastTotal = tot;
      tr.lastTime  = t;
      continue;
    }

    const double dt      = t - tr.lastTime;
    const long long delta = (long long)tot - (long long)tr.lastTotal;
    // delta < 0 means SingleSpectra's "Clear All Hist." zeroed the counter; show 0, not a spike.
    const double rate = (dt > 0.0 && delta > 0) ? (double)delta / dt : 0.0;
    tr.lastTotal = tot;
    tr.lastTime  = t;

    tr.graph->addData(t, rate);

    const int dec = (rate < 1000.0) ? 1 : 0;
    tr.item->setText(ChannelName(tr.d, tr.ch) + "  --  " + QString::number(rate, 'f', dec) + " /s");
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
