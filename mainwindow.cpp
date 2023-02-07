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

  bnOpenScope->setEnabled(true);

}

MainWindow::~MainWindow(){

  //---- may be no need to delete as thay are child of this
  //delete bnProgramSettings;
  //delete bnOpenDigitizers;
  //delete bnCloseDigitizers;
  //delete bnDigiSettings;
  //delete bnNewExp;
  //delete logInfo;

  updateTraceThread->Stop();
  updateTraceThread->quit();
  updateTraceThread->wait();
  delete updateTraceThread;

  delete dataTrace; /// dataTrace must be deleted before plot
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
void MainWindow::OpenScope(){


  cbScopeDigi->clear(); ///thsi will also trigger QComboBox::currentIndexChanged
  cbScopeCh->clear();
  if( digi ) {


    for( int i = 0 ; i < nDigi; i++) {
      cbScopeDigi->addItem("Digi-" + QString::number(digi[i]->GetSerialNumber()), i);
    }

    //*---- set digitizer to take full trace; since in scope mode, no data saving, speed would be fast (How fast?)
    //* when the input rate is faster than trigger rate, Digitizer will stop data taking.

    int iDigi = cbScopeDigi->currentIndex();
    int ch = cbScopeCh->currentIndex();

    if( digi[iDigi]->IsDummy() ) return;

    digi[iDigi]->Reset();
    digi[iDigi]->ProgramPHA(false);
    
    digi[iDigi]->WriteValue("/ch/0..63/par/ChEnable", "false");
    digi[iDigi]->WriteValue(("/ch/" + std::to_string(ch) + "/par/ChEnable").c_str(), "true");
    digi[iDigi]->SetPHADataFormat(0);

    digi[iDigi]->StartACQ();

    readDataThread[iDigi]->SetScopeRun(true);
    readDataThread[iDigi]->start();
    
    updateTraceThread->start();
    bnStartACQ->setEnabled(false);
    bnStopACQ->setEnabled(false);
  }

  scope->show();

}
void MainWindow::StopScope(){

  updateTraceThread->Stop();
  updateTraceThread->quit();
  updateTraceThread->wait();

  if(digi){
    for(int i = 0; i < nDigi; i++){
      if( digi[i]->IsDummy() ) continue;
      digi[i]->StopACQ();

      readDataThread[i]->quit();
      readDataThread[i]->wait();
    }
    bnStartACQ->setEnabled(true);
    bnStopACQ->setEnabled(true);
  }

}

void MainWindow::UpdateScope(){

  if( scope->isVisible() == false) return;

  int iDigi = cbScopeDigi->currentIndex();
  //int ch = cbScopeCh->currentIndex();

  if( digi ){
    digiMTX.lock();
    unsigned int traceLength = digi[iDigi]->evt->traceLenght;
    unsigned int dataLength = dataTrace->count();

    if( traceLength < dataLength){
      for( unsigned int i = 0; i < traceLength; i++) {
        dataTrace->replace(i, i, digi[iDigi]->evt->analog_probes[0][i]);
      }
      dataTrace->removePoints(traceLength, dataLength-traceLength);
    }else{

      for( unsigned int i = 0 ; i < traceLength; i++){ 
        if( i < dataLength ) {
          dataTrace->replace(i, i, digi[iDigi]->evt->analog_probes[0][i]);
        }else{
          dataTrace->append(i, digi[iDigi]->evt->analog_probes[0][i]);
        }
        
      }

    }
    digiMTX.unlock();

    plot->axes(Qt::Vertical).first()->setRange(-1, 16000); /// this must be after createDefaultAxes();
    plot->axes(Qt::Horizontal).first()->setRange(0, traceLength);
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
  
}

void MainWindow::SetUpPlot(){ //@--- this function run at start up
  scope = new QMainWindow(this);
  scope->setWindowTitle("Scope");
  scope->setGeometry(0, 0, 1000, 800);  
  scope->setWindowFlags( scope->windowFlags() & ~Qt::WindowCloseButtonHint );

  plot = new QChart();
  dataTrace = new QLineSeries();
  dataTrace->setName("data");
  for(int i = 0; i < 100; i ++) dataTrace->append(i, QRandomGenerator::global()->bounded(10));
  plot->addSeries(dataTrace);
  plot->createDefaultAxes(); /// this must be after addSeries();
  plot->axes(Qt::Vertical).first()->setRange(-1, 11); /// this must be after createDefaultAxes();
  plot->axes(Qt::Horizontal).first()->setRange(-1, 101);

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
    int index = cbScopeDigi->currentIndex();
    if( index == -1 ) return;
    for( int i = 0; i < digi[index]->GetNChannels(); i++){
      cbScopeCh->addItem("ch-" + QString::number(i), i);
    }
  });

  //------------ Probe selection
  cbAnaProbe[0] = new QComboBox(scope);
  cbAnaProbe[0]->addItem("ADC Input",        "ADCInput");
  cbAnaProbe[0]->addItem("Time Filter",      "TimeFiler");
  cbAnaProbe[0]->addItem("Trapazoid",        "EnergyFilter");
  cbAnaProbe[0]->addItem("Trap. Baseline",   "EnergyFilterBaseline");
  cbAnaProbe[0]->addItem("Trap. - Baseline", "EnergyFilterMinusBaseline");

  cbAnaProbe[1] = new QComboBox(scope);
  for( int i = 0; i < cbAnaProbe[0]->count() ; i++) cbAnaProbe[1]->addItem(cbAnaProbe[0]->itemText(i), cbAnaProbe[0]->itemData(i));

  connect(cbAnaProbe[0], &QComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbAnaProbe, 2);});
  connect(cbAnaProbe[1], &QComboBox::currentIndexChanged, this, [=](){ this->ProbeChange(cbAnaProbe, 2);});

  //connect(cbAnaProbe[0], &QComboBox::currentIndexChanged, this, [=](){ 
  //  int iDigi = cbScopeDigi->currentIndex();
  //  int ch = cbScopeCh->currentIndex();
  //  char str[200] = ("/ch/" + std::to_string(ch) + "/par/WaveAnalogProbe0").c_str();
  //  digi[iDigi]->WriteValue(str, (cbAnaProbe[0]->currentData()).toString().toStdString());
  //});

  cbAnaProbe[0]->setCurrentIndex(1); ///trigger the AnaProbeChange
  cbAnaProbe[0]->setCurrentIndex(0);
  cbAnaProbe[1]->setCurrentIndex(4);

  cbDigProbe[0] = new QComboBox(scope);
  cbDigProbe[0]->addItem("Trigger",              "Trigger");
  cbDigProbe[0]->addItem("Time Filter Armed",    "TimeFilerArmed");
  cbDigProbe[0]->addItem("ReTrigger Guard",      "ReTriggerGaurd");
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

  layout->addWidget(cbAnaProbe[0], rowID, 2);
  layout->addWidget(cbAnaProbe[1], rowID, 3);

  rowID ++;
  layout->addWidget(cbDigProbe[0], rowID, 0);
  layout->addWidget(cbDigProbe[1], rowID, 1);
  layout->addWidget(cbDigProbe[2], rowID, 2);
  layout->addWidget(cbDigProbe[3], rowID, 3);

  //------------ plot view
  rowID ++;
  QChartView * plotView = new QChartView(plot);
  plotView->setRenderHints(QPainter::Antialiasing);
  layout->addWidget(plotView, rowID, 0, 1, 4);

  //------------ close button
  rowID ++;
  QPushButton * bnStop = new QPushButton("Stop", scope);
  layout->addWidget(bnStop, rowID, 2);
  connect(bnStop, &QPushButton::clicked, this, &MainWindow::StopScope);

  QPushButton * bnClose = new QPushButton("Close", scope);
  layout->addWidget(bnClose, rowID, 3);
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
