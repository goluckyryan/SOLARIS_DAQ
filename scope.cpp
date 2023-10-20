#include "scope.h"

#include <QValueAxis>
#include <QRandomGenerator>
#include <QGroupBox>
#include <QStandardItemModel>
#include <QLabel>

#define MaxDisplayTraceDataLength 2000 //data point, 

Scope::Scope(Digitizer2Gen **digi, unsigned int nDigi, ReadDataThread ** readDataThread, QMainWindow *parent) : QMainWindow(parent){
  this->digi = digi;
  this->nDigi = nDigi;
  if( nDigi > MaxNumberOfDigitizer ) {
    this->nDigi = MaxNumberOfChannel;
    qDebug() << "Please increase the MaxNumberOfChannel";
  }
  this->readDataThread = readDataThread;

  setWindowTitle("Scope");
  setGeometry(0, 0, 1000, 800);  
  setWindowFlags( this->windowFlags() & ~Qt::WindowCloseButtonHint );

  allowChange = false;
  originalValueSet = false;

  plot = new Trace();
  for( int i = 0; i < 6; i++) {
    dataTrace[i] = new QLineSeries();
    dataTrace[i]->setName("Trace " + QString::number(i));
    for(int j = 0; j < 100; j ++) dataTrace[i]->append(40*j, QRandomGenerator::global()->bounded(8000) + 8000);
    plot->addSeries(dataTrace[i]);
  }

  dataTrace[0]->setPen(QPen(Qt::red, 2));
  dataTrace[1]->setPen(QPen(Qt::blue, 2));
  dataTrace[2]->setPen(QPen(Qt::darkRed, 1));
  dataTrace[3]->setPen(QPen(Qt::darkYellow, 1));
  dataTrace[4]->setPen(QPen(Qt::darkGreen, 1));
  dataTrace[5]->setPen(QPen(Qt::darkBlue, 1));

  plot->setAnimationDuration(1); // msec
  plot->setAnimationOptions(QChart::NoAnimation);
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

  updateTraceThread = new TimingThread();
  updateTraceThread->SetWaitTimeSec(0.2);
  connect(updateTraceThread, &TimingThread::TimeUp, this, &Scope::UpdateScope);
  
  //*================ add Widgets
  int rowID = -1;
  QWidget * layoutWidget = new QWidget(this);
  setCentralWidget(layoutWidget);
  QGridLayout * layout = new QGridLayout(layoutWidget);
  layoutWidget->setLayout(layout);

  //------------ Digitizer + channel selection
  rowID ++;
  cbScopeDigi = new RComboBox(this);
  cbScopeCh = new RComboBox(this);
  layout->addWidget(cbScopeDigi, rowID, 0);
  layout->addWidget(cbScopeCh, rowID, 1);

  allowChange = false;
  cbScopeDigi->clear(); ///this will also trigger RComboBox::currentIndexChanged
  cbScopeCh->clear();
  for( unsigned int i = 0 ; i < nDigi; i++) {
    cbScopeDigi->addItem("Digi-" + QString::number(digi[i]->GetSerialNumber()), i);
  }
  cbScopeDigi->setCurrentIndex(1);
  allowChange = true;

  connect(cbScopeDigi, &RComboBox::currentIndexChanged, this, &Scope::ChangeDigitizer);

  connect(cbScopeCh, &RComboBox::currentIndexChanged, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    digiMTX[iDigi].lock();
    ReadScopeSettings();
    if( digi[iDigi]->IsAcqOn()){
      digi[iDigi]->WriteValue(PHA::CH::ChannelEnable, "False", -1);
      digi[iDigi]->WriteValue(PHA::CH::ChannelEnable, "True", cbScopeCh->currentIndex());
    }
    digiMTX[iDigi].unlock();
  });

  bnScopeReset = new QPushButton("ReProgram Channels", this);
  layout->addWidget(bnScopeReset, rowID, 2);
  connect(bnScopeReset, &QPushButton::clicked, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    //digi[iDigi]->Reset();
    digi[iDigi]->ProgramChannels();
    //SendLogMsg("Reset Digi-" + QString::number(digi[iDigi]->GetSerialNumber()) + " and Set Default PHA.");
    ReadScopeSettings();
    UpdateOtherPanels();
    SendLogMsg("Re-program all Channels to default PHA settings");
  });

  bnScopeReadSettings = new QPushButton("Read Ch. Settings", this);
  layout->addWidget(bnScopeReadSettings, rowID, 3);
  connect(bnScopeReadSettings, &QPushButton::clicked, this, [=](){
    if( !allowChange ) return;
    ReadScopeSettings();
    UpdateOtherPanels();
  });

  //TODO----- add copy settings and paste settings
  chkSetAllChannel = new QCheckBox("apply to all channels", this);
  layout->addWidget(chkSetAllChannel, rowID, 4);

  //------------ Probe selection
  rowID ++;

  //TODO --- add None for probel selection
  cbAnaProbe[0] = new RComboBox(this);
  cbAnaProbe[1] = new RComboBox(this);
  cbDigProbe[0] = new RComboBox(this);
  cbDigProbe[1] = new RComboBox(this);
  cbDigProbe[2] = new RComboBox(this);
  cbDigProbe[3] = new RComboBox(this);

  connect(cbAnaProbe[0], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbAnaProbe, 2);});
  connect(cbAnaProbe[1], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbAnaProbe, 2);});

  connect(cbAnaProbe[0], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX[iDigi].lock();
    digi[iDigi]->WriteValue(anaProbeList[0], (cbAnaProbe[0]->currentData()).toString().toStdString(), ch);
    digiMTX[iDigi].unlock();
  });
  
  connect(cbAnaProbe[1], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX[iDigi].lock();
    digi[iDigi]->WriteValue(anaProbeList[1], (cbAnaProbe[1]->currentData()).toString().toStdString(), ch);
    digiMTX[iDigi].unlock();
  });

  connect(cbDigProbe[0], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[1], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[2], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[3], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});

  connect(cbDigProbe[0], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX[iDigi].lock();
    digi[iDigi]->WriteValue(digiProbeList[0], (cbDigProbe[0]->currentData()).toString().toStdString(), ch);
    digiMTX[iDigi].unlock();
  });
  connect(cbDigProbe[1], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX[iDigi].lock();
    digi[iDigi]->WriteValue(digiProbeList[1], (cbDigProbe[1]->currentData()).toString().toStdString(), ch);
    digiMTX[iDigi].unlock();
  });
  connect(cbDigProbe[2], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX[iDigi].lock();
    digi[iDigi]->WriteValue(digiProbeList[2], (cbDigProbe[2]->currentData()).toString().toStdString(), ch);
    digiMTX[iDigi].unlock();
  });
  connect(cbDigProbe[3], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX[iDigi].lock();
    digi[iDigi]->WriteValue(digiProbeList[3], (cbDigProbe[3]->currentData()).toString().toStdString(), ch);
    digiMTX[iDigi].unlock();
  });

  layout->addWidget(cbAnaProbe[0], rowID, 0);
  layout->addWidget(cbAnaProbe[1], rowID, 1);

  layout->addWidget(cbDigProbe[0], rowID, 2);
  layout->addWidget(cbDigProbe[1], rowID, 3);
  layout->addWidget(cbDigProbe[2], rowID, 4);
  layout->addWidget(cbDigProbe[3], rowID, 5);

  for( int i = 0; i < 6; i++) layout->setColumnStretch(i, 1);

  rowID ++;

  {//------------ wave settings
    settingBox = new QGroupBox("Channel Settings (need ACQ stop)", this);
    layout->addWidget(settingBox, rowID, 0, 1, 6);

    bLayout = new QGridLayout(settingBox);
    bLayout->setSpacing(0);

    if( digi[0]->GetFPGAType() == DPPType::PHA ) SetupPHA();
    
    if( digi[0]->GetFPGAType() == DPPType::PSD ) SetupPSD();

  }

  //Trigger the ChangeDigitizer()
  cbScopeDigi->setCurrentIndex(0);
  cbScopeCh->setCurrentIndex(1);
  cbScopeCh->setCurrentIndex(0);

  //------------ plot view
  rowID ++;
  TraceView * plotView = new TraceView(plot);
  plotView->setRenderHints(QPainter::Antialiasing);
  layout->addWidget(plotView, rowID, 0, 1, 6);

  //------------- Key binding
  rowID ++;
  QLabel * lbhints = new QLabel("Type 'r' to restore view, '+/-' Zoom in/out, arrow key to pan.", this);
  layout->addWidget(lbhints, rowID, 0, 1, 4);
  
  QLabel * lbinfo = new QLabel("Trace update every " + QString::number(updateTraceThread->GetWaitTimeinSec()) + " sec.", this);
  lbinfo->setAlignment(Qt::AlignRight);
  layout->addWidget(lbinfo, rowID, 5);

  rowID ++;
  QLabel * lbinfo2 = new QLabel("Maximum time range is " + QString::number(MaxDisplayTraceDataLength * PHA::TraceStep) + " ns due to processing speed.", this);
  layout->addWidget(lbinfo2, rowID, 0, 1, 5);

  //------------ close button
  rowID ++;
  bnScopeStart = new QPushButton("Start", this);
  layout->addWidget(bnScopeStart, rowID, 0);
  bnScopeStart->setEnabled(true);
  connect(bnScopeStart, &QPushButton::clicked, this, [=](){this->StartScope();});

  bnScopeStop = new QPushButton("Stop", this);
  layout->addWidget(bnScopeStop, rowID, 1);
  bnScopeStop->setEnabled(false);
  connect(bnScopeStop, &QPushButton::clicked, this, &Scope::StopScope);

  QLabel * lbTriggerRate = new QLabel("Trigger Rate [Hz] : ", this);
  lbTriggerRate->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(lbTriggerRate, rowID, 2);

  leTriggerRate = new QLineEdit(this);
  leTriggerRate->setAlignment(Qt::AlignRight);
  leTriggerRate->setReadOnly(true);
  layout->addWidget(leTriggerRate, rowID, 3);

  QPushButton * bnClose = new QPushButton("Close", this);
  layout->addWidget(bnClose, rowID, 5);
  connect(bnClose, &QPushButton::clicked, this, &Scope::close);

  show();

  //StartScope();

  UpdateSettingsFromMemeory();

}

Scope::~Scope(){
  printf("------- %s \n", __func__);
  StopScope();
  updateTraceThread->Stop();
  updateTraceThread->quit();
  updateTraceThread->wait();
  delete updateTraceThread;
  for( int i = 0; i < 6; i++) delete dataTrace[i];
  delete plot;
  printf("------- end of %s \n", __func__);
}

void Scope::ChangeDigitizer(){

  int index = cbScopeDigi->currentIndex();
  if( index == -1 ) return;
  allowChange = false;

  cbScopeCh->clear();
  for( int i = 0; i < digi[index]->GetNChannels(); i++){
    cbScopeCh->addItem("ch-" + QString::number(i), i);
  }
  cbScopeCh->setCurrentIndex(0);

  anaProbeList.clear();
  digiProbeList.clear();
  if( digi[index]->GetFPGAType() == DPPType::PHA ) {
    anaProbeList.push_back(PHA::CH::WaveAnalogProbe0);
    anaProbeList.push_back(PHA::CH::WaveAnalogProbe1);
    digiProbeList.push_back(PHA::CH::WaveDigitalProbe0);
    digiProbeList.push_back(PHA::CH::WaveDigitalProbe1);
    digiProbeList.push_back(PHA::CH::WaveDigitalProbe2);
    digiProbeList.push_back(PHA::CH::WaveDigitalProbe3);
  } 
  if( digi[index]->GetFPGAType() == DPPType::PSD){
    anaProbeList.push_back(PSD::CH::WaveAnalogProbe0);
    anaProbeList.push_back(PSD::CH::WaveAnalogProbe1);
    digiProbeList.push_back(PSD::CH::WaveDigitalProbe0);
    digiProbeList.push_back(PSD::CH::WaveDigitalProbe1);
    digiProbeList.push_back(PSD::CH::WaveDigitalProbe2);
    digiProbeList.push_back(PSD::CH::WaveDigitalProbe3);    }

  cbAnaProbe[0]->clear();
  cbAnaProbe[1]->clear();

  for( int i = 0; i < (int) anaProbeList[0].GetAnswers().size(); i++ ) {
    cbAnaProbe[0]->addItem(QString::fromStdString((anaProbeList[0].GetAnswers())[i].second), 
                          QString::fromStdString((anaProbeList[0].GetAnswers())[i].first));
  }
  for( int i = 0; i < cbAnaProbe[0]->count() ; i++) cbAnaProbe[1]->addItem(cbAnaProbe[0]->itemText(i), cbAnaProbe[0]->itemData(i));

  cbDigProbe[0]->clear();
  cbDigProbe[1]->clear();
  cbDigProbe[2]->clear();
  cbDigProbe[3]->clear();
  for( int i = 0; i < (int) digiProbeList[0].GetAnswers().size(); i++ ) {
    cbDigProbe[0]->addItem(QString::fromStdString((digiProbeList[0].GetAnswers())[i].second), 
                          QString::fromStdString((digiProbeList[0].GetAnswers())[i].first));
  }
  for( int i = 0; i < cbDigProbe[0]->count() ; i++) {
    cbDigProbe[1]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
    cbDigProbe[2]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
    cbDigProbe[3]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
  }

  if( digi[index]->GetFPGAType() == DPPType::PHA ) SetupPHA();

  if( digi[index]->GetFPGAType() == DPPType::PSD ) SetupPSD();


  digiMTX[index].lock();
  ReadScopeSettings();
  if( digi[index]->IsAcqOn() ){
    digi[index]->WriteValue(PHA::CH::ChannelEnable, "False", -1);
    digi[index]->WriteValue(PHA::CH::ChannelEnable, "True", cbScopeCh->currentIndex());
  }
  digiMTX[index].unlock();
  allowChange = true;

}

void Scope::CleanUpSettingsGroupBox(){
  QList<QLabel *> labelChildren1 = settingBox->findChildren<QLabel *>();
  for( int i = 0; i < labelChildren1.size(); i++) delete labelChildren1[i];
  
  QList<RComboBox *> labelChildren2 = settingBox->findChildren<RComboBox *>();
  for( int i = 0; i < labelChildren2.size(); i++) delete labelChildren2[i];
  
  QList<RSpinBox *> labelChildren3 = settingBox->findChildren<RSpinBox *>();
  for( int i = 0; i < labelChildren3.size(); i++) delete labelChildren3[i];
}

void Scope::SetupPHA(){

  CleanUpSettingsGroupBox();

  ScopeMakeSpinBox(sbRL, "Record Lenght [ns] ", bLayout, 0, 0, PHA::CH::RecordLength);
  ScopeMakeSpinBox(sbThreshold, "Threshold [LSB] ", bLayout, 0, 2, PHA::CH::TriggerThreshold);
  ScopeMakeComoBox(cbPolarity, "Polarity ", bLayout, 0, 4, PHA::CH::Polarity);
  ScopeMakeComoBox(cbWaveRes, "Wave Re. ", bLayout, 0, 6, PHA::CH::WaveResolution);

  //------------------ next row
  ScopeMakeSpinBox(sbPT, "Pre Trigger [ns] ", bLayout, 1, 0, PHA::CH::PreTrigger);
  ScopeMakeSpinBox(sbDCOffset, "DC offset [%] ", bLayout, 1, 2,  PHA::CH::DC_Offset);
  ScopeMakeSpinBox(sbTimeRiseTime, "Trigger Rise Time [ns] ", bLayout, 1, 4, PHA::CH::TimeFilterRiseTime);
  ScopeMakeSpinBox(sbTimeGuard, "Trigger Guard [ns] ", bLayout, 1, 6, PHA::CH::TimeFilterRetriggerGuard);

  //----------------- next row
  ScopeMakeSpinBox(sbTrapRiseTime, "Trap. Rise Time [ns] ", bLayout, 2, 0, PHA::CH::EnergyFilterRiseTime);
  ScopeMakeSpinBox(sbTrapFlatTop, "Trap. Flat Top [ns] ", bLayout, 2, 2, PHA::CH::EnergyFilterFlatTop);
  ScopeMakeSpinBox(sbTrapPoleZero, "Trap. Pole Zero [ns] ", bLayout, 2, 4, PHA::CH::EnergyFilterPoleZero);
  ScopeMakeSpinBox(sbEnergyFineGain, "Energy Fine Gain ", bLayout, 2, 6, PHA::CH::EnergyFilterFineGain);

  //----------------- next row
  ScopeMakeSpinBox(sbTrapPeaking, "Trap. Peaking [%] ", bLayout, 3, 0, PHA::CH::EnergyFilterPeakingPosition);
  ScopeMakeComoBox(cbTrapPeakAvg, "Trap. Peaking ", bLayout, 3, 2, PHA::CH::EnergyFilterPeakingAvg);
  ScopeMakeSpinBox(sbBaselineGuard, "Baseline Guard [ns] ", bLayout, 3, 4, PHA::CH::EnergyFilterBaselineGuard);
  ScopeMakeComoBox(cbBaselineAvg, "Baseline Avg ", bLayout, 3, 6, PHA::CH::EnergyFilterBaselineAvg);

  //----------------- next row
  ScopeMakeSpinBox(sbPileUpGuard, "Pile-up Guard [ns] ", bLayout, 4, 0, PHA::CH::EnergyFilterPileUpGuard);
  ScopeMakeComoBox(cbLowFreqFilter, "Low Freq. Filter ", bLayout, 4, 2, PHA::CH::EnergyFilterLowFreqFilter);

}

void Scope::SetupPSD(){

  CleanUpSettingsGroupBox();

  ScopeMakeSpinBox(sbRL, "Record Lenght [ns] ", bLayout, 0, 0, PSD::CH::RecordLength);
  ScopeMakeSpinBox(sbThreshold, "Threshold [LSB] ", bLayout, 0, 2, PSD::CH::TriggerThreshold);
  ScopeMakeComoBox(cbPolarity, "Polarity ", bLayout, 0, 4, PSD::CH::Polarity);
  ScopeMakeComoBox(cbWaveRes, "Wave Re. ", bLayout, 0, 6, PSD::CH::WaveResolution);

  //------------------ next row
  ScopeMakeSpinBox(sbPT, "Pre Trigger [ns] ", bLayout, 1, 0, PSD::CH::PreTrigger);
  ScopeMakeSpinBox(sbDCOffset, "DC offset [%] ", bLayout, 1, 2,  PSD::CH::DC_Offset);
  ScopeMakeComoBox(cbbADCInputBaselineAvg, "ADC BL Avg. ", bLayout, 1, 4, PSD::CH::ADCInputBaselineAvg);
  ScopeMakeSpinBox(spbADCInputBaselineGuard, "ADC BL Guard [ns] ", bLayout, 1, 6, PSD::CH::ADCInputBaselineGuard);

  //------------------ next row
  ScopeMakeSpinBox(spbCFDDelay, "CFD Delay [ns] ", bLayout, 2, 0, PSD::CH::CFDDelay);
  ScopeMakeSpinBox(spbCFDFraction, "CFD Frac. [%] ", bLayout, 2, 2, PSD::CH::CFDFraction);
  ScopeMakeComoBox(cbbSmoothingFactor, "Smoothing ", bLayout, 2, 4, PSD::CH::SmoothingFactor);
  ScopeMakeSpinBox(spbAbsBaseline, "Abs. BL ", bLayout, 2, 6, PSD::CH::AbsoluteBaseline);

  //------------------ next row
  ScopeMakeComoBox(cbbTriggerFilter, "Trig. Filter ", bLayout, 3, 0, PSD::CH::TriggerFilterSelection);
  ScopeMakeComoBox(cbbTimeFilterSmoothing, "Trig. Smooth ", bLayout, 3, 2, PSD::CH::TimeFilterSmoothing);
  ScopeMakeSpinBox(spbTimeFilterReTriggerGuard, "Trig. Guard [ns] ", bLayout, 3, 4, PSD::CH::TimeFilterRetriggerGuard);
  ScopeMakeSpinBox(spbPileupGap, "PileUp Gap [ns] ", bLayout, 3, 6, PSD::CH::PileupGap);

  //------------------ next row
  ScopeMakeSpinBox(spbGateLong, "Long Gate [ns] ", bLayout, 4, 0, PSD::CH::GateLongLength);
  ScopeMakeSpinBox(spbGateShort, "Shart Gate [ns] ", bLayout, 4, 2, PSD::CH::GateLongLength);
  ScopeMakeSpinBox(spbGateOffset, "Gate offset [ns] ", bLayout, 4, 4, PSD::CH::GateLongLength);
  ScopeMakeComoBox(cbbEnergyGain, "Energy Gain ", bLayout, 4, 6, PSD::CH::EnergyGain);


}

void Scope::ReadScopeSettings(){

  if( !isVisible() ) return;

  int iDigi = cbScopeDigi->currentIndex();
  if( !digi[iDigi] || digi[iDigi]->IsDummy() || !digi[iDigi]->IsConnected()) return;

  UpdateSettingsFromMemeory();

}

void Scope::UpdateSettingsFromMemeory(){
  if( !isVisible() ) return;

  printf("Scope::%s\n", __func__);

  int iDigi = cbScopeDigi->currentIndex();
  if( !digi[iDigi] || digi[iDigi]->IsDummy() || !digi[iDigi]->IsConnected()) return;

  allowChange = false;

  int ch = cbScopeCh->currentIndex();

  for( int i = 0 ; i < 2; i++){
    if( digi[iDigi]->GetFPGAType() == DPPType::PHA ) ScopeReadComboBoxValue(iDigi, ch, cbAnaProbe[i], PHA::CH::AnalogProbe[i]);
    if( digi[iDigi]->GetFPGAType() == DPPType::PSD ) ScopeReadComboBoxValue(iDigi, ch, cbAnaProbe[i], PSD::CH::AnalogProbe[i]);
  }

  for( int i = 0 ; i < 4; i++){
    if( digi[iDigi]->GetFPGAType() == DPPType::PHA )  ScopeReadComboBoxValue(iDigi, ch, cbDigProbe[i], PHA::CH::DigitalProbe[i]);
    if( digi[iDigi]->GetFPGAType() == DPPType::PSD )  ScopeReadComboBoxValue(iDigi, ch, cbDigProbe[i], PSD::CH::DigitalProbe[i]);
  }

  if(  digi[iDigi]->GetFPGAType() == DPPType::PHA ){
    ScopeReadComboBoxValue(iDigi, ch, cbPolarity, PHA::CH::Polarity);
    ScopeReadComboBoxValue(iDigi, ch, cbWaveRes, PHA::CH::WaveResolution);
    ScopeReadComboBoxValue(iDigi, ch, cbTrapPeakAvg, PHA::CH::EnergyFilterPeakingAvg);
    ScopeReadComboBoxValue(iDigi, ch, cbBaselineAvg, PHA::CH::EnergyFilterBaselineAvg);
    ScopeReadComboBoxValue(iDigi, ch, cbLowFreqFilter, PHA::CH::EnergyFilterLowFreqFilter);

    ScopeReadSpinBoxValue(iDigi, ch, sbRL, PHA::CH::RecordLength);
    ScopeReadSpinBoxValue(iDigi, ch, sbPT, PHA::CH::PreTrigger);
    ScopeReadSpinBoxValue(iDigi, ch, sbDCOffset, PHA::CH::DC_Offset);
    ScopeReadSpinBoxValue(iDigi, ch, sbThreshold, PHA::CH::TriggerThreshold);
    ScopeReadSpinBoxValue(iDigi, ch, sbTimeRiseTime, PHA::CH::TimeFilterRiseTime);
    ScopeReadSpinBoxValue(iDigi, ch, sbTimeGuard, PHA::CH::TimeFilterRetriggerGuard);
    ScopeReadSpinBoxValue(iDigi, ch, sbTrapRiseTime, PHA::CH::EnergyFilterRiseTime);
    ScopeReadSpinBoxValue(iDigi, ch, sbTrapFlatTop, PHA::CH::EnergyFilterFlatTop);
    ScopeReadSpinBoxValue(iDigi, ch, sbTrapPoleZero, PHA::CH::EnergyFilterPoleZero);
    ScopeReadSpinBoxValue(iDigi, ch, sbEnergyFineGain, PHA::CH::EnergyFilterFineGain);
    ScopeReadSpinBoxValue(iDigi, ch, sbTrapPeaking, PHA::CH::EnergyFilterPeakingPosition);
    ScopeReadSpinBoxValue(iDigi, ch, sbBaselineGuard, PHA::CH::EnergyFilterBaselineGuard);
    ScopeReadSpinBoxValue(iDigi, ch, sbPileUpGuard, PHA::CH::EnergyFilterPileUpGuard);

    sbRL->setStyleSheet("");
    sbPT->setStyleSheet("");
    sbThreshold->setStyleSheet("");
    sbTimeRiseTime->setStyleSheet("");
    sbTimeGuard->setStyleSheet("");
    sbTrapRiseTime->setStyleSheet("");
    sbTrapFlatTop->setStyleSheet("");
    sbTrapPoleZero->setStyleSheet("");
    sbEnergyFineGain->setStyleSheet("");
    sbTrapPeaking->setStyleSheet("");
    sbBaselineGuard->setStyleSheet("");
    sbPileUpGuard->setStyleSheet("");
  }

  if(  digi[iDigi]->GetFPGAType() == DPPType::PSD ){

    ScopeReadComboBoxValue(iDigi, ch, cbPolarity, PSD::CH::Polarity);
    ScopeReadComboBoxValue(iDigi, ch, cbWaveRes, PSD::CH::WaveResolution);
    ScopeReadComboBoxValue(iDigi, ch, cbbADCInputBaselineAvg, PSD::CH::ADCInputBaselineAvg);
    ScopeReadComboBoxValue(iDigi, ch, cbbSmoothingFactor,  PSD::CH::SmoothingFactor);
    ScopeReadComboBoxValue(iDigi, ch, cbbTriggerFilter,  PSD::CH::TriggerFilterSelection);
    ScopeReadComboBoxValue(iDigi, ch, cbbTimeFilterSmoothing, PSD::CH::TimeFilterSmoothing);
    ScopeReadComboBoxValue(iDigi, ch, cbbEnergyGain, PSD::CH::EnergyGain);


    ScopeReadSpinBoxValue(iDigi, ch, sbRL, PSD::CH::RecordLength);
    ScopeReadSpinBoxValue(iDigi, ch, sbThreshold,  PSD::CH::TriggerThreshold);
    ScopeReadSpinBoxValue(iDigi, ch, sbPT,  PSD::CH::PreTrigger);
    ScopeReadSpinBoxValue(iDigi, ch, sbDCOffset,  PSD::CH::DC_Offset);
    ScopeReadSpinBoxValue(iDigi, ch, spbADCInputBaselineGuard,  PSD::CH::ADCInputBaselineGuard);
    ScopeReadSpinBoxValue(iDigi, ch, spbCFDDelay,  PSD::CH::CFDDelay);
    ScopeReadSpinBoxValue(iDigi, ch, spbCFDFraction, PSD::CH::CFDFraction);
    ScopeReadSpinBoxValue(iDigi, ch, spbAbsBaseline,  PSD::CH::AbsoluteBaseline);
    ScopeReadSpinBoxValue(iDigi, ch, spbTimeFilterReTriggerGuard,  PSD::CH::TimeFilterRetriggerGuard);
    ScopeReadSpinBoxValue(iDigi, ch, spbPileupGap,  PSD::CH::PileupGap);
    ScopeReadSpinBoxValue(iDigi, ch, spbGateLong, PSD::CH::GateLongLength);
    ScopeReadSpinBoxValue(iDigi, ch, spbGateShort, PSD::CH::GateShortLength);
    ScopeReadSpinBoxValue(iDigi, ch, spbGateOffset, PSD::CH::GateOffset);

    cbPolarity->setStyleSheet("");
    cbWaveRes->setStyleSheet("");
    cbbADCInputBaselineAvg->setStyleSheet("");
    cbbSmoothingFactor->setStyleSheet("");
    cbbTriggerFilter->setStyleSheet("");
    cbbTimeFilterSmoothing->setStyleSheet("");
    cbbEnergyGain->setStyleSheet("");

    sbRL->setStyleSheet("");
    sbThreshold->setStyleSheet("");
    sbPT->setStyleSheet("");
    sbDCOffset->setStyleSheet("");
    spbADCInputBaselineGuard->setStyleSheet("");
    spbCFDDelay->setStyleSheet("");
    spbCFDFraction->setStyleSheet("");
    spbAbsBaseline->setStyleSheet("");
    spbTimeFilterReTriggerGuard->setStyleSheet("");
    spbPileupGap->setStyleSheet("");
    spbGateLong->setStyleSheet("");
    spbGateShort->setStyleSheet("");
    spbGateOffset->setStyleSheet("");

  }


  allowChange = true;
}

void Scope::StartScope(){

  if( !digi ) return; 

  for( int iDigi = 0 ; iDigi < nDigi; iDigi ++ ){

    if( digi[iDigi]->IsDummy() ) return;

    int ch = cbScopeCh->currentIndex();

    //*---- set digitizer to take full trace; since in scope mode, no data saving, speed would be fast (How fast?)
    //* when the input rate is faster than trigger rate, Digitizer will stop data taking.

    ReadScopeSettings();

    /// the settings are the same for PHA and PSD
    for( int ch2 = 0 ; ch2 < digi[iDigi]->GetNChannels(); ch2 ++){
      channelEnable[iDigi][ch2] = digi[iDigi]->ReadValue(PHA::CH::ChannelEnable, ch2);
    }

    digi[iDigi]->WriteValue(PHA::CH::ChannelEnable, "False", -1);

    if( iDigi == cbScopeDigi->currentIndex() ){
      digi[iDigi]->WriteValue(PHA::CH::ChannelEnable, "True", ch);

      waveSaving =  digi[iDigi]->ReadValue(PHA::CH::WaveSaving, ch);
      digi[iDigi]->WriteValue(PHA::CH::WaveSaving, "Always", ch);

      waveTriggerSource = digi[iDigi]->ReadValue(PHA::CH::WaveTriggerSource, ch);
      digi[iDigi]->WriteValue(PHA::CH::WaveTriggerSource, "ChSelfTrigger", ch);
    }

    originalValueSet = true;

    digi[iDigi]->SetDataFormat(DataFormat::ALL); 
    digi[iDigi]->StartACQ();

    readDataThread[iDigi]->SetSaveData(false);
    readDataThread[iDigi]->start();

    updateTraceThread->start();

    ScopeControlOnOff(false);
    emit TellSettingsPanelControlOnOff();
  }
  emit TellACQOnOff(true);
  allowChange = true;
}

void Scope::StopScope(){

  printf("%s\n", __func__);

  updateTraceThread->Stop();
  updateTraceThread->quit();
  updateTraceThread->wait();

  /// the settings are the same for PHA and PSD

  if(digi){
    for(int i = 0; i < nDigi; i++){
      if( digi[i]->IsDummy() ) continue;

      readDataThread[i]->Stop();
      readDataThread[i]->quit();
      readDataThread[i]->wait();

      digiMTX[i].lock();
      digi[i]->StopACQ();
      if( originalValueSet ){
        for( int ch2 = 0 ; ch2 < digi[i]->GetNChannels(); ch2 ++){
          digi[i]->WriteValue(PHA::CH::ChannelEnable, channelEnable[i][ch2], ch2);
        }
        if( i == cbScopeDigi->currentIndex() ) {
          digi[i]->WriteValue(PHA::CH::WaveTriggerSource, waveTriggerSource, cbScopeCh->currentIndex());
          digi[i]->WriteValue(PHA::CH::WaveSaving, waveSaving, cbScopeCh->currentIndex());
        }
      }
      digiMTX[i].unlock();
    }
    
    emit TellACQOnOff(false);
  }

  originalValueSet = false;

  ScopeControlOnOff(true);
  emit TellSettingsPanelControlOnOff();

  allowChange = true;
}

void Scope::UpdateScope(){

  int iDigi = cbScopeDigi->currentIndex();
  int ch = cbScopeCh->currentIndex();
  int sample2ns = PHA::TraceStep * (1 << cbWaveRes->currentIndex());

  emit UpdateScalar();

  /// the settings are the same for PHA and PSD

  if( digi ){

    digiMTX[iDigi].lock();    
    std::string time = digi[iDigi]->ReadValue(PHA::CH::ChannelRealtime, ch); // for refreashing SelfTrgRate and SavedCount
    std::string haha = digi[iDigi]->ReadValue(PHA::CH::SelfTrgRate, ch);
    leTriggerRate->setText(QString::fromStdString(haha));

    //unsigned int traceLength = qMin((int) digi[iDigi]->hit->traceLenght, MaxDisplayTraceDataLength);
    unsigned int traceLength = qMin( atoi(digi[iDigi]->GetSettingValue(PHA::CH::RecordLength, ch).c_str())/sample2ns,   MaxDisplayTraceDataLength  );

    if( atoi(haha.c_str()) == 0 ) {
      digiMTX[iDigi].unlock();

      for( int j = 0; j < 6; j++){
        QVector<QPointF> points;
        for( unsigned int i = 0 ; i < traceLength; i++) points.append(QPointF(sample2ns * i , j > 1 ? 0 : (j+1)*1000));
        dataTrace[j]->replace(points);
      }
      plot->axes(Qt::Horizontal).first()->setRange(0, sample2ns * traceLength);
      return;
    }
    
    //printf("%s, traceLength : %d , %d\n", __func__, traceLength, digi[iDigi]->hit->analog_probes[0][10]);

    for( int j = 0; j < 2; j++) {
      QVector<QPointF> points;
      for( unsigned int i = 0 ; i < traceLength; i++) points.append(QPointF(sample2ns * i , digi[iDigi]->hit->analog_probes[j][i]));
      dataTrace[j]->replace(points);
    }
    for( int j = 0; j < 4; j++) {
      QVector<QPointF> points;
      for( unsigned int i = 0 ; i < traceLength; i++) points.append(QPointF(sample2ns * i , (j+1)*5000 + 4000*digi[iDigi]->hit->digital_probes[j][i]));
      dataTrace[j+2]->replace(points);
    }
    //digi[iDigi]->hit->ClearTrace();
    digiMTX[iDigi].unlock();
    plot->axes(Qt::Horizontal).first()->setRange(0, sample2ns * traceLength);

  }

}

void Scope::ProbeChange(RComboBox * cb[], const int size ){

  if( allowChange == false ) return;

  //printf("%s\n", __func__);
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

  //int ID = cbScopeDigi->currentIndex();
  //digiMTX[ID].lock();
  if( size == 2) {// analog probes
    for( int j = 0; j < 2; j++ )dataTrace[j]->setName(cb[j]->currentText());
  }
  if( size == 4){ // digitial probes
    for( int j = 2; j < 6; j++ )dataTrace[j]->setName(cb[j-2]->currentText());
  }
  //digiMTX[ID].unlock();

}

void Scope::ScopeControlOnOff(bool on){

  bnScopeStop->setEnabled(!on);
  bnScopeStart->setEnabled(on);
  bnScopeReset->setEnabled(on);
  bnScopeReadSettings->setEnabled(on);

  if( digi[cbScopeDigi->currentIndex()]->GetFPGAType() == DPPType::PHA ){ 
    sbRL->setEnabled(on);
    sbPT->setEnabled(on);
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
    cbBaselineAvg->setEnabled(on);

    //sbDCOffset->setEnabled(on);
    //sbThreshold->setEnabled(on);
    //sbBaselineGuard->setEnabled(on);
    //sbPileUpGuard->setEnabled(on);
    //cbLowFreqFilter->setEnabled(on);
  }

  if( digi[cbScopeDigi->currentIndex()]->GetFPGAType() == DPPType::PSD ){

    cbPolarity->setEnabled(on);
    cbWaveRes->setEnabled(on);
    cbbADCInputBaselineAvg->setEnabled(on);
    cbbSmoothingFactor->setEnabled(on);
    cbbTriggerFilter->setEnabled(on);
    cbbTimeFilterSmoothing->setEnabled(on);
    cbbEnergyGain->setEnabled(on);

    sbRL->setEnabled(on);
    sbPT->setEnabled(on);
    
    //sbThreshold->setEnabled(on);
    //sbDCOffset->setEnabled(on);
    
    //spbADCInputBaselineGuard->setEnabled(on);
    //spbCFDDelay->setEnabled(on);
    //spbCFDFraction->setEnabled(on);
    //spbAbsBaseline->setEnabled(on);
    //spbTimeFilterReTriggerGuard->setEnabled(on);
    //spbPileupGap->setEnabled(on);
    //spbGateLong->setEnabled(on);
    //spbGateShort->setEnabled(on);
    //spbGateOffset->setEnabled(on);

  }

}

void Scope::ScopeReadSpinBoxValue(int iDigi, int ch, RSpinBox *sb, const Reg digPara){
  std::string ans = digi[iDigi]->GetSettingValue(digPara, ch);
  sb->setValue(atoi(ans.c_str()));
}

void Scope::ScopeReadComboBoxValue(int iDigi, int ch, RComboBox *cb, const Reg digPara){
  std::string ans = digi[iDigi]->GetSettingValue(digPara, ch);
  int index = cb->findData(QString::fromStdString(ans));
  if( index >= 0 && index < cb->count()) {
    cb->setCurrentIndex(index);
  }else{
    qDebug() << QString::fromStdString(ans);
  }
}

void Scope::ScopeMakeSpinBox(RSpinBox * &sb, QString str, QGridLayout *layout, int row, int col, const Reg digPara){
  //printf("%s\n", __func__);
  QLabel * lb = new QLabel(str, settingBox);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lb, row, col);
  sb = new RSpinBox(settingBox);
  sb->setMinimum(atof(digPara.GetAnswers()[0].first.c_str()));
  sb->setMaximum(atof(digPara.GetAnswers()[1].first.c_str()));
  sb->setSingleStep(atof(digPara.GetAnswers()[2].first.c_str()));
  layout->addWidget(sb, row, col+1);
  connect(sb, &RSpinBox::valueChanged, this, [=](){
    if( !allowChange ) return;
    sb->setStyleSheet("color:blue");
  });
  connect(sb, &RSpinBox::returnPressed, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    if( sb->decimals() == 0 && sb->singleStep() != 1) {
      double step = sb->singleStep();
      double value = sb->value();
      sb->setValue( (std::round(value/step)*step));
    }

    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    QString msg;
    msg = QString::fromStdString(digPara.GetPara()) + "|DIG:"+ QString::number(digi[iDigi]->GetSerialNumber()) + ",CH:" + (ch == -1 ? "All" : QString::number(ch));
    msg += " = " + QString::number(sb->value());
    if( digi[iDigi]->WriteValue(digPara, std::to_string(sb->value()), ch)){
      SendLogMsg(msg + "|OK.");
      sb->setStyleSheet("");
      UpdateSettingsFromMemeory();
      UpdateOtherPanels();
    }else{
      SendLogMsg(msg + "|Fail.");
      sb->setStyleSheet("color:red;");
    }
  });


}

void Scope::ScopeMakeComoBox(RComboBox * &cb, QString str, QGridLayout *layout, int row, int col, const Reg digPara){
  QLabel * lb = new QLabel(str, settingBox);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lb, row, col);

  cb = new RComboBox(settingBox);
  for( int i = 0 ; i < (int) digPara.GetAnswers().size(); i++){
    cb->addItem(QString::fromStdString((digPara.GetAnswers())[i].second), QString::fromStdString((digPara.GetAnswers())[i].first));
  }
  layout->addWidget(cb, row, col+1);

  connect(cb, &RComboBox::currentIndexChanged, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    QString msg;
    msg = QString::fromStdString(digPara.GetPara()) + "|DIG:"+ QString::number(digi[iDigi]->GetSerialNumber()) + ",CH:" + (ch == -1 ? "All" : QString::number(ch));
    msg += " = " + cb->currentData().toString();
    if( digi[iDigi]->WriteValue(digPara, cb->currentData().toString().toStdString(), ch)){
      SendLogMsg(msg + "|OK.");
      cb->setStyleSheet("");
      UpdateSettingsFromMemeory();
      UpdateOtherPanels();
    }else{
      SendLogMsg(msg + "|Fail.");
      cb->setStyleSheet("color:red;");
    }
  });
}



