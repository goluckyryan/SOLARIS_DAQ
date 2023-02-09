#include "scope.h"

#include <QValueAxis>
#include <QRandomGenerator>
#include <QGroupBox>
#include <QChartView>
#include <QStandardItemModel>
#include <QLabel>


Scope::Scope(Digitizer2Gen **digi, unsigned int nDigi, ReadDataThread ** readDataThread, QMainWindow *parent) : QMainWindow(parent){
  this->digi = digi;
  this->nDigi = nDigi;
  this->readDataThread = readDataThread;

  setWindowTitle("Scope");
  setGeometry(0, 0, 1000, 800);  
  setWindowFlags( this->windowFlags() & ~Qt::WindowCloseButtonHint );

  allowChange = false;

  plot = new QChart();
  for( int i = 0; i < 6; i++) {
    dataTrace[i] = new QLineSeries();
    dataTrace[i]->setName("Trace " + QString::number(i));
    for(int j = 0; j < 100; j ++) dataTrace[i]->append(j, QRandomGenerator::global()->bounded(8000) + 8000);
    plot->addSeries(dataTrace[i]);
  }

  dataTrace[0]->setPen(QPen(Qt::red, 2));
  dataTrace[1]->setPen(QPen(Qt::blue, 2));
  dataTrace[2]->setPen(QPen(Qt::darkRed, 1));
  dataTrace[3]->setPen(QPen(Qt::darkYellow, 1));
  dataTrace[4]->setPen(QPen(Qt::darkGreen, 1));
  dataTrace[5]->setPen(QPen(Qt::darkBlue, 1));

  plot->createDefaultAxes(); /// this must be after addSeries();
  /// this must be after createDefaultAxes();
  QValueAxis * yaxis = qobject_cast<QValueAxis*> (plot->axes(Qt::Vertical).first());
  QValueAxis * xaxis = qobject_cast<QValueAxis*> (plot->axes(Qt::Horizontal).first());
  yaxis->setTickCount(6);
  yaxis->setTickInterval(16384);
  yaxis->setRange(-16384, 65536);
  yaxis->setLabelFormat("%.0f");

  xaxis->setRange(0, 5000);
  xaxis->setTickCount(11);
  xaxis->setLabelFormat("%.0f");
  xaxis->setTitleText("Time [ns]");

  updateTraceThread = new UpdateTraceThread();
  connect(updateTraceThread, &UpdateTraceThread::updateTrace, this, &Scope::UpdateScope);

  //*================ add Widgets
  int rowID = -1;
  QWidget * layoutWidget = new QWidget(this);
  setCentralWidget(layoutWidget);
  QGridLayout * layout = new QGridLayout(layoutWidget);
  layoutWidget->setLayout(layout);

  //------------ Digitizer + channel selection
  rowID ++;
  cbScopeDigi = new QComboBox(this);
  cbScopeCh = new QComboBox(this);
  layout->addWidget(cbScopeDigi, rowID, 0);
  layout->addWidget(cbScopeCh, rowID, 1);

  connect(cbScopeDigi, &QComboBox::currentIndexChanged, this, [=](){
    //if( allowChange ) StopScope();
    int index = cbScopeDigi->currentIndex();
    if( index == -1 ) return;
    for( int i = 0; i < digi[index]->GetNChannels(); i++){
      cbScopeCh->addItem("ch-" + QString::number(i), i);
    }
    //if( allowChange )StartScope(index);
  });

  connect(cbScopeCh, &QComboBox::currentIndexChanged, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    digiMTX.lock();
    digi[iDigi]->WriteChValue("0..63", DIGIPARA::CH::ChannelEnable, "false");
    digi[iDigi]->WriteChValue(std::to_string(ch), DIGIPARA::CH::ChannelEnable, "true");
    ReadScopeSettings(iDigi, ch);
    digiMTX.unlock();
  });

  allowChange = false;
  cbScopeDigi->clear(); ///this will also trigger QComboBox::currentIndexChanged
  cbScopeCh->clear();
  for( unsigned int i = 0 ; i < nDigi; i++) {
    cbScopeDigi->addItem("Digi-" + QString::number(digi[i]->GetSerialNumber()), i);
  }
  allowChange = true;


  bnScopeReset = new QPushButton("ReProgram Digitizer", this);
  layout->addWidget(bnScopeReset, rowID, 2);
  connect(bnScopeReset, &QPushButton::clicked, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    digi[iDigi]->Reset();
    digi[iDigi]->ProgramPHA(false);
  });

  bnScopeReadSettings = new QPushButton("Read Ch. Settings", this);
  layout->addWidget(bnScopeReadSettings, rowID, 3);
  connect(bnScopeReadSettings, &QPushButton::clicked, this, [=](){
    if( !allowChange ) return;
    ReadScopeSettings(cbScopeDigi->currentIndex(), cbScopeCh->currentIndex());
  });

  //TODO----- add copy settings and paste settings
  QCheckBox * chkSetAllChannel = new QCheckBox("apply to all channels", this);
  layout->addWidget(chkSetAllChannel, rowID, 4);
  chkSetAllChannel->setEnabled(false);

  //------------ Probe selection
  rowID ++;
  //TODO --- add None
  cbAnaProbe[0] = new QComboBox(this);
  cbAnaProbe[0]->addItem("ADC Input",        "ADCInput");
  cbAnaProbe[0]->addItem("Time Filter",      "TimeFilter");
  cbAnaProbe[0]->addItem("Trapazoid",        "EnergyFilter");
  cbAnaProbe[0]->addItem("Trap. Baseline",   "EnergyFilterBaseline");
  cbAnaProbe[0]->addItem("Trap. - Baseline", "EnergyFilterMinusBaseline");

  cbAnaProbe[1] = new QComboBox(this);
  for( int i = 0; i < cbAnaProbe[0]->count() ; i++) cbAnaProbe[1]->addItem(cbAnaProbe[0]->itemText(i), cbAnaProbe[0]->itemData(i));

  connect(cbAnaProbe[0], &QComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbAnaProbe, 2);});
  connect(cbAnaProbe[1], &QComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbAnaProbe, 2);});

  cbAnaProbe[0]->setCurrentIndex(1); ///trigger the AnaProbeChange
  cbAnaProbe[0]->setCurrentIndex(0);
  cbAnaProbe[1]->setCurrentIndex(4);

  connect(cbAnaProbe[0], &QComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    digiMTX.lock();
    digi[iDigi]->WriteChValue(std::to_string(ch), DIGIPARA::CH::WaveAnalogProbe0, (cbAnaProbe[0]->currentData()).toString().toStdString());
    digiMTX.unlock();
  });
  
  connect(cbAnaProbe[1], &QComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    digiMTX.lock();
    digi[iDigi]->WriteChValue(std::to_string(ch), DIGIPARA::CH::WaveAnalogProbe1, (cbAnaProbe[1]->currentData()).toString().toStdString());
    digiMTX.unlock();
  });

  //TODO --- add None
  cbDigProbe[0] = new QComboBox(this);
  cbDigProbe[0]->addItem("Trigger",              "Trigger");
  cbDigProbe[0]->addItem("Time Filter Armed",    "TimeFilterArmed");
  cbDigProbe[0]->addItem("ReTrigger Guard",      "ReTriggerGuard");
  cbDigProbe[0]->addItem("Trap. basline Freeze", "EnergyFilterBaselineFreeze");
  cbDigProbe[0]->addItem("Peaking",              "EnergyFilterPeaking");
  cbDigProbe[0]->addItem("Peak Ready",           "EnergyFilterPeakReady");
  cbDigProbe[0]->addItem("Pile-up Guard",        "EnergyFilterPileUpGuard");
  cbDigProbe[0]->addItem("Event Pile Up",        "EventPileUp");
  cbDigProbe[0]->addItem("ADC Saturate",         "ADCSaturation");
  cbDigProbe[0]->addItem("ADC Sat. Protection",  "ADCSaturationProtection");
  cbDigProbe[0]->addItem("Post Sat. Event",      "PostSaturationEvent");
  cbDigProbe[0]->addItem("Trap. Saturate",       "EnergylterSaturation");
  cbDigProbe[0]->addItem("ACQ Inhibit",          "AcquisitionInhibit");

  cbDigProbe[1] = new QComboBox(this);
  cbDigProbe[2] = new QComboBox(this);
  cbDigProbe[3] = new QComboBox(this);
  for( int i = 0; i < cbDigProbe[0]->count() ; i++) {
    cbDigProbe[1]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
    cbDigProbe[2]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
    cbDigProbe[3]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
  }

  connect(cbDigProbe[0], &QComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[1], &QComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[2], &QComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[3], &QComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});

  cbDigProbe[0]->setCurrentIndex(1); ///trigger the DigProbeChange
  cbDigProbe[0]->setCurrentIndex(0);
  cbDigProbe[1]->setCurrentIndex(4);
  cbDigProbe[2]->setCurrentIndex(5);
  cbDigProbe[3]->setCurrentIndex(6);

  connect(cbDigProbe[0], &QComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    digiMTX.lock();
    digi[iDigi]->WriteChValue(std::to_string(ch), DIGIPARA::CH::WaveDigitalProbe0, (cbDigProbe[0]->currentData()).toString().toStdString());
    digiMTX.unlock();
  });
  connect(cbDigProbe[1], &QComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    digiMTX.lock();
    digi[iDigi]->WriteChValue(std::to_string(ch), DIGIPARA::CH::WaveDigitalProbe1, (cbDigProbe[1]->currentData()).toString().toStdString());
    digiMTX.unlock();
  });
  connect(cbDigProbe[2], &QComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    digiMTX.lock();
    digi[iDigi]->WriteChValue(std::to_string(ch), DIGIPARA::CH::WaveDigitalProbe2, (cbDigProbe[2]->currentData()).toString().toStdString());
    digiMTX.unlock();
  });
  connect(cbDigProbe[3], &QComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    digiMTX.lock();
    digi[iDigi]->WriteChValue(std::to_string(ch), DIGIPARA::CH::WaveDigitalProbe3, (cbDigProbe[3]->currentData()).toString().toStdString());
    digiMTX.unlock();
  });

  layout->addWidget(cbAnaProbe[0], rowID, 0);
  layout->addWidget(cbAnaProbe[1], rowID, 1);

  layout->addWidget(cbDigProbe[0], rowID, 2);
  layout->addWidget(cbDigProbe[1], rowID, 3);
  layout->addWidget(cbDigProbe[2], rowID, 4);
  layout->addWidget(cbDigProbe[3], rowID, 5);

  rowID ++;
  {//------------ wave settings
    QGroupBox * box = new QGroupBox("Channel Settings (need ACQ stop)", this);
    layout->addWidget(box, rowID, 0, 1, 6);

    QGridLayout * bLayout = new QGridLayout(box);
    bLayout->setSpacing(0);

    sbRL = new QSpinBox(this);
    ScopeMakeSpinBox(sbRL, "Record Lenght [ns] ", bLayout, 0, 0, 32, 648000, DIGIPARA::TraceStep, DIGIPARA::CH::RecordLength);

    sbThreshold = new QSpinBox(this);
    ScopeMakeSpinBox(sbThreshold, "Threshold [LSB] ", bLayout, 0, 2, 0, 8191, -20, DIGIPARA::CH::TriggerThreshold);

    cbPolarity = new QComboBox(this);
    cbPolarity->addItem("Pos. +", "Positive");
    cbPolarity->addItem("Neg. -", "Negative");
    ScopeMakeComoBox(cbPolarity, "Polarity ", bLayout, 0, 4, DIGIPARA::CH::Polarity);

    cbWaveRes = new QComboBox(this);
    cbWaveRes->addItem(" 8 ns", "RES8");
    cbWaveRes->addItem("16 ns", "RES16");
    cbWaveRes->addItem("32 ns", "RES32");
    cbWaveRes->addItem("64 ns", "RES64");
    ScopeMakeComoBox(cbWaveRes, "Wave Re. ", bLayout, 0, 6, DIGIPARA::CH::WaveResolution);

    //------------------ next row
    sbPT = new QSpinBox(this);
    ScopeMakeSpinBox(sbPT, "Pre Trigger [ns] ", bLayout, 1, 0, 32, 32000, DIGIPARA::TraceStep, DIGIPARA::CH::PreTrigger);

    sbDCOffset = new QSpinBox(this);
    ScopeMakeSpinBox(sbDCOffset, "DC offset [%] ", bLayout, 1, 2, 0, 100, -10, DIGIPARA::CH::DC_Offset);

    sbTimeRiseTime = new QSpinBox(this);
    ScopeMakeSpinBox(sbTimeRiseTime, "Trigger Rise Time [ns] ", bLayout, 1, 4, 32, 2000, DIGIPARA::TraceStep, DIGIPARA::CH::TimeFilterRiseTime);

    sbTimeGuard = new QSpinBox(this);
    ScopeMakeSpinBox(sbTimeGuard, "Trigger Guard [ns] ", bLayout, 1, 6, 0, 8000, DIGIPARA::TraceStep, DIGIPARA::CH::TimeFilterRetriggerGuard);

    //----------------- next row
    sbTrapRiseTime = new QSpinBox(this);
    ScopeMakeSpinBox(sbTrapRiseTime, "Trap. Rise Time [ns] ", bLayout, 2, 0, 32, 13000, DIGIPARA::TraceStep, DIGIPARA::CH::EnergyFilterRiseTime);

    sbTrapFlatTop = new QSpinBox(this);
    ScopeMakeSpinBox(sbTrapFlatTop, "Trap. Flat Top [ns] ", bLayout, 2, 2, 32, 3000, DIGIPARA::TraceStep, DIGIPARA::CH::EnergyFilterFlatTop);

    sbTrapPoleZero = new QSpinBox(this);
    ScopeMakeSpinBox(sbTrapPoleZero, "Trap. Pole Zero [ns] ", bLayout, 2, 4, 32, 524000, DIGIPARA::TraceStep, DIGIPARA::CH::EnergyFilterPoleZero);

    sbEnergyFineGain = new QSpinBox(this);
    ScopeMakeSpinBox(sbEnergyFineGain, "Energy Fine Gain ", bLayout, 2, 6, 1, 10, -1, DIGIPARA::CH::EnergyFilterFineGain);

    //----------------- next row
    sbTrapPeaking = new QSpinBox(this);
    ScopeMakeSpinBox(sbTrapPeaking, "Trap. Peaking [%] ", bLayout, 3, 0, 1, 100, -10, DIGIPARA::CH::EnergyFilterPeakingPosition);

    cbTrapPeakAvg = new QComboBox(this);
    cbTrapPeakAvg->addItem(" 1 sample", "OneShot");
    cbTrapPeakAvg->addItem(" 4 sample", "LowAVG");
    cbTrapPeakAvg->addItem("16 sample", "MediumAVG");
    cbTrapPeakAvg->addItem("64 sample", "HighAVG");
    ScopeMakeComoBox(cbTrapPeakAvg, "Trap. Peaking ", bLayout, 3, 2, DIGIPARA::CH::EnergyFilterPeakingAvg);

    sbBaselineGuard = new QSpinBox(this);
    ScopeMakeSpinBox(sbBaselineGuard, "Baseline Guard [ns] ", bLayout, 3, 4, 0, 8000, DIGIPARA::TraceStep, DIGIPARA::CH::EnergyFilterBaselineGuard);

    cbBaselineAvg = new QComboBox(this);
    cbBaselineAvg->addItem("    0 samp.", "Fixed");
    cbBaselineAvg->addItem("   16 samp.", "VeryLow");
    cbBaselineAvg->addItem("   64 samp.", "Low");
    cbBaselineAvg->addItem("  256 samp.", "MediumLow");
    cbBaselineAvg->addItem(" 1024 samp.", "Medium");
    cbBaselineAvg->addItem(" 4096 samp.", "MediumHigh");
    cbBaselineAvg->addItem("16384 samp.", "High");
    ScopeMakeComoBox(cbBaselineAvg, "Baseline Avg ", bLayout, 3, 6, DIGIPARA::CH::EnergyFilterBaselineAvg);
    //----------------

    sbPileUpGuard = new QSpinBox(this);
    ScopeMakeSpinBox(sbPileUpGuard, "Pile-up Guard [ns] ", bLayout, 4, 0, 0, 64000, DIGIPARA::TraceStep, DIGIPARA::CH::EnergyFilterPileUpGuard);

    cbLowFreqFilter = new QComboBox(this);
    cbLowFreqFilter->addItem("Disabled", "Off");
    cbLowFreqFilter->addItem("Enabled", "On");
    ScopeMakeComoBox(cbLowFreqFilter, "Low Freq. Filter ", bLayout, 4, 2, DIGIPARA::CH::EnergyFilterLowFreqFilter);

  }

  //------------ plot view
  rowID ++;
  QChartView * plotView = new QChartView(plot);
  plotView->setRenderHints(QPainter::Antialiasing);
  layout->addWidget(plotView, rowID, 0, 1, 6);

  //TODO zoom and pan, see Zoom Line example

  //------------ close button
  rowID ++;
  bnScopeStart = new QPushButton("Start", this);
  layout->addWidget(bnScopeStart, rowID, 0);
  bnScopeStart->setEnabled(false);
  connect(bnScopeStart, &QPushButton::clicked, this, [=](){this->StartScope();});

  bnScopeStop = new QPushButton("Stop", this);
  layout->addWidget(bnScopeStop, rowID, 1);
  connect(bnScopeStop, &QPushButton::clicked, this, &Scope::StopScope);

  QPushButton * bnClose = new QPushButton("Close", this);
  layout->addWidget(bnClose, rowID, 5);
  connect(bnClose, &QPushButton::clicked, this, &Scope::StopScope);
  connect(bnClose, &QPushButton::clicked, this, &Scope::close);


  show();

  StartScope();

}

Scope::~Scope(){

  updateTraceThread->Stop();
  updateTraceThread->quit();
  updateTraceThread->wait();
  delete updateTraceThread;


  for( int i = 0; i < 6; i++) delete dataTrace[i];

  delete plot;

}

void Scope::ReadScopeSettings(int iDigi, int ch){
  if( !digi[iDigi] && digi[iDigi]->IsDummy() ) return;

  printf("%s\n", __func__);

  allowChange = false;

  for( int i = 0 ; i < 2; i++){
    ScopeReadComboBoxValue(iDigi, ch, cbAnaProbe[i], DIGIPARA::CH::AnalogProbe[i]);
  }

  for( int i = 0 ; i < 4; i++){
    ScopeReadComboBoxValue(iDigi, ch, cbDigProbe[i], DIGIPARA::CH::DigitalProbe[i]);
  }

  ScopeReadComboBoxValue(iDigi, ch, cbPolarity, DIGIPARA::CH::Polarity);
  ScopeReadComboBoxValue(iDigi, ch, cbWaveRes, DIGIPARA::CH::WaveResolution);
  ScopeReadComboBoxValue(iDigi, ch, cbTrapPeakAvg, DIGIPARA::CH::EnergyFilterPeakingAvg);
  ScopeReadComboBoxValue(iDigi, ch, cbBaselineAvg, DIGIPARA::CH::EnergyFilterBaselineAvg);
  ScopeReadComboBoxValue(iDigi, ch, cbLowFreqFilter, DIGIPARA::CH::EnergyFilterLowFreqFilter);

  ScopeReadSpinBoxValue(iDigi, ch, sbRL, DIGIPARA::CH::RecordLength);
  ScopeReadSpinBoxValue(iDigi, ch, sbPT, DIGIPARA::CH::PreTrigger);
  ScopeReadSpinBoxValue(iDigi, ch, sbDCOffset, DIGIPARA::CH::DC_Offset);
  ScopeReadSpinBoxValue(iDigi, ch, sbThreshold, DIGIPARA::CH::TriggerThreshold);
  ScopeReadSpinBoxValue(iDigi, ch, sbTimeRiseTime, DIGIPARA::CH::TimeFilterRiseTime);
  ScopeReadSpinBoxValue(iDigi, ch, sbTimeGuard, DIGIPARA::CH::TimeFilterRetriggerGuard);
  ScopeReadSpinBoxValue(iDigi, ch, sbTrapRiseTime, DIGIPARA::CH::EnergyFilterRiseTime);
  ScopeReadSpinBoxValue(iDigi, ch, sbTrapFlatTop, DIGIPARA::CH::EnergyFilterFlatTop);
  ScopeReadSpinBoxValue(iDigi, ch, sbTrapPoleZero, DIGIPARA::CH::EnergyFilterPoleZero);
  ScopeReadSpinBoxValue(iDigi, ch, sbEnergyFineGain, DIGIPARA::CH::EnergyFilterFineGain);
  ScopeReadSpinBoxValue(iDigi, ch, sbTrapPeaking, DIGIPARA::CH::EnergyFilterPeakingPosition);
  ScopeReadSpinBoxValue(iDigi, ch, sbBaselineGuard, DIGIPARA::CH::EnergyFilterBaselineGuard);
  ScopeReadSpinBoxValue(iDigi, ch, sbPileUpGuard, DIGIPARA::CH::EnergyFilterPileUpGuard);

  allowChange = true;
}

void Scope::StartScope(){

  if( !digi ) return; 
  
  int iDigi = cbScopeDigi->currentIndex();

  if( digi[iDigi]->IsDummy() ) return;

  printf("%s\n", __func__);

  int ch = cbScopeCh->currentIndex();

  //*---- set digitizer to take full trace; since in scope mode, no data saving, speed would be fast (How fast?)
  //* when the input rate is faster than trigger rate, Digitizer will stop data taking.

  ReadScopeSettings(iDigi, ch);

  digi[iDigi]->WriteChValue("0..63", DIGIPARA::CH::ChannelEnable, "false");
  digi[iDigi]->WriteChValue(std::to_string(ch), DIGIPARA::CH::ChannelEnable, "true");
  digi[iDigi]->SetPHADataFormat(0);

  digi[iDigi]->StartACQ();

  readDataThread[iDigi]->SetScopeRun(true);
  readDataThread[iDigi]->start();

  updateTraceThread->start();

  ScopeControlOnOff(false);

  allowChange = true;
}

void Scope::StopScope(){

  printf("%s\n", __func__);

  updateTraceThread->Stop();
  updateTraceThread->quit();
  updateTraceThread->wait();

  if(digi){
    for(int i = 0; i < nDigi; i++){
      if( digi[i]->IsDummy() ) continue;
      digiMTX.lock();
      digi[i]->StopACQ();
      digi[i]->WriteChValue("0..63", DIGIPARA::CH::ChannelEnable, "true");
      digiMTX.unlock();

      readDataThread[i]->quit();
      readDataThread[i]->wait();

    }
  }

  ScopeControlOnOff(true);
  allowChange = true;
}

void Scope::UpdateScope(){

  int iDigi = cbScopeDigi->currentIndex();
  int sample2ns = DIGIPARA::TraceStep * (1 << cbWaveRes->currentIndex());

  if( digi ){
    digiMTX.lock();
    unsigned int traceLength = digi[iDigi]->evt->traceLenght;
    unsigned int dataLength = dataTrace[0]->count();

    //---- remove all points
    for( int j = 0; j < 6; j++ ) dataTrace[j]->removePoints(0, dataLength);

    for( unsigned int i = 0 ; i < traceLength; i++){ 
      for( int j = 0; j < 2; j++) dataTrace[j]->append(sample2ns * i, digi[iDigi]->evt->analog_probes[j][i]);
      for( int j = 2; j < 6; j++) dataTrace[j]->append(sample2ns * i, (j-1)*1000 + 4000 * digi[iDigi]->evt->digital_probes[j-2][i]);
    }

    digiMTX.unlock();

    plot->axes(Qt::Horizontal).first()->setRange(0, sample2ns * traceLength);
  }

}

void Scope::ProbeChange(QComboBox * cb[], const int size ){

  QStandardItemModel * model[size] = {NULL};
  for( int i = 0; i < size; i++){
    model[i] = qobject_cast<QStandardItemModel*>(cb[i]->model());
  }

  /// Enable all items
  for( int i = 0; i < cb[0]->count(); i++) {
    for( int j = 0; j < size; j ++ ) model[j]->item(i)->setEnabled(true);
  }

  for( int i = 0; i < size; i++){
    int index = cb[i]->currentIndex();
    for( int j = 0; j < size; j++){
      if( i == j ) continue;
      model[j]->item(index)->setEnabled(false);
    }
  }

  digiMTX.lock();
  if( size == 2) {// analog probes
    for( int j = 0; j < 2; j++ )dataTrace[j]->setName(cb[j]->currentText());
  }
  if( size == 4){ // digitial probes
    for( int j = 2; j < 6; j++ )dataTrace[j]->setName(cb[j-2]->currentText());
  }
  digiMTX.unlock();

}

void Scope::ScopeControlOnOff(bool on){
  bnScopeStop->setEnabled(!on);

  bnScopeStart->setEnabled(on);
  bnScopeReset->setEnabled(on);
  bnScopeReadSettings->setEnabled(on);

  sbRL->setEnabled(on);
  sbPT->setEnabled(on);
  sbDCOffset->setEnabled(on);
  sbThreshold->setEnabled(on);
  sbTimeRiseTime->setEnabled(on);
  sbTimeGuard->setEnabled(on);
  sbTrapRiseTime->setEnabled(on);
  sbTrapFlatTop->setEnabled(on);
  sbTrapPoleZero->setEnabled(on);
  sbEnergyFineGain->setEnabled(on);
  sbTrapPeaking->setEnabled(on);
  cbPolarity->setEnabled(on);
  cbWaveRes->setEnabled(on);
  cbTrapPeakAvg->setEnabled(on);

  sbBaselineGuard->setEnabled(on);
  sbPileUpGuard->setEnabled(on);
  cbBaselineAvg->setEnabled(on);
  cbLowFreqFilter->setEnabled(on);
}

void Scope::ScopeReadSpinBoxValue(int iDigi, int ch, QSpinBox *sb, std::string digPara){
  std::string ans = digi[iDigi]->ReadChValue(std::to_string(ch), digPara);
  sb->setValue(atoi(ans.c_str()));
}

void Scope::ScopeReadComboBoxValue(int iDigi, int ch, QComboBox *cb, std::string digPara){
  std::string ans = digi[iDigi]->ReadChValue(std::to_string(ch), digPara);
  int index = cb->findData(QString::fromStdString(ans));
  if( index >= 0 && index < cb->count()) {
    cb->setCurrentIndex(index);
  }else{
    qDebug() << QString::fromStdString(ans);
  }
}

void Scope::ScopeMakeSpinBox(QSpinBox *sb, QString str, QGridLayout *layout, int row, int col, int min, int max, int step, std::string digPara){
  QLabel * lb = new QLabel(str, this);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lb, row, col);
  sb->setMinimum(min);
  sb->setMaximum(max);
  sb->setSingleStep(abs(step));
  layout->addWidget(sb, row, col+1);
  connect(sb, &QSpinBox::valueChanged, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    if( step > 1 ) sb->setValue(step*((sb->value() +  step - 1)/step));
    digiMTX.lock();
    digi[iDigi]->WriteChValue(std::to_string(cbScopeCh->currentIndex()), digPara, std::to_string(sb->value()));
    digiMTX.unlock();
  });
}

void Scope::ScopeMakeComoBox(QComboBox *cb, QString str, QGridLayout *layout, int row, int col, std::string digPara){
  QLabel * lb = new QLabel(str, this);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lb, row, col);
  layout->addWidget(cb, row, col+1);
  connect(cb, &QComboBox::currentIndexChanged, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
     digiMTX.lock();
    digi[iDigi]->WriteChValue(std::to_string(cbScopeCh->currentIndex()), digPara, cb->currentData().toString().toStdString());
    digiMTX.unlock();
  });
}



