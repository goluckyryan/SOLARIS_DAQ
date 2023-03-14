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
  this->readDataThread = readDataThread;

  setWindowTitle("Scope");
  setGeometry(0, 0, 1000, 800);  
  setWindowFlags( this->windowFlags() & ~Qt::WindowCloseButtonHint );

  allowChange = false;

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
  cbScopeDigi = new RComboBox(this);
  cbScopeCh = new RComboBox(this);
  layout->addWidget(cbScopeDigi, rowID, 0);
  layout->addWidget(cbScopeCh, rowID, 1);

  connect(cbScopeDigi, &RComboBox::currentIndexChanged, this, [=](){
    //if( allowChange ) StopScope();
    int index = cbScopeDigi->currentIndex();
    if( index == -1 ) return;
    for( int i = 0; i < digi[index]->GetNChannels(); i++){
      cbScopeCh->addItem("ch-" + QString::number(i), i);
    }
    //if( allowChange )StartScope(index);
  });

  connect(cbScopeCh, &RComboBox::currentIndexChanged, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    digiMTX.lock();
    digi[iDigi]->WriteValue(PHA::CH::ChannelEnable, "False", -1);
    digi[iDigi]->WriteValue(PHA::CH::ChannelEnable, "True", ch);
    ReadScopeSettings();
    UpdateSettingsPanel();
    digiMTX.unlock();
  });

  allowChange = false;
  cbScopeDigi->clear(); ///this will also trigger RComboBox::currentIndexChanged
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
    SendLogMsg("Reset Digi-" + QString::number(digi[iDigi]->GetSerialNumber()) + " and Set Default PHA.");
  });

  bnScopeReadSettings = new QPushButton("Read Ch. Settings", this);
  layout->addWidget(bnScopeReadSettings, rowID, 3);
  connect(bnScopeReadSettings, &QPushButton::clicked, this, [=](){
    if( !allowChange ) return;
    ReadScopeSettings();
  });

  //TODO----- add copy settings and paste settings
  chkSetAllChannel = new QCheckBox("apply to all channels", this);
  layout->addWidget(chkSetAllChannel, rowID, 4);

  //------------ Probe selection
  rowID ++;
  //TODO --- add None
  cbAnaProbe[0] = new RComboBox(this);
  for( int i = 0; i < (int) PHA::CH::WaveAnalogProbe0.GetAnswers().size(); i++ ) {
    cbAnaProbe[0]->addItem(QString::fromStdString((PHA::CH::WaveAnalogProbe0.GetAnswers())[i].second), 
                           QString::fromStdString((PHA::CH::WaveAnalogProbe0.GetAnswers())[i].first));
  }

  cbAnaProbe[1] = new RComboBox(this);
  for( int i = 0; i < cbAnaProbe[0]->count() ; i++) cbAnaProbe[1]->addItem(cbAnaProbe[0]->itemText(i), cbAnaProbe[0]->itemData(i));

  connect(cbAnaProbe[0], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbAnaProbe, 2);});
  connect(cbAnaProbe[1], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbAnaProbe, 2);});

  cbAnaProbe[0]->setCurrentIndex(1); ///trigger the AnaProbeChange
  cbAnaProbe[0]->setCurrentIndex(0);
  cbAnaProbe[1]->setCurrentIndex(4);

  connect(cbAnaProbe[0], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX.lock();
    digi[iDigi]->WriteValue(PHA::CH::WaveAnalogProbe0, (cbAnaProbe[0]->currentData()).toString().toStdString(), ch);
    digiMTX.unlock();
  });
  
  connect(cbAnaProbe[1], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX.lock();
    digi[iDigi]->WriteValue(PHA::CH::WaveAnalogProbe1, (cbAnaProbe[1]->currentData()).toString().toStdString(), ch);
    digiMTX.unlock();
  });

  //TODO --- add None
  cbDigProbe[0] = new RComboBox(this);
  for( int i = 0; i < (int) PHA::CH::WaveDigitalProbe0.GetAnswers().size(); i++ ) {
    cbDigProbe[0]->addItem(QString::fromStdString((PHA::CH::WaveDigitalProbe0.GetAnswers())[i].second), 
                           QString::fromStdString((PHA::CH::WaveDigitalProbe0.GetAnswers())[i].first));
  }

  cbDigProbe[1] = new RComboBox(this);
  cbDigProbe[2] = new RComboBox(this);
  cbDigProbe[3] = new RComboBox(this);
  for( int i = 0; i < cbDigProbe[0]->count() ; i++) {
    cbDigProbe[1]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
    cbDigProbe[2]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
    cbDigProbe[3]->addItem(cbDigProbe[0]->itemText(i), cbDigProbe[0]->itemData(i));
  }

  connect(cbDigProbe[0], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[1], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[2], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});
  connect(cbDigProbe[3], &RComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbDigProbe, 4);});

  cbDigProbe[0]->setCurrentIndex(1); ///trigger the DigProbeChange
  cbDigProbe[0]->setCurrentIndex(0);
  cbDigProbe[1]->setCurrentIndex(4);
  cbDigProbe[2]->setCurrentIndex(5);
  cbDigProbe[3]->setCurrentIndex(6);

  connect(cbDigProbe[0], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX.lock();
    digi[iDigi]->WriteValue(PHA::CH::WaveDigitalProbe0, (cbDigProbe[0]->currentData()).toString().toStdString(), ch);
    digiMTX.unlock();
  });
  connect(cbDigProbe[1], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX.lock();
    digi[iDigi]->WriteValue(PHA::CH::WaveDigitalProbe1, (cbDigProbe[1]->currentData()).toString().toStdString(), ch);
    digiMTX.unlock();
  });
  connect(cbDigProbe[2], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX.lock();
    digi[iDigi]->WriteValue(PHA::CH::WaveDigitalProbe2, (cbDigProbe[2]->currentData()).toString().toStdString(), ch);
    digiMTX.unlock();
  });
  connect(cbDigProbe[3], &RComboBox::currentIndexChanged, this, [=](){ 
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();
    if( chkSetAllChannel->isChecked() ) ch = -1;
    digiMTX.lock();
    digi[iDigi]->WriteValue(PHA::CH::WaveDigitalProbe3, (cbDigProbe[3]->currentData()).toString().toStdString(), ch);
    digiMTX.unlock();
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
    QGroupBox * box = new QGroupBox("Channel Settings (need ACQ stop)", this);
    layout->addWidget(box, rowID, 0, 1, 6);

    QGridLayout * bLayout = new QGridLayout(box);
    bLayout->setSpacing(0);

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
    ScopeMakeSpinBox(sbPileUpGuard, "Pile-up Guard [ns] ", bLayout, 4, 0, PHA::CH::EnergyFilterPileUpGuard);
    ScopeMakeComoBox(cbLowFreqFilter, "Low Freq. Filter ", bLayout, 4, 2, PHA::CH::EnergyFilterLowFreqFilter);

  }

  //------------ plot view
  rowID ++;
  TraceView * plotView = new TraceView(plot);
  plotView->setRenderHints(QPainter::Antialiasing);
  layout->addWidget(plotView, rowID, 0, 1, 6);

  //------------- Ketbinding
  rowID ++;
  QLabel * lbhints = new QLabel("Type 'r' to restore view.", this);
  layout->addWidget(lbhints, rowID, 0, 1, 3);
  
  QLabel * lbinfo = new QLabel("Trace update every " + QString::number(updateTraceThread->GetWaitTimeSec()) + " sec.", this);
  lbinfo->setAlignment(Qt::AlignRight);
  layout->addWidget(lbinfo, rowID, 5);

  rowID ++;
  QLabel * lbinfo2 = new QLabel("Maximum time range is " + QString::number(MaxDisplayTraceDataLength * PHA::TraceStep) + " ns due to processing speed.", this);
  layout->addWidget(lbinfo2, rowID, 0, 1, 5);


  //------------ close button
  rowID ++;
  bnScopeStart = new QPushButton("Start", this);
  layout->addWidget(bnScopeStart, rowID, 0);
  bnScopeStart->setEnabled(false);
  connect(bnScopeStart, &QPushButton::clicked, this, [=](){this->StartScope();});

  bnScopeStop = new QPushButton("Stop", this);
  layout->addWidget(bnScopeStop, rowID, 1);
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

void Scope::ReadScopeSettings(){

  int iDigi = cbScopeDigi->currentIndex();
  int ch = cbScopeCh->currentIndex();

  if( !digi[iDigi] && digi[iDigi]->IsDummy() ) return;

  printf("%s\n", __func__);

  allowChange = false;

  for( int i = 0 ; i < 2; i++){
    ScopeReadComboBoxValue(iDigi, ch, cbAnaProbe[i], PHA::CH::AnalogProbe[i]);
  }

  for( int i = 0 ; i < 4; i++){
    ScopeReadComboBoxValue(iDigi, ch, cbDigProbe[i], PHA::CH::DigitalProbe[i]);
  }

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

  ReadScopeSettings();

  digi[iDigi]->WriteValue(PHA::CH::ChannelEnable, "False", -1);
  digi[iDigi]->WriteValue(PHA::CH::ChannelEnable, "True", ch);
  digi[iDigi]->SetPHADataFormat(0);

  digi[iDigi]->StartACQ();

  readDataThread[iDigi]->SetSaveData(false);
  readDataThread[iDigi]->start();

  updateTraceThread->start();

  ScopeControlOnOff(false);
  emit TellSettingsPanelControlOnOff();

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
      digi[i]->WriteValue(PHA::CH::ChannelEnable, "True", -1);
      digiMTX.unlock();

      readDataThread[i]->quit();
      readDataThread[i]->wait();

    }
  }

  ScopeControlOnOff(true);
  emit TellSettingsPanelControlOnOff();

  allowChange = true;
}

void Scope::UpdateScope(){

  int iDigi = cbScopeDigi->currentIndex();
  int ch = cbScopeCh->currentIndex();
  int sample2ns = PHA::TraceStep * (1 << cbWaveRes->currentIndex());

  emit UpdateScalar();

  if( digi ){

    digiMTX.lock();    
    std::string time = digi[iDigi]->ReadValue(PHA::CH::ChannelRealtime, ch); // for refreashing SelfTrgRate and SavedCount
    std::string haha = digi[iDigi]->ReadValue(PHA::CH::SelfTrgRate, ch);
    leTriggerRate->setText(QString::fromStdString(haha));
    if( atoi(haha.c_str()) == 0 ) {
      digiMTX.unlock();

      for( int j = 0; j < 4; j++){
        QVector<QPointF> points;
        for( int i = 0 ; i < dataTrace[j]->count(); i++) points.append(QPointF(sample2ns * i , j > 1 ? 0 : (j+1)*1000));
        dataTrace[j]->replace(points);
      }
      return;
    }
    
    unsigned int traceLength = qMin((int) digi[iDigi]->evt->traceLenght, MaxDisplayTraceDataLength);

    for( int j = 0; j < 2; j++) {
      QVector<QPointF> points;
      for( unsigned int i = 0 ; i < traceLength; i++) points.append(QPointF(sample2ns * i , digi[iDigi]->evt->analog_probes[j][i]));
      dataTrace[j]->replace(points);
    }
    for( int j = 0; j < 4; j++) {
      QVector<QPointF> points;
      for( unsigned int i = 0 ; i < traceLength; i++) points.append(QPointF(sample2ns * i , (j+1)*1000 + 4000*digi[iDigi]->evt->digital_probes[j][i]));
      dataTrace[j+2]->replace(points);
    }
    digiMTX.unlock();
    plot->axes(Qt::Horizontal).first()->setRange(0, sample2ns * traceLength);

  }

}

void Scope::ProbeChange(RComboBox * cb[], const int size ){

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

void Scope::ScopeReadSpinBoxValue(int iDigi, int ch, RSpinBox *sb, const Reg digPara){
  std::string ans = digi[iDigi]->ReadValue(digPara, ch);
  sb->setValue(atoi(ans.c_str()));
}

void Scope::ScopeReadComboBoxValue(int iDigi, int ch, RComboBox *cb, const Reg digPara){
  std::string ans = digi[iDigi]->ReadValue(digPara, ch);
  int index = cb->findData(QString::fromStdString(ans));
  if( index >= 0 && index < cb->count()) {
    cb->setCurrentIndex(index);
  }else{
    qDebug() << QString::fromStdString(ans);
  }
}

void Scope::ScopeMakeSpinBox(RSpinBox * &sb, QString str, QGridLayout *layout, int row, int col, const Reg digPara){
  QLabel * lb = new QLabel(str, this);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lb, row, col);
  sb = new RSpinBox(this);
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
      
      //TODO digiSettingPanel update setting
      printf("UpdateSettingsPanel \n");
      emit UpdateSettingsPanel();

    }else{
      SendLogMsg(msg + "|Fail.");
      sb->setStyleSheet("color:red;");
    }

  });


}

void Scope::ScopeMakeComoBox(RComboBox * &cb, QString str, QGridLayout *layout, int row, int col, const Reg digPara){
  QLabel * lb = new QLabel(str, this);
  lb->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lb, row, col);

  cb = new RComboBox(this);
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
      //TODO digiSettingPanel update setting
      printf("UpdateSettingsPanel \n");
      emit UpdateSettingsPanel();
    }else{
      SendLogMsg(msg + "|Fail.");
      cb->setStyleSheet("color:red;");
    }
  });
}



