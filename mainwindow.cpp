#include "mainwindow.h"

#include <QLabel>
#include <QGridLayout>
#include <QDialog>
#include <QFileDialog>
#include <QStorageInfo>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QVariant>
#include <QChartView>
#include <QValueAxis>
#include <QStandardItemModel>

#include <unistd.h>

//------ static memeber
Digitizer2Gen ** MainWindow::digi = NULL;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent){

  setWindowTitle("SOLARIS DAQ");
  setGeometry(500, 100, 1000, 500);
  QIcon icon("SOLARIS_favicon.png");
  setWindowIcon(icon);

  nDigi = 0;
  digiSerialNum.clear();
  digiSetting = NULL;
  readDataThread = NULL;

  SetUpPlot();

  QWidget * mainLayoutWidget = new QWidget(this);
  setCentralWidget(mainLayoutWidget);
  QVBoxLayout * layoutMain = new QVBoxLayout(mainLayoutWidget);
  mainLayoutWidget->setLayout(layoutMain);


  {//====================== General
    QGroupBox * box1 = new QGroupBox("General", mainLayoutWidget);
    layoutMain->addWidget(box1);

    QGridLayout * layout1 = new QGridLayout(box1);

    bnProgramSettings = new QPushButton("Program Settings", this);
    connect(bnProgramSettings, &QPushButton::clicked, this, &MainWindow::ProgramSettings);

    bnNewExp = new QPushButton("New/Change/Reload Exp", this);
    connect(bnNewExp, &QPushButton::clicked, this, &MainWindow::SetupNewExp);

    QLabel * lExpName = new QLabel("Exp Name ", this);
    lExpName->setAlignment(Qt::AlignRight | Qt::AlignCenter);

    leExpName = new QLineEdit("<Exp Name>", this);
    leExpName->setAlignment(Qt::AlignHCenter);
    leExpName->setReadOnly(true);

    bnOpenScope = new QPushButton("Open scope", this);
    bnOpenScope->setEnabled(false);
    connect(bnOpenScope, &QPushButton::clicked, this, &MainWindow::OpenScope);

    bnOpenDigitizers = new QPushButton("Open Digitizers", this);
    connect(bnOpenDigitizers, SIGNAL(clicked()), this, SLOT(OpenDigitizers()));

    bnCloseDigitizers = new QPushButton("Close Digitizers", this);
    bnCloseDigitizers->setEnabled(false);
    connect(bnCloseDigitizers, SIGNAL(clicked()), this, SLOT(CloseDigitizers()));
  
    bnDigiSettings = new QPushButton("Digitizers Settings", this);
    bnDigiSettings->setEnabled(false);
    connect(bnDigiSettings, SIGNAL(clicked()), this, SLOT(OpenDigitizersSettings()));

    bnSOLSettings = new QPushButton("SOLARIS Settings", this);
    bnSOLSettings->setEnabled(false);

    layout1->addWidget(bnProgramSettings, 0, 0);
    layout1->addWidget(bnNewExp, 0, 1);
    layout1->addWidget(lExpName, 0, 2);
    layout1->addWidget(leExpName, 0, 3);

    layout1->addWidget(bnOpenScope, 1, 0);
    layout1->addWidget(bnOpenDigitizers, 1, 1);
    layout1->addWidget(bnCloseDigitizers, 1, 2, 1, 2);

    layout1->addWidget(bnDigiSettings, 2, 1);
    layout1->addWidget(bnSOLSettings, 2, 2, 1, 2);


    layout1->setColumnStretch(0, 2);
    layout1->setColumnStretch(1, 2);
    layout1->setColumnStretch(2, 1);
    layout1->setColumnStretch(3, 1);

  }


  {//====================== ACD control
    QGroupBox * box2 = new QGroupBox("ACQ control", mainLayoutWidget);
    layoutMain->addWidget(box2);

    QGridLayout * layout2 = new QGridLayout(box2);
    
    QLabel * lbRawDataPath = new QLabel("Raw Data Path : ", this);
    lbRawDataPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    leRawDataPath = new QLineEdit(this);
    leRawDataPath->setReadOnly(true);
    leRawDataPath->setStyleSheet("background-color: #F3F3F3;");

    bnStartACQ = new QPushButton("Start ACQ", this);
    bnStartACQ->setEnabled(false);
    connect(bnStartACQ, &QPushButton::clicked, this, &MainWindow::StartACQ);
    
    bnStopACQ = new QPushButton("Stop ACQ", this);
    bnStopACQ->setEnabled(false);
    connect(bnStopACQ, &QPushButton::clicked, this, &MainWindow::StopACQ);

    QLabel * lbRunID = new QLabel("Run ID : ", this); 
    lbRunID->setAlignment(Qt::AlignRight | Qt::AlignCenter);

    leRunID = new QLineEdit(this);
    leRunID->setAlignment(Qt::AlignHCenter);
    leRunID->setReadOnly(true);
    
    QLabel * lbRunComment = new QLabel("Run Comment : ", this);
    lbRunComment->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    QLineEdit * runComment = new QLineEdit(this);
    runComment->setReadOnly(true);
    
    layout2->addWidget(lbRawDataPath, 0, 0);
    layout2->addWidget(leRawDataPath, 0, 1, 1, 3);

    layout2->addWidget(lbRunID,    1, 0);
    layout2->addWidget(leRunID,    1, 1);
    layout2->addWidget(bnStartACQ, 1, 2);
    layout2->addWidget(bnStopACQ,  1, 3);

    layout2->addWidget(lbRunComment, 2, 0);
    layout2->addWidget(runComment,   2, 1, 1, 3);

    layout2->setColumnStretch(0, 1);
    layout2->setColumnStretch(1, 1);
    layout2->setColumnStretch(2, 2);
    layout2->setColumnStretch(3, 2);

  }

  layoutMain->addStretch();

  {//===================== Log Msg
    QGroupBox * box3 = new QGroupBox("Log Message", mainLayoutWidget);
    layoutMain->addWidget(box3);
    layoutMain->setStretchFactor(box3, 1);

    QVBoxLayout * layout3 = new QVBoxLayout(box3);

    logInfo = new QPlainTextEdit(this);
    logInfo->isReadOnly();
    QFont font; 
    font.setFamily("Courier New");
    logInfo->setFont(font);

    layout3->addWidget(logInfo);

  }
  
  LogMsg("<font style=\"color: blue;\"><b>Welcome to SOLARIS DAQ.</b></font>");

  if( OpenProgramSettings() )  OpenExpSettings();

}

MainWindow::~MainWindow(){

  //---- may be no need to delete as thay are child of this
  //delete bnProgramSettings;
  //delete bnOpenDigitizers;
  //delete bnCloseDigitizers;
  //delete bnDigiSettings;
  //delete bnNewExp;
  //delete logInfo;

  StopScope();

  updateTraceThread->Stop();
  updateTraceThread->quit();
  updateTraceThread->wait();
  delete updateTraceThread;

  for( int i = 0; i < 6; i++) delete dataTrace[i];
  //delete [] dataTrace; /// dataTrace must be deleted before plot
  delete plot;

  //---- need manually delete
  if( digiSetting != NULL ) delete digiSetting;

  CloseDigitizers();

}

//^################################################################ ACQ control 
void MainWindow::StartACQ(){

  LogMsg("Start Run....");
  for( int i =0 ; i < nDigi; i ++){
    if( digi[i]->IsDummy () ) continue;
    digi[i]->Reset();
    digi[i]->ProgramPHA(false);
    digi[i]->SetPHADataFormat(1);// only save 1 trace

    digi[i]->WriteValue("/ch/0..63/par/WaveAnalogProbe0", "ADCInput");

    //TODO =========================== save file 
    remove("haha_000.sol"); // remove file
    digi[i]->OpenOutFile("haha");// haha_000.sol
    digi[i]->StartACQ();

    //TODO ========================== Sync start.
    readDataThread[i]->SetScopeRun(false);
    readDataThread[i]->start();
  }

  bnStartACQ->setEnabled(false);
  bnStopACQ->setEnabled(true);
  bnOpenScope->setEnabled(false);

  //updateTraceThread->start();

  LogMsg("end of " + QString::fromStdString(__func__));
}

void MainWindow::StopACQ(){

  for( int i = 0; i < nDigi; i++){
    if( digi[i]->IsDummy () ) continue;
    digi[i]->StopACQ();

    readDataThread[i]->quit();
    readDataThread[i]->wait();
    digi[i]->CloseOutFile();

  }

  LogMsg("Stop Run");
  bnStartACQ->setEnabled(true);
  bnStopACQ->setEnabled(false);
  bnOpenScope->setEnabled(true);

  runID ++;
  leRunID->setText(QString::number(runID));

}

//^###################################################################### open and close digitizer
void MainWindow::OpenDigitizers(){

  LogMsg("<font style=\"color:blue;\">Opening " + QString::number(nDigi) + " Digitizers..... </font>");

  digi = new Digitizer2Gen*[nDigi];
  readDataThread = new ReadDataThread*[nDigi];

  for( int i = 0; i < nDigi; i++){

    LogMsg("IP : " + IPList[i] + " | " + QString::number(i+1) + "/" + QString::number(nDigi));

    digi[i] = new Digitizer2Gen();
    digi[i]->OpenDigitizer(("dig2://" + IPList[i]).toStdString().c_str());

    if(digi[i]->IsConnected()){

      digiSerialNum.push_back(digi[i]->GetSerialNumber());

      LogMsg("Opened digitizer : <font style=\"color:red;\">" + QString::number(digi[i]->GetSerialNumber()) + "</font>");
      bnStartACQ->setEnabled(true);
      bnStopACQ->setEnabled(false);
      bnOpenScope->setEnabled(true);

      readDataThread[i] = new ReadDataThread(digi[i], this);
      connect(readDataThread[i], &ReadDataThread::sendMsg, this, &MainWindow::LogMsg);

    }else{
      LogMsg("Cannot open digitizer. Use a dummy with serial number " + QString::number(i));
      digi[i]->SetDummy();
      digiSerialNum.push_back(i);

      readDataThread[i] = NULL;
    }
  }

  bnDigiSettings->setEnabled(true);
  bnCloseDigitizers->setEnabled(true);
  bnOpenDigitizers->setEnabled(false);
  bnOpenDigitizers->setStyleSheet("");

  /// set scope digitizer
  allowChange = false;
  cbScopeDigi->clear(); ///this will also trigger QComboBox::currentIndexChanged
  cbScopeCh->clear();
  for( int i = 0 ; i < nDigi; i++) {
    cbScopeDigi->addItem("Digi-" + QString::number(digi[i]->GetSerialNumber()), i);
  }
  allowChange = true;

}

void MainWindow::CloseDigitizers(){

  if( digi == NULL) return;
  
  for( int i = 0; i < nDigi; i++){
    digi[i]->CloseDigitizer();

    delete digi[i];

    LogMsg("Closed Digitizer : " + QString::number(digiSerialNum[0]));
    
    digiSerialNum.clear();

    bnOpenDigitizers->setEnabled(true);
    bnCloseDigitizers->setEnabled(false);
    bnDigiSettings->setEnabled(false);
    bnStartACQ->setEnabled(false);
    bnStopACQ->setEnabled(false);

    if( digiSetting != NULL )  digiSetting->close(); 

    if( readDataThread[i] != NULL ){
      readDataThread[i]->quit();
      readDataThread[i]->wait();
      delete readDataThread[i];
    }
  }
  delete [] digi;
  delete [] readDataThread;
  digi = NULL;
  readDataThread = NULL;
  nDigi = 0;

  bnOpenDigitizers->setFocus();
}

//^###################################################################### Open Scope

//TODO ==== seperate Scope into a Scrop Panel class.

void MainWindow::OpenScope(){

  if( digi ) {
    //*---- get digi setting
    int iDigi = cbScopeDigi->currentIndex();
    if( digi[iDigi]->IsDummy() ) return;

    //digi[iDigi]->Reset();
    //digi[iDigi]->ProgramPHA(false);

    StartScope(iDigi);

    bnStartACQ->setEnabled(false);
    bnStopACQ->setEnabled(false);

  }

  scope->show();

}

void MainWindow::ReadScopeSettings(int iDigi, int ch){
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
  allowChange = true;
}

void MainWindow::StartScope(int iDigi){

  if( !digi ) return; 
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

void MainWindow::StopScope(){

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
    bnStartACQ->setEnabled(true);
    bnStopACQ->setEnabled(true);
  }

  ScopeControlOnOff(true);
  allowChange = true;
}

void MainWindow::UpdateScope(){

  if( scope->isVisible() == false) return;

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

void MainWindow::ProbeChange(QComboBox * cb[], const int size ){

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

void MainWindow::ScopeControlOnOff(bool on){
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
}

void MainWindow::ScopeReadSpinBoxValue(int iDigi, int ch, QSpinBox *sb, std::string digPara){
  std::string ans = digi[iDigi]->ReadChValue(std::to_string(ch), digPara);
  sb->setValue(atoi(ans.c_str()));
}

void MainWindow::ScopeReadComboBoxValue(int iDigi, int ch, QComboBox *cb, std::string digPara){
  std::string ans = digi[iDigi]->ReadChValue(std::to_string(ch), digPara);
  int index = cb->findData(QString::fromStdString(ans));
  if( index >= 0 && index < cb->count()) {
    cb->setCurrentIndex(index);
  }else{
    qDebug() << QString::fromStdString(ans);
  }
}

void MainWindow::ScopeMakeSpinBox(QSpinBox *sb, QString str, QGridLayout *layout, int row, int col, int min, int max, int step, std::string digPara){
  QLabel * lb = new QLabel(str, scope);
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

void MainWindow::ScopeMakeComoBox(QComboBox *cb, QString str, QGridLayout *layout, int row, int col, std::string digPara){
  QLabel * lb = new QLabel(str, scope);
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

void MainWindow::SetUpPlot(){ //@--- this function run at start up
  scope = new QMainWindow(this);
  scope->setWindowTitle("Scope");
  scope->setGeometry(0, 0, 1000, 800);  
  scope->setWindowFlags( scope->windowFlags() & ~Qt::WindowCloseButtonHint );

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
  xaxis->setTickCount(10);
  xaxis->setLabelFormat("%.0f");
  xaxis->setTitleText("Time [ns]");

  updateTraceThread = new UpdateTraceThread();
  connect(updateTraceThread, &UpdateTraceThread::updateTrace, this, &MainWindow::UpdateScope);

  //*================ add Widgets
  int rowID = -1;
  QWidget * layoutWidget = new QWidget(scope);
  scope->setCentralWidget(layoutWidget);
  QGridLayout * layout = new QGridLayout(layoutWidget);
  layoutWidget->setLayout(layout);

  //------------ Digitizer + channel selection
  rowID ++;
  cbScopeDigi = new QComboBox(scope);
  cbScopeCh = new QComboBox(scope);
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

  bnScopeReset = new QPushButton("ReProgram Digitizer", scope);
  layout->addWidget(bnScopeReset, rowID, 2);
  connect(bnScopeReset, &QPushButton::clicked, this, [=](){
    if( !allowChange ) return;
    int iDigi = cbScopeDigi->currentIndex();
    digi[iDigi]->Reset();
    digi[iDigi]->ProgramPHA(false);
  });

  bnScopeReadSettings = new QPushButton("Read Ch. Settings", scope);
  layout->addWidget(bnScopeReadSettings, rowID, 3);
  connect(bnScopeReadSettings, &QPushButton::clicked, this, [=](){
    if( !allowChange ) return;
    ReadScopeSettings(cbScopeDigi->currentIndex(), cbScopeCh->currentIndex());
  });

  //TODO----- add copy settings and paste settings
  QCheckBox * chkSetAllChannel = new QCheckBox("apply to all channels", scope);
  layout->addWidget(chkSetAllChannel, rowID, 4);

  //------------ Probe selection
  rowID ++;
  //TODO --- add None
  cbAnaProbe[0] = new QComboBox(scope);
  cbAnaProbe[0]->addItem("ADC Input",        "ADCInput");
  cbAnaProbe[0]->addItem("Time Filter",      "TimeFilter");
  cbAnaProbe[0]->addItem("Trapazoid",        "EnergyFilter");
  cbAnaProbe[0]->addItem("Trap. Baseline",   "EnergyFilterBaseline");
  cbAnaProbe[0]->addItem("Trap. - Baseline", "EnergyFilterMinusBaseline");

  cbAnaProbe[1] = new QComboBox(scope);
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
  cbDigProbe[0] = new QComboBox(scope);
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

  cbDigProbe[1] = new QComboBox(scope);
  cbDigProbe[2] = new QComboBox(scope);
  cbDigProbe[3] = new QComboBox(scope);
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
    QGroupBox * box = new QGroupBox("Channel Settings (need ACQ stop)", scope);
    layout->addWidget(box, rowID, 0, 1, 6);

    QGridLayout * bLayout = new QGridLayout(box);
    bLayout->setSpacing(0);

    sbRL = new QSpinBox(scope);
    ScopeMakeSpinBox(sbRL, "Record Lenght [ns] ", bLayout, 0, 0, 32, 648000, DIGIPARA::TraceStep, DIGIPARA::CH::RecordLength);

    sbThreshold = new QSpinBox(scope);
    ScopeMakeSpinBox(sbThreshold, "Threshold [LSB] ", bLayout, 0, 2, 0, 8191, -20, DIGIPARA::CH::TriggerThreshold);

    cbPolarity = new QComboBox(scope);
    cbPolarity->addItem("Pos. +", "Positive");
    cbPolarity->addItem("Neg. -", "Negative");
    ScopeMakeComoBox(cbPolarity, "Polarity ", bLayout, 0, 4, DIGIPARA::CH::Polarity);

    cbWaveRes = new QComboBox(scope);
    cbWaveRes->addItem(" 8 ns", "RES8");
    cbWaveRes->addItem("16 ns", "RES16");
    cbWaveRes->addItem("32 ns", "RES32");
    cbWaveRes->addItem("64 ns", "RES64");
    ScopeMakeComoBox(cbWaveRes, "Wave Re. ", bLayout, 0, 6, DIGIPARA::CH::WaveResolution);

    //------------------ next row
    sbPT = new QSpinBox(scope);
    ScopeMakeSpinBox(sbPT, "Pre Trigger [ns] ", bLayout, 1, 0, 32, 32000, DIGIPARA::TraceStep, DIGIPARA::CH::PreTrigger);

    sbDCOffset = new QSpinBox(scope);
    ScopeMakeSpinBox(sbDCOffset, "DC offset [%] ", bLayout, 1, 2, 0, 100, -10, DIGIPARA::CH::DC_Offset);

    sbTimeRiseTime = new QSpinBox(scope);
    ScopeMakeSpinBox(sbTimeRiseTime, "Trigger Rise Time [ns] ", bLayout, 1, 4, 32, 2000, DIGIPARA::TraceStep, DIGIPARA::CH::TimeFilterRiseTime);

    sbTimeGuard = new QSpinBox(scope);
    ScopeMakeSpinBox(sbTimeGuard, "Trigger Guard [ns] ", bLayout, 1, 6, 0, 8000, DIGIPARA::TraceStep, DIGIPARA::CH::TimeFilterRetriggerGuard);

    //----------------- next row
    sbTrapRiseTime = new QSpinBox(scope);
    ScopeMakeSpinBox(sbTrapRiseTime, "Trap. Rise Time [ns] ", bLayout, 2, 0, 32, 13000, DIGIPARA::TraceStep, DIGIPARA::CH::EnergyFilterRiseTime);

    sbTrapFlatTop = new QSpinBox(scope);
    ScopeMakeSpinBox(sbTrapFlatTop, "Trap. Flat Top [ns] ", bLayout, 2, 2, 32, 3000, DIGIPARA::TraceStep, DIGIPARA::CH::EnergyFilterFlatTop);

    sbTrapPoleZero = new QSpinBox(scope);
    ScopeMakeSpinBox(sbTrapPoleZero, "Trap. Pole Zero [ns] ", bLayout, 2, 4, 32, 524000, DIGIPARA::TraceStep, DIGIPARA::CH::EnergyFilterPoleZero);

    sbEnergyFineGain = new QSpinBox(scope);
    ScopeMakeSpinBox(sbEnergyFineGain, "Energy Fine Gain ", bLayout, 2, 6, 1, 10, -1, DIGIPARA::CH::EnergyFilterFineGain);

    //----------------- next row
    sbTrapPeaking = new QSpinBox(scope);
    ScopeMakeSpinBox(sbTrapPeaking, "Trap. Peaking [%] ", bLayout, 3, 0, 1, 100, -10, DIGIPARA::CH::EnergyFilterPeakingPosition);

    cbTrapPeakAvg = new QComboBox(scope);
    cbTrapPeakAvg->addItem(" 1 sample", "OneShot");
    cbTrapPeakAvg->addItem(" 4 sample", "LowAVG");
    cbTrapPeakAvg->addItem("16 sample", "MediumAVG");
    cbTrapPeakAvg->addItem("64 sample", "HighAVG");
    ScopeMakeComoBox(cbTrapPeakAvg, "Trap. Peaking ", bLayout, 3, 2, DIGIPARA::CH::EnergyFilterPeakingAvg);
  }

  //------------ plot view
  rowID ++;
  QChartView * plotView = new QChartView(plot);
  plotView->setRenderHints(QPainter::Antialiasing);
  layout->addWidget(plotView, rowID, 0, 1, 6);

  //TODO zoom and pan, see Zoom Line example

  //------------ close button
  rowID ++;
  bnScopeStart = new QPushButton("Start", scope);
  layout->addWidget(bnScopeStart, rowID, 0);
  bnScopeStart->setEnabled(false);
  connect(bnScopeStart, &QPushButton::clicked, this, [=](){this->StartScope(cbScopeDigi->currentIndex());});

  bnScopeStop = new QPushButton("Stop", scope);
  layout->addWidget(bnScopeStop, rowID, 1);
  connect(bnScopeStop, &QPushButton::clicked, this, &MainWindow::StopScope);

  QPushButton * bnClose = new QPushButton("Close", scope);
  layout->addWidget(bnClose, rowID, 5);
  connect(bnClose, &QPushButton::clicked, this, &MainWindow::StopScope);
  connect(bnClose, &QPushButton::clicked, scope, &QMainWindow::close);

}


//^###################################################################### Open digitizer setting panel
void MainWindow::OpenDigitizersSettings(){
  LogMsg("Open digitizers Settings Panel");

  if( digiSetting == NULL){
    digiSetting = new DigiSettings(digi, nDigi);
    connect(digiSetting, &DigiSettings::sendLogMsg, this, &MainWindow::LogMsg);
    digiSetting->show();
  }else{
    digiSetting->show();
  }
}

//^###################################################################### Program Settings
void MainWindow::ProgramSettings(){

  LogMsg("Open <b>Program Settings</b>.");

  QDialog dialog(this);
  dialog.setWindowTitle("Program Settings");
  dialog.setGeometry(0, 0, 700, 530);
  dialog.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint);

  QGridLayout * layout = new QGridLayout(&dialog);
  layout->setVerticalSpacing(5);

  unsigned int rowID = 0;

  //-------- Instruction
  QPlainTextEdit * helpInfo = new QPlainTextEdit(&dialog);
  helpInfo->setReadOnly(true);
  helpInfo->setStyleSheet("background-color: #F3F3F3;");
  helpInfo->setLineWrapMode(QPlainTextEdit::LineWrapMode::WidgetWidth);
  helpInfo->appendHtml("These setting will be saved at the <font style=\"color : blue;\"> \
                            Settings Save Path </font> as <b>programSettings.txt</b>. \
                        If no such file exist, the program will create it.");
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Analysis Path  </font> is the path of \
                           the folder of the analysis code. e.g. /home/<user>/analysis/");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Data Path  </font> is the path of the \
                             <b>parents folder</b> of Raw data will store. e.g. /mnt/data0/, \
                          experiment data will be saved under this folder. e.g. /mnt/data1/exp1");
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("These 2 paths will be used when <font style=\"color : blue;\">  New/Change/Reload Exp </font>");
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Digitizers IP List </font> is the list of IP \
                           digi of the digitizers IP. Break by \",\", continue by \"-\". e.g. 192.168.0.100,102  for 2 digitizers, or 192.168.0.100-102 for 3 digitizers.");
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Database IP </font> or <font style=\"color : blue;\">  Elog IP </font> can be empty. In that case, no database and elog will be used.");


  layout->addWidget(helpInfo, rowID, 0, 1, 4);

  //-------- analysis Path
  rowID ++;
  QLabel *lbSaveSettingPath = new QLabel("Settings Save Path", &dialog);
  lbSaveSettingPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbSaveSettingPath, rowID, 0);
  lSaveSettingPath = new QLineEdit(settingFilePath, &dialog); layout->addWidget(lSaveSettingPath, rowID, 1, 1, 2);

  QPushButton * bnSaveSettingPath = new QPushButton("browser", &dialog); layout->addWidget(bnSaveSettingPath, rowID, 3);
  connect(bnSaveSettingPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(0);});

  //-------- analysis Path
  rowID ++;
  QLabel *lbAnalysisPath = new QLabel("Analysis Path", &dialog);
  lbAnalysisPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbAnalysisPath, rowID, 0);
  lAnalysisPath = new QLineEdit(analysisPath, &dialog); layout->addWidget(lAnalysisPath, rowID, 1, 1, 2);

  QPushButton * bnAnalysisPath = new QPushButton("browser", &dialog); layout->addWidget(bnAnalysisPath, rowID, 3);
  connect(bnAnalysisPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(1);});

  //-------- data Path
  rowID ++;
  QLabel *lbDataPath = new QLabel("Data Path", &dialog); 
  lbDataPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDataPath, rowID, 0);
  lDataPath = new QLineEdit(dataPath, &dialog); layout->addWidget(lDataPath, rowID, 1, 1, 2);

  QPushButton * bnDataPath = new QPushButton("browser", &dialog); layout->addWidget(bnDataPath, rowID, 3);
  connect(bnDataPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(2);});

  //-------- IP Domain
  rowID ++;
  QLabel *lbIPDomain = new QLabel("Digitizers IP List", &dialog); 
  lbIPDomain->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbIPDomain, rowID, 0);
  lIPDomain = new QLineEdit(IPListStr, &dialog); layout->addWidget(lIPDomain, rowID, 1, 1, 2);
  //-------- DataBase IP
  rowID ++;
  QLabel *lbDatbaseIP = new QLabel("Database IP", &dialog); 
  lbDatbaseIP->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDatbaseIP, rowID, 0);
  lDatbaseIP = new QLineEdit(DatabaseIP, &dialog); layout->addWidget(lDatbaseIP, rowID, 1, 1, 2);
  //-------- DataBase name
  rowID ++;
  QLabel *lbDatbaseName = new QLabel("Database Name", &dialog);
  lbDatbaseName->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDatbaseName, rowID, 0);
  lDatbaseName = new QLineEdit(DatabaseName, &dialog); layout->addWidget(lDatbaseName, rowID, 1, 1, 2);
  //-------- Elog IP
  rowID ++;
  QLabel *lbElogIP = new QLabel("Elog IP", &dialog);
  lbElogIP->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbElogIP, rowID, 0);
  lElogIP = new QLineEdit(ElogIP, &dialog); layout->addWidget(lElogIP, rowID, 1, 1, 2);

  rowID ++;
  QPushButton *button1 = new QPushButton("OK and Save", &dialog);
  layout->addWidget(button1, rowID, 1);
  QObject::connect(button1, &QPushButton::clicked, this, &MainWindow::SaveProgramSettings);
  QObject::connect(button1, &QPushButton::clicked, &dialog, &QDialog::accept);
  
  QPushButton *button2 = new QPushButton("Cancel", &dialog);
  layout->addWidget(button2, rowID, 2);
  QObject::connect(button2, &QPushButton::clicked, this, [=](){this->LogMsg("Cancel <b>Program Settings</b>");});
  QObject::connect(button2, &QPushButton::clicked, &dialog, &QDialog::reject);


  layout->setColumnStretch(0, 2);
  layout->setColumnStretch(1, 2);
  layout->setColumnStretch(2, 2);
  layout->setColumnStretch(3, 1);

  dialog.exec();
}

void MainWindow::OpenDirectory(int id){ 
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::Directory);
  fileDialog.exec();

  //qDebug() << fileDialog.selectedFiles();
  
  switch (id){
    case 0 : lSaveSettingPath->setText(fileDialog.selectedFiles().at(0)); break;
    case 1 : lAnalysisPath->setText(fileDialog.selectedFiles().at(0)); break;
    case 2 : lDataPath->setText(fileDialog.selectedFiles().at(0)); break;
  }
}

bool MainWindow::OpenProgramSettings(){

  QString settingFile = QDir::current().absolutePath() + "/programSettings.txt";

  LogMsg("Loading <b>" + settingFile + "</b> for Program Settings.");

  QFile file(settingFile);

  bool ret = false;

  if( !file.open(QIODevice::Text | QIODevice::ReadOnly) ) {
    LogMsg("<b>" + settingFile + "</b> not found.");
    LogMsg("Please Open the <font style=\"color : red;\">Program Settings </font>");
  }else{
  
    QTextStream in(&file);
    QString line = in.readLine();

    int count = 0;
    while( !line.isNull()){
      if( line.left(6) == "//----") break;

      switch (count){
        case 0 : settingFilePath = line; break;
        case 1 : analysisPath    = line; break;
        case 2 : dataPath        = line; break;
        case 3 : IPListStr          = line; break;
        case 4 : DatabaseIP      = line; break;
        case 5 : DatabaseName    = line; break;
        case 6 : ElogIP          = line; break;
      }

      count ++;
      line = in.readLine();
    }

    if( count == 7 ) {
      logMsgHTMLMode = false;
      LogMsg("Setting File Path : " + settingFilePath);
      LogMsg("    Analysis Path : " + analysisPath);
      LogMsg("        Data Path : " + dataPath);
      LogMsg("    Digi. IP List : " + IPListStr);
      LogMsg("      Database IP : " + DatabaseIP);
      LogMsg("    Database Name : " + DatabaseName);
      LogMsg("           ElogIP : " + ElogIP);
      logMsgHTMLMode = true;
      
      ret = true;
    }else{
      LogMsg("Settings are not complete.");
      LogMsg("Please Open the <font style=\"color : red;\">Program Settings </font>");
    }
  }


  if( ret ){

    //------- decode IPListStr
    nDigi = 0;
    QStringList parts = IPListStr.split(".");
    QString IPDomain = parts[0] + "." + parts[1] + "." + parts[2] + ".";
    parts = parts[3].split(",");
    for(int i = 0; i < parts.size(); i++){
      if( parts[i].indexOf("-") != -1){
          QStringList haha = parts[i].split("-");
          for( int j = haha[0].toInt();  j <= haha.last().toInt(); j++){
            IPList << IPDomain + QString::number(j);
          }
      }else{
        IPList << IPDomain + parts[i];
      }
    }

    nDigi = IPList.size();

    return true;

  }else{

    bnProgramSettings->setStyleSheet("color: red;");
    bnNewExp->setEnabled(false);
    bnOpenDigitizers->setEnabled(false);
    return false;
  }
}

void MainWindow::SaveProgramSettings(){

  IPListStr = lIPDomain->text();
  DatabaseIP = lDatbaseIP->text();
  DatabaseName = lDatbaseName->text();
  ElogIP = lElogIP->text();

  settingFilePath = lSaveSettingPath->text();
  analysisPath = lAnalysisPath->text();
  dataPath = lDataPath->text();

  QFile file(settingFilePath + "/programSettings.txt");
  
  file.open(QIODevice::Text | QIODevice::WriteOnly);

  file.write((settingFilePath+"\n").toStdString().c_str());
  file.write((analysisPath+"\n").toStdString().c_str());
  file.write((dataPath+"\n").toStdString().c_str());
  file.write((IPListStr+"\n").toStdString().c_str());
  file.write((DatabaseIP+"\n").toStdString().c_str());
  file.write((DatabaseName+"\n").toStdString().c_str());
  file.write((ElogIP+"\n").toStdString().c_str());
  file.write("//------------end of file.");
  
  file.close();
  LogMsg("Saved program settings to <b>"+settingFilePath + "/programSettings.txt<b>.");

  bnProgramSettings->setStyleSheet("");
  bnNewExp->setEnabled(true);
  bnOpenDigitizers->setEnabled(true);

  OpenExpSettings();

}

//^###################################################################### Setup new exp

void MainWindow::SetupNewExp(){

  LogMsg("Open <b>New/Change/Reload Exp</b>.");

  QDialog dialog(this);
  dialog.setWindowTitle("Setup / change Experiment");
  dialog.setGeometry(0, 0, 500, 550);
  dialog.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);

  QGridLayout * layout = new QGridLayout(&dialog);
  layout->setVerticalSpacing(5);

  unsigned short rowID = 0;

  //------- instruction
  QPlainTextEdit * instr = new QPlainTextEdit(&dialog);
  layout->addWidget(instr, rowID, 0, 1, 4);
  instr->setReadOnly(true);
  instr->setStyleSheet("background-color: #F3F3F3;");
  instr->appendHtml("Setup new experiment will do following things:");
  instr->appendHtml("<b>0,</b> Check the git repository in <font style=\"color:blue;\">Analysis Path</font>");
  instr->appendHtml("<b>1,</b> Create folder in <font style=\"color:blue;\">Data Path</font>");
  instr->appendHtml("<b>2,</b> Create Symbolic links in <font style=\"color:blue;\">Analysis Path</font>");
  instr->appendHtml("<b>3,</b> Create <b>expName.sh</b> in <font style=\"color:blue;\">Analysis Path</font> ");
  instr->appendHtml("<p></p>");
  instr->appendHtml("If <font style=\"color:blue;\">Use Git</font> is <b>checked</b>, \
                    the repository <b>MUST</b> be clean. \
                    It will then create a new branch with the <font style=\"color:blue;\">New Exp Name </font> \
                    or change to pre-exist branch.");
  instr->appendHtml("<p></p>");
  instr->appendHtml("If there is no git repository in <font style=\"color:blue;\">Analysis Path</font>, \
                    it will create one with a branch name of <font style=\"color:blue;\">New Exp Name </font>.");
  instr->appendHtml("<p></p>");
  instr->appendHtml("<b>expName.sh</> stores the exp name, runID, and elogID."); 

  //------- Analysis Path
  rowID ++;
  QLabel * l1 = new QLabel("Analysis Path ", &dialog);
  l1->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(l1, rowID, 0);

  QLineEdit * le1 = new QLineEdit(analysisPath, &dialog);
  le1->setReadOnly(true);
  layout->addWidget(le1, rowID, 1, 1, 3);

  //------- Data Path
  rowID ++;
  QLabel * l2 = new QLabel("Data Path ", &dialog);
  l2->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(l2, rowID, 0);

  QLineEdit * le2 = new QLineEdit(dataPath, &dialog);
  le2->setReadOnly(true);
  layout->addWidget(le2, rowID, 1, 1, 3);

  //------- get harddisk space;
  rowID ++;
  //?QStorageInfo storage("/path/to/drive");
  QStorageInfo storage = QStorageInfo::root();
  qint64 availableSpace = storage.bytesAvailable();

  QLabel * lbDiskSpace = new QLabel("Disk space avalible ", &dialog);
  lbDiskSpace->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(lbDiskSpace, rowID, 0);

  QLineEdit * leDiskSpace = new QLineEdit(QString::number(availableSpace/1024./1024./1024.) + " [GB]", &dialog);
  leDiskSpace->setReadOnly(true);
  layout->addWidget(leDiskSpace, rowID, 1, 1, 3);


  //*---------- get git branch
  isGitExist = false;
  QProcess git;
  git.setWorkingDirectory(analysisPath);
  //?git.setWorkingDirectory("/home/ryan/digios");
  git.start("git", QStringList() << "branch" << "-a");
  git.waitForFinished();

  QByteArray output = git.readAllStandardOutput();
  QStringList branches = (QString::fromLocal8Bit(output)).split("\n");
  branches.removeAll("");

  //qDebug() << branches;

  if( branches.size() == 0) {
    isGitExist = false;
  }else{
    isGitExist = true;
  }

  QString presentBranch;
  unsigned short bID = 0; // id of the present branch
  for( unsigned short i = 0; i < branches.size(); i++){
    if( branches[i].indexOf("*") != -1 ){
      presentBranch = branches[i].remove("*").remove(" ");
      bID = i;
      break;
    }
  }

  //*----------- check git branch is clean for git exist
  bool isCleanGit = false;
  if( isGitExist ){
    git.start("git", QStringList() << "status" << "--porcelain" << "--untracked-files=no");
    git.waitForFinished();
    output = git.readAllStandardOutput();
    if( (QString::fromLocal8Bit(output)).isEmpty() ) isCleanGit = true;
  }

  //------- present git branch
  rowID ++;
  QLabel * l3 = new QLabel("Present Git Branches ", &dialog);
  l3->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(l3, rowID, 0);

  QLineEdit * le3 = new QLineEdit(presentBranch, &dialog);

  if( isGitExist == false ) {
    le3->setText("No git repository!!!");
    le3->setStyleSheet("color: red;");
  }
  le3->setReadOnly(true);
  layout->addWidget(le3, rowID, 1, 1, 3);

  //------- add a separator
  rowID ++;
  QFrame * line = new QFrame;
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  layout->addWidget(line, rowID, 0, 1, 4);

  //------- use git checkbox
  rowID ++;
  QCheckBox * cbUseGit = new QCheckBox("Use Git", &dialog);
  cbUseGit->setChecked(true);
  useGit = true;
  layout->addWidget(cbUseGit, rowID, 1);
  connect(cbUseGit, &QCheckBox::clicked, this, [=](){this->useGit = cbUseGit->isChecked();});

  //------- display git cleanness
  //?---- don't know why isGitExist && !isCleanGit does not work
  if( isGitExist ){
    if ( !isCleanGit){  
      QLabel * lCleanGit = new QLabel("Git not CLEAN!!! Nothing can be done.", &dialog);
      lCleanGit->setStyleSheet("color: red;");
      layout->addWidget(lCleanGit, rowID, 2, 1, 2);
    }
  }

  //------- show list of exisiting git repository
  rowID ++;
  QLabel * l4 = new QLabel("Existing Git Branches ", &dialog);
  l4->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(l4, rowID, 0);

  QComboBox * cb = new QComboBox(&dialog);
  layout->addWidget(cb, rowID, 1, 1, 2);

  QPushButton *bnChangeBranch = new QPushButton("Change", &dialog);
  layout->addWidget(bnChangeBranch, rowID, 3);

  connect(bnChangeBranch, &QPushButton::clicked, this, [=](){ this->ChangeExperiment(cb->currentText());  });
  connect(bnChangeBranch, &QPushButton::clicked, &dialog, &QDialog::accept);

  if( isGitExist == false ){
    cb->setEnabled(false);
    bnChangeBranch->setEnabled(false);
  }else{
    for( int i = 0; i < branches.size(); i++){
      if( i == bID ) continue;
      cb->addItem(branches[i].remove(" "));
    }
    if ( branches.size() == 1) {
      cb->setEnabled(false);
      cb->addItem("no other branch");
      bnChangeBranch->setEnabled(false);
    }
  }

  connect(cbUseGit, &QCheckBox::clicked, this, [=](){
                        if( branches.size() > 1 ) cb->setEnabled(cbUseGit->isChecked());
                      });
  connect(cbUseGit, &QCheckBox::clicked, this, [=](){
                        if(branches.size() > 1 ) bnChangeBranch->setEnabled(cbUseGit->isChecked());
                      });
  
  //------- type existing or new experiment
  rowID ++;
  QLabel * lNewExp = new QLabel("New Exp Name ", &dialog);
  lNewExp->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(lNewExp, rowID, 0);

  QLineEdit * newExp = new QLineEdit("type and Enter", &dialog);
  layout->addWidget(newExp, rowID, 1, 1, 2);

  QPushButton *button1 = new QPushButton("Create", &dialog);
  layout->addWidget(button1, rowID, 3);
  button1->setEnabled(false);
  
  connect(newExp, &QLineEdit::returnPressed, this, [=](){ if( newExp->text() != "") button1->setEnabled(true);});
  connect(button1, &QPushButton::clicked, this, [=](){ this->CreateNewExperiment(newExp->text());});
  connect(button1, &QPushButton::clicked, &dialog, &QDialog::accept);

  //----- diable all possible actions
  //?---- don't know why isGitExist && !isCleanGit does not work
  if( isGitExist){
    if ( !isCleanGit ){
      cbUseGit->setEnabled(false);
      cb->setEnabled(false);
      bnChangeBranch->setEnabled(false);
      newExp->setEnabled(false);
      button1->setEnabled(false);
    }
  }
  //--------- cancel
  rowID ++;
  QPushButton *bnCancel = new QPushButton("Cancel", &dialog);
  layout->addWidget(bnCancel, rowID, 0, 1, 4);
  connect(bnCancel, &QPushButton::clicked, this, [=](){this->LogMsg("Cancel <b>New/Change/Reload Exp</b>");});
  connect(bnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);

  layout->setRowStretch(0, 1);
  for( int i = 1; i < rowID; i++) layout->setRowStretch(i, 2);

  OpenExpSettings();

  dialog.exec();

}
 
bool MainWindow::OpenExpSettings(){
  //this method set the analysis setting ann symbloic link to raw data
  //ONLY load file, not check the git

  if( analysisPath == "") return false;

  QString settingFile = analysisPath + "/expName.sh";

  LogMsg("Loading <b>" + settingFile + "</b> for Experiment.");

  QFile file(settingFile);
  if( !file.open(QIODevice::Text | QIODevice::ReadOnly) ) {
    LogMsg("<b>" + settingFile + "</b> not found.");
    LogMsg("Please Open the <font style=\"color : red;\">New/Change/Reload Exp</font>");

    bnNewExp->setStyleSheet("color: red;");
    bnOpenDigitizers->setEnabled(false);
    leExpName->setText("no expName found.");
    
    return false;
  }

  QTextStream in(&file);
  QString line = in.readLine();

  int count = 0;
  while( !line.isNull()){

    int index = line.indexOf("=");
    QString haha = line.mid(index+1).remove(" ").remove("\"");

    //qDebug() << haha;

    switch (count){
      case 0 : expName = haha; break;
      case 1 : rawDataFolder = haha; break;
      case 2 : runID = haha.toInt(); break;
      case 3 : elogID = haha.toInt(); break;
    }

    count ++;
    line = in.readLine();
  }

  leRawDataPath->setText(rawDataFolder);
  leExpName->setText(expName);
  leRunID->setText(QString::number(runID));

  bnOpenDigitizers->setStyleSheet("color:red;");

  return true;

}

void MainWindow::CreateNewExperiment(const QString newExpName){
  
  LogMsg("Creating new Exp. : <font style=\"color: red;\">" + newExpName + "</font>");

  //@----- git must be clean
  //----- creat new git branch
  if( useGit ){
    QProcess git;
    git.setWorkingDirectory(analysisPath);
    if( !isGitExist){
      git.start("git", QStringList() << "init" << "-b" << newExpName);
      git.waitForFinished();

      LogMsg("Initialzed a git repositiory with branch name <b>" + newExpName + "</b>");
    }else{
      git.start("git", QStringList() << "checkout" << "-b" << newExpName);
      git.waitForFinished();

      LogMsg("Creat name branch : <b>" + newExpName + "</b>");
    }
  }

  CreateRawDataFolderAndLink(newExpName);

  //----- create the expName.sh
  QFile file2(analysisPath + "/expName.sh");
  
  file2.open(QIODevice::Text | QIODevice::WriteOnly);
  file2.write(("expName = "+ newExpName + "\n").toStdString().c_str());
  file2.write(("rawDataPath = "+ rawDataFolder + "\n").toStdString().c_str());
  file2.write("runID = 0\n");
  file2.write("elogID = 0\n");
  file2.write("//------------end of file.");
  file2.close();
  LogMsg("Saved expName.sh to <b>"+ analysisPath + "/expName.sh<b>.");


  //----- create git branch
  if( useGit ){
    QProcess git;
    git.setWorkingDirectory(analysisPath);
    git.start("git", QStringList() << "add" << "-A");
    git.waitForFinished();
    
    git.start("git", QStringList() << "commit" << "-m" << "initial commit.");
    git.waitForFinished();

    LogMsg("Commit branch : <b>" + newExpName + "</b> as \"initial commit\"");
  }

  expName = newExpName;
  runID = 0;
  elogID = 0;

  leRawDataPath->setText(rawDataFolder);
  leExpName->setText(expName);
  leRunID->setText(QString::number(runID));

  bnNewExp->setStyleSheet("");
  bnOpenDigitizers->setEnabled(true);
  bnOpenDigitizers->setStyleSheet("color:red;");

}

void MainWindow::ChangeExperiment(const QString newExpName){

  //@---- git must exist.

  QProcess git;
  git.setWorkingDirectory(analysisPath);
  git.start("git", QStringList() << "checkout" << newExpName);
  git.waitForFinished();

  LogMsg("Swicted to branch : <b>" + newExpName + "</b>");

  CreateRawDataFolderAndLink(newExpName);

  OpenExpSettings();

}

void MainWindow::CreateRawDataFolderAndLink(const QString newExpName){

  //----- create data folder
  rawDataFolder = dataPath + "/" + newExpName;
  QDir dir;
  if( !dir.exists(rawDataFolder)){
    if( dir.mkdir(rawDataFolder)){
      LogMsg("<b>" + rawDataFolder + "</b> created." );
    }else{
      LogMsg("<b>" + rawDataFolder + "</b> cannot be created. Access right problem?" );
    }
  }else{
      LogMsg("<b>" + rawDataFolder + "</b> already exist." );
  }

  //----- create symbloic link
  QString linkName = analysisPath + "/data_raw";
  QFile file;

  if( file.exists(linkName)) {
    file.remove(linkName);
  }

  if (file.link(rawDataFolder, linkName)) {
      LogMsg("Symbolic link  <b>" + linkName +"</b> -> " + rawDataFolder + " created.");
  } else {
      LogMsg("Symbolic link  <b>" + linkName +"</b> -> " + rawDataFolder + " cannot be created.");
  }
}

//^###################################################################### log msg
void MainWindow::LogMsg(QString msg){

    QString outputStr = QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString("MM.dd hh:mm:ss"), msg);
    if( logMsgHTMLMode ){ 
      logInfo->appendHtml(outputStr);
    }else{
      logInfo->appendPlainText(outputStr);
    }
    QScrollBar *v = logInfo->verticalScrollBar();
    v->setValue(v->maximum());
    //qDebug() << msg;
    logInfo->repaint();
}
