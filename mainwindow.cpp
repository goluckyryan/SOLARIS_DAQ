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
#include <QApplication>
#include <QDateTime>
#include <QProcess>
#include <QScreen>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <unistd.h>

//------ static memeber
Digitizer2Gen ** MainWindow::digi = NULL;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent){

  setWindowTitle("SOLARIS DAQ");
  setGeometry(500, 100, 1000, 500);
  QIcon icon("SOLARIS_favicon.png");
  setWindowIcon(icon);

  nDigi = 0;
  digiSetting = NULL;
  influx = NULL;
  readDataThread = NULL;
  scope = NULL;
  runTimer = new QTimer();
  connect(runTimer, &QTimer::timeout, this, &MainWindow::AutoRun);

  {
    scalar = new QMainWindow(this);
    scalar->setWindowTitle("Scalar");
    scalar->setGeometry(0, 0, 1000, 800);

    QScrollArea * scopeScroll = new QScrollArea(scalar);
    scalar->setCentralWidget(scopeScroll);
    scopeScroll->setWidgetResizable(true);
    scopeScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget * layoutWidget = new QWidget(scalar);
    scopeScroll->setWidget(layoutWidget);

    scalarLayout = new QGridLayout(layoutWidget);
    scalarLayout->setSpacing(0);

    leTrigger = NULL;
    leAccept = NULL;

    scalarThread = new ScalarThread();
    connect(scalarThread, &ScalarThread::updataScalar, this, &MainWindow::UpdateScalar);
  }

  QWidget * mainLayoutWidget = new QWidget(this);
  setCentralWidget(mainLayoutWidget);
  QVBoxLayout * layoutMain = new QVBoxLayout(mainLayoutWidget);
  mainLayoutWidget->setLayout(layoutMain);

  {//====================== General
    QGroupBox * box1 = new QGroupBox("General", mainLayoutWidget);
    layoutMain->addWidget(box1);

    QGridLayout * layout1 = new QGridLayout(box1);

    bnProgramSettings = new QPushButton("Program Settings", this);
    connect(bnProgramSettings, &QPushButton::clicked, this, &MainWindow::ProgramSettingsPanel);

    bnNewExp = new QPushButton("New/Change/Reload Exp", this);
    connect(bnNewExp, &QPushButton::clicked, this, &MainWindow::SetupNewExpPanel);

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

    //QPushButton * bnCustomCommand = new QPushButton("Command line", this);


    layout1->addWidget(bnProgramSettings, 0, 0);
    layout1->addWidget(bnNewExp, 0, 1);
    layout1->addWidget(lExpName, 0, 2);
    layout1->addWidget(leExpName, 0, 3);

    layout1->addWidget(bnOpenDigitizers, 1, 1);
    layout1->addWidget(bnCloseDigitizers, 1, 2, 1, 2);

    layout1->addWidget(bnOpenScope, 2, 0);
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

    bnOpenScalar = new QPushButton("Open Scalar", this);
    bnOpenScalar->setEnabled(false);
    connect(bnOpenScalar, &QPushButton::clicked, this, &MainWindow::OpenScaler);

    QLabel * lbRunID = new QLabel("Run ID : ", this); 
    lbRunID->setAlignment(Qt::AlignRight | Qt::AlignCenter);

    leRunID = new QLineEdit(this);
    leRunID->setAlignment(Qt::AlignHCenter);
    leRunID->setReadOnly(true);
    leRunID->setStyleSheet("background-color: #F3F3F3;");

    chkSaveRun = new QCheckBox("Save Run", this);
    chkSaveRun->setChecked(true);
    chkSaveRun->setEnabled(false);

    cbAutoRun = new QComboBox(this);
    cbAutoRun->addItem("Single infinte", -1);
    cbAutoRun->addItem("Single 30 mins", 30);
    cbAutoRun->addItem("Single 60 mins", 60);
    cbAutoRun->addItem("Single 2 hrs",  120);
    cbAutoRun->addItem("Single 3 hrs",  180);
    cbAutoRun->addItem("Single 5 hrs",  300);
    cbAutoRun->addItem("Every 30 mins", -30);
    cbAutoRun->addItem("Every 60 mins", -60);
    cbAutoRun->addItem("Every 2 hrs",  -120);
    cbAutoRun->addItem("Every 3 hrs",  -180);
    cbAutoRun->addItem("Every 5 hrs",  -300);
    cbAutoRun->setEnabled(false);

    bnStartACQ = new QPushButton("Start ACQ", this);
    bnStartACQ->setEnabled(false);
    connect(bnStartACQ, &QPushButton::clicked, this, &MainWindow::StartACQ);
    
    bnStopACQ = new QPushButton("Stop ACQ", this);
    bnStopACQ->setEnabled(false);
    connect(bnStopACQ, &QPushButton::clicked, this, &MainWindow::StopACQ);

    QLabel * lbRunComment = new QLabel("Run Comment : ", this);
    lbRunComment->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    leRunComment = new QLineEdit(this);
    leRunComment->setReadOnly(true);
    leRunComment->setStyleSheet("background-color: #F3F3F3;");
    
    layout2->addWidget(lbRawDataPath, 0, 0);
    layout2->addWidget(leRawDataPath, 0, 1, 1, 4);
    layout2->addWidget(bnOpenScalar, 0, 5);

    layout2->addWidget(lbRunID,    1, 0);
    layout2->addWidget(leRunID,    1, 1);
    layout2->addWidget(chkSaveRun, 1, 2);
    layout2->addWidget(cbAutoRun,  1, 3);
    layout2->addWidget(bnStartACQ, 1, 4);
    layout2->addWidget(bnStopACQ,  1, 5);

    layout2->addWidget(lbRunComment, 2, 0);
    layout2->addWidget(leRunComment,   2, 1, 1, 5);

    layout2->setColumnStretch(0, 2);
    layout2->setColumnStretch(1, 1);
    layout2->setColumnStretch(2, 1);
    layout2->setColumnStretch(3, 1);
    layout2->setColumnStretch(4, 3);
    layout2->setColumnStretch(5, 3);

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

  if( LoadProgramSettings() )  LoadExpSettings();

}

MainWindow::~MainWindow(){

  //---- may be no need to delete as thay are child of this
  //delete bnProgramSettings;
  //delete bnOpenDigitizers;
  //delete bnCloseDigitizers;
  //delete bnDigiSettings;
  //delete bnNewExp;
  //delete logInfo;
  printf("- %s\n", __func__);

  printf("-------- Delete readData Thread\n");
  if( digi ){
    for( int i = 0; i < nDigi ; i++){
      if( digi[i]->IsDummy()) continue;
      if( readDataThread[i]->isRunning()) StopACQ();
    }
  }
  CloseDigitizers();

  printf("-------- Delete scalar Thread\n");
  DeleteTriggerLineEdit();
  delete scalarThread;
  
  //---- need manually delete
  printf("-------- delete scope\n");
  if( scope != NULL ) delete scope;
  
  printf("-------- delete digiSetting\n");
  if( digiSetting != NULL ) delete digiSetting;

  printf("-------- delete influx\n");
  if( influx != NULL ) delete influx;

}

//^################################################################ ACQ control 
void MainWindow::StartACQ(){

  if( chkSaveRun->isChecked() ){
    runID ++;
    leRunID->setText(QString::number(runID));

    runIDStr = QString::number(runID).rightJustified(3, '0');
    LogMsg("=========================== Start <b><font style=\"color : red;\">Run-" + runIDStr + "</font></b>");

    //============ start comment
    //if( cbAutoRun->currentData().toInt() > 0  ){
      QDialog * dOpen = new QDialog(this);
      dOpen->setWindowTitle("Start Run Comment");
      dOpen->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
      dOpen->setMinimumWidth(600);
      connect(dOpen, &QDialog::finished, dOpen, &QDialog::deleteLater);

      QGridLayout * vlayout = new QGridLayout(dOpen);
      QLabel *label = new QLabel("Enter Run comment for <font style=\"color : red;\">Run-" +  runIDStr + "</font> : ", dOpen);
      QLineEdit *lineEdit = new QLineEdit(dOpen);
      QPushButton *button1 = new QPushButton("OK", dOpen);
      QPushButton *button2 = new QPushButton("Cancel", dOpen);

      vlayout->addWidget(label, 0, 0, 1, 2);
      vlayout->addWidget(lineEdit, 1, 0, 1, 2);
      vlayout->addWidget(button1, 2, 0);
      vlayout->addWidget(button2, 2, 1);

      connect(button1, &QPushButton::clicked, dOpen, &QDialog::accept);
      connect(button2, &QPushButton::clicked, dOpen, &QDialog::reject);
      int result = dOpen->exec();

      if(result == QDialog::Accepted ){
        startComment = lineEdit->text();
        if( startComment == "") startComment = "No commet was typed.";
        leRunComment->setText(startComment);
      }else{
        LogMsg("Start Run aborted. ");
        return;
      }
    //}
    //TODO ============ elog
    QString elogMsg = "=============== Run-" + runIDStr + "<br />"
                    +  QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.z") + "<br />"
                    + "comment : " + startComment + "<br />" + 
                    + "----------------------------------------------";
    WriteElog(elogMsg, "Run-" + runIDStr, "Run", runID);
    //TODO ============ update expName.sh
    WriteExpNameSh();

  }else{
    LogMsg("=========================== Start no-save Run");
  }

  //============================= start digitizer
  for( int i =0 ; i < nDigi; i ++){
    if( digi[i]->IsDummy () ) continue;
    digi[i]->Reset();
    digi[i]->ProgramPHA(false);
    digi[i]->SetPHADataFormat(1);// only save 1 trace

    digi[i]->WriteValue("/ch/0..63/par/WaveAnalogProbe0", "ADCInput");

    if( chkSaveRun->isChecked() ){
      QString outFileName = rawDataFolder + "/" + expName + "_" + runIDStr+ "_" + QString::number(digi[i]->GetSerialNumber());
      qDebug() << outFileName;
      digi[i]->OpenOutFile(outFileName.toStdString());// overwrite
    }
    digi[i]->StartACQ();

    //TODO ========================== Sync start.
    readDataThread[i]->SetSaveData(chkSaveRun->isChecked());
    readDataThread[i]->start();
  }

  if( !scalar->isVisible() ) scalar->show();
  lbScalarACQStatus->setText("<font style=\"color: green;\"><b>ACQ On</b></font>");
  scalarThread->start();

  bnStartACQ->setEnabled(false);
  bnStopACQ->setEnabled(true);
  bnOpenScope->setEnabled(false);
  chkSaveRun->setEnabled(false);
  cbAutoRun->setEnabled(false);

  //TODO ======= Auto Run
  if( cbAutoRun->currentIndex() > 0 ){
    int timeMinite = cbAutoRun->currentData().toInt();
    runTimer->start(timeMinite * 60 * 1000); // unit is msec
  }

}

void MainWindow::StopACQ(){

  if( chkSaveRun->isChecked() ){
    //============ stop comment
    QDialog * dOpen = new QDialog(this);
    dOpen->setWindowTitle("Stop Run Comment");
    dOpen->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    dOpen->setMinimumWidth(600);
    connect(dOpen, &QDialog::finished, dOpen, &QDialog::deleteLater);

    QGridLayout * vlayout = new QGridLayout(dOpen);
    QLabel *label = new QLabel("Enter Run comment for ending <font style=\"color : red;\">Run-" +  runIDStr + "</font> : ", dOpen);
    QLineEdit *lineEdit = new QLineEdit(dOpen);
    QPushButton *button1 = new QPushButton("OK", dOpen);
    QPushButton *button2 = new QPushButton("Cancel", dOpen);

    vlayout->addWidget(label, 0, 0, 1, 2);
    vlayout->addWidget(lineEdit, 1, 0, 1, 2);
    vlayout->addWidget(button1, 2, 0);
    vlayout->addWidget(button2, 2, 1);

    connect(button1, &QPushButton::clicked, dOpen, &QDialog::accept);
    connect(button2, &QPushButton::clicked, dOpen, &QDialog::reject);
    int result = dOpen->exec();

    if(result == QDialog::Accepted ){
      stopComment = lineEdit->text();
      if( stopComment == "") stopComment = "No commet was typed.";
      leRunComment->setText(stopComment);
    }else{
      LogMsg("Cancel Run aborted. ");
      return;
    }
  }

  //TODO ======= Stop the Auto Run
  runTimer->stop();

  //=============== Stop digitizer
  for( int i = 0; i < nDigi; i++){
    if( digi[i]->IsDummy () ) continue;
    digi[i]->StopACQ();

    if( readDataThread[i]->isRunning()){
      readDataThread[i]->quit();
      readDataThread[i]->wait();
    }
    if( chkSaveRun->isChecked() ) digi[i]->CloseOutFile();
  }

  if( scalarThread->isRunning()){
    scalarThread->Stop();
    scalarThread->quit();
    scalarThread->wait();
  }
  if( chkSaveRun->isChecked() ){
    LogMsg("===========================  <b><font style=\"color : red;\">Run-" + runIDStr + "</font></b> stopped.");
  }else{
    LogMsg("===========================  no-Save Run stopped.");
  }
  bnStartACQ->setEnabled(true);
  bnStopACQ->setEnabled(false);
  bnOpenScope->setEnabled(true);
  chkSaveRun->setEnabled(true);

  if( chkSaveRun->isChecked() ){
    //TODO ============= elog
    QString msg = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.z") + "<br />";

    for( int i = 0; i < nDigi; i++){
      if( digi[i]->IsDummy () ) continue;
      msg += "FileSize ("+ QString::number(digi[i]->GetSerialNumber()) +"): " +  QString::number(digi[i]->GetTotalFilesSize()/1024./1024.) + " MB <br />";
    }
    msg += "comment : " + stopComment + "<br />"
        + "======================";
    AppendElog(msg, chromeWindowID);
  }

  lbScalarACQStatus->setText("<font style=\"color: red;\"><b>ACQ Off</b></font>");

  //if( scalarThread->isRunning()) printf("Scalar Thread still running.\n");
  //if( scalarThread->isFinished()) printf("Scalar Thread finsihed.\n");

}

void MainWindow::AutoRun(){

  if( cbAutoRun->currentData().toInt() > 0  ){ 
    //---- stop run

  }else{
    //----- stop run and start a new run
  }

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

      LogMsg("Opened digitizer : <font style=\"color:red;\">" + QString::number(digi[i]->GetSerialNumber()) + "</font>");
      bnStartACQ->setEnabled(true);
      bnStopACQ->setEnabled(false);
      bnOpenScope->setEnabled(true);
      chkSaveRun->setEnabled(true);
      bnOpenDigitizers->setEnabled(false);
      bnOpenDigitizers->setStyleSheet("");
      //cbAutoRun->setEnabled(true);
      cbAutoRun->setEnabled(false);

      readDataThread[i] = new ReadDataThread(digi[i], this);
      connect(readDataThread[i], &ReadDataThread::sendMsg, this, &MainWindow::LogMsg);
      //connect(readDataThread[i], &ReadDataThread::checkFileSize, this, &MainWindow::CheckOutFileSize);
      //connect(readDataThread[i], &ReadDataThread::endOfLastData, this, &MainWindow::CheckOutFileSize);

      //*------ search for settings_XXXX.dat
      QString settingFile = analysisPath + "/settings_" + QString::number(digi[i]->GetSerialNumber()) + ".dat";
      if( digi[i]->LoadSettingsFromFile( settingFile.toStdString().c_str() ) ){
        LogMsg("Found setting file <b>" + settingFile + "</b> and loaded.");
        digi[i]->SetSettingFileName(settingFile.toStdString());
      }else{
        LogMsg("<font style=\"color: red;\">Unable to found setting file <b>" + settingFile + "</b>. </font>");
        digi[i]->SetSettingFileName("");
        LogMsg("Reset digitizer and use deault setting");
        digi[i]->Reset();
        digi[i]->ProgramPHA(false);
      }

      SetUpScalar();
      bnOpenScalar->setEnabled(true);

    }else{
      digi[i]->SetDummy(i);
      LogMsg("Cannot open digitizer. Use a dummy with serial number " + QString::number(i) + " and " + QString::number(digi[i]->GetNChannels()) + " ch.");

      readDataThread[i] = NULL;
    }
  }

  bnDigiSettings->setEnabled(true);
  bnCloseDigitizers->setEnabled(true);

}

void MainWindow::CloseDigitizers(){

  if( digi == NULL) return;

  scalar->close();
  DeleteTriggerLineEdit(); // this use digi->GetNChannels(); 
  
  for( int i = 0; i < nDigi; i++){    
    if( digi[i] == NULL) return;
    digi[i]->CloseDigitizer();
    delete digi[i];

    LogMsg("Closed Digitizer : " + QString::number(digi[i]->GetSerialNumber()));

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


  bnOpenDigitizers->setEnabled(true);
  bnOpenDigitizers->setFocus();
  bnCloseDigitizers->setEnabled(false);
  bnDigiSettings->setEnabled(false);
  bnStartACQ->setEnabled(false);
  bnStopACQ->setEnabled(false);
  bnOpenScope->setEnabled(false);
  bnOpenScalar->setEnabled(false);
  chkSaveRun->setEnabled(false);
  cbAutoRun->setEnabled(false);

}

//^###################################################################### Open Scope
void MainWindow::OpenScope(){

  if( digi ){
    if( !scope ){
      scope = new Scope(digi, nDigi, readDataThread);
      connect(scope, &Scope::CloseWindow, this, [=](){ bnStartACQ->setEnabled(true); });
      connect(scope, &Scope::UpdateScalar, this, &MainWindow::UpdateScalar);
      connect(scope, &Scope::SendLogMsg, this, &MainWindow::LogMsg);
    }else{
      scope->show();
    }
  }

  bnStartACQ->setEnabled(false);
}

//^###################################################################### Open digitizer setting panel
void MainWindow::OpenDigitizersSettings(){
  LogMsg("Open digitizers Settings Panel");

  if( digiSetting == NULL){
    digiSetting = new DigiSettingsPanel(digi, nDigi);
    connect(digiSetting, &DigiSettingsPanel::sendLogMsg, this, &MainWindow::LogMsg);
    digiSetting->show();
  }else{
    digiSetting->show();
  }
}

//^###################################################################### Open Scaler, when DAQ is running
void MainWindow::OpenScaler(){
  scalar->show();
}

void MainWindow::SetUpScalar(){

  scalar->setGeometry(0, 0, 10 + nDigi * 230, 800);

  lbLastUpdateTime = new QLabel("Last update : ");
  lbLastUpdateTime->setAlignment(Qt::AlignCenter);
  scalarLayout->addWidget(lbLastUpdateTime, 0, 1, 1, 1 + nDigi);

  lbScalarACQStatus = new QLabel("ACQ status");
  lbScalarACQStatus->setAlignment(Qt::AlignCenter);
  scalarLayout->addWidget(lbScalarACQStatus, 1, 1, 1, 1 + nDigi);

  ///==== create the 1st row
  int rowID = 3;
  for( int ch = 0; ch < MaxNumberOfChannel; ch++){

    if( ch == 0 ){
      QLabel * lbCH_H = new QLabel("Ch", scalar); scalarLayout->addWidget(lbCH_H, rowID, 0);
    }  

    rowID ++;
    QLabel * lbCH = new QLabel(QString::number(ch), scalar);
    lbCH->setAlignment(Qt::AlignCenter);
    scalarLayout->addWidget(lbCH, rowID, 0);
  }
  
  ///===== create the trigger and accept
  leTrigger = new QLineEdit**[nDigi];
  leAccept = new QLineEdit**[nDigi];
  for( int iDigi = 0; iDigi < nDigi; iDigi++){
    rowID = 2;
    leTrigger[iDigi] = new QLineEdit *[digi[iDigi]->GetNChannels()];
    leAccept[iDigi] = new QLineEdit *[digi[iDigi]->GetNChannels()];
    for( int ch = 0; ch < MaxNumberOfChannel; ch++){

      if( ch == 0 ){
          QLabel * lbDigi = new QLabel("Digi-" + QString::number(digi[iDigi]->GetSerialNumber()), scalar); 
          lbDigi->setAlignment(Qt::AlignCenter);
          scalarLayout->addWidget(lbDigi, rowID, 2*iDigi+1, 1, 2);

          rowID ++;

          QLabel * lbA = new QLabel("Trig. [Hz]", scalar);
          lbA->setAlignment(Qt::AlignCenter);
          scalarLayout->addWidget(lbA, rowID, 2*iDigi+1);
          QLabel * lbB = new QLabel("Accp. [Hz]", scalar);
          lbB->setAlignment(Qt::AlignCenter);
          scalarLayout->addWidget(lbB, rowID, 2*iDigi+2);
      }
    
      rowID ++;
      
      leTrigger[iDigi][ch] = new QLineEdit();
      leTrigger[iDigi][ch]->setReadOnly(true);
      leTrigger[iDigi][ch]->setAlignment(Qt::AlignRight);
      scalarLayout->addWidget(leTrigger[iDigi][ch], rowID, 2*iDigi+1);

      leAccept[iDigi][ch] = new QLineEdit();
      leAccept[iDigi][ch]->setReadOnly(true);
      leAccept[iDigi][ch]->setAlignment(Qt::AlignRight);
      leAccept[iDigi][ch]->setStyleSheet("background-color: #F0F0F0;");
      scalarLayout->addWidget(leAccept[iDigi][ch], rowID, 2*iDigi+2);
    }
  }

}

void MainWindow::DeleteTriggerLineEdit(){

  if( leTrigger == NULL ) return;

  for( int i = 0; i < nDigi; i++){
    for( int ch = 0; ch < digi[i]->GetNChannels(); ch ++){
      delete leTrigger[i][ch];
      delete leAccept[i][ch];
    }
    delete [] leTrigger[i];
    delete [] leAccept[i];
  }
  delete [] leTrigger;
  leTrigger = NULL;
  leAccept = NULL;

}

void MainWindow::UpdateScalar(){
  if( !digi ) return;

  lbLastUpdateTime->setText("Last update: " + QDateTime::currentDateTime().toString("MM.dd hh:mm:ss"));

  if( influx ) influx->ClearDataPointsBuffer();
  std::string haha[MaxNumberOfChannel] = {""};
  double acceptRate[MaxNumberOfChannel] = {0};

  ///===== Get trigger for all channel
  for( int iDigi = 0; iDigi < nDigi; iDigi ++ ){
    if( digi[iDigi]->IsDummy() ) return;
    
    //=========== use ReadStat to get the trigger rate
    //digiMTX.lock();
    //digi[iDigi]->ReadStat(); // digitizer update it every 500 msec;
    //digiMTX.unlock();
    //for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch ++){
    //  leTrigger[iDigi][ch]->setText(QString::number(digi[iDigi]->GetTriggerCount(ch)*1e9*1.0/ digi[iDigi]->GetRealTime(ch)));
    //}

    //=========== another method, directly readValue
    digiMTX.lock();
    for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch ++){
      std::string time = digi[iDigi]->ReadChValue(std::to_string(ch), DIGIPARA::CH::ChannelRealtime); // for refreashing SelfTrgRate and SavedCount
      haha[ch] = digi[iDigi]->ReadChValue(std::to_string(ch), DIGIPARA::CH::SelfTrgRate);
      leTrigger[iDigi][ch]->setText(QString::fromStdString(haha[ch]));
      std::string kaka = digi[iDigi]->ReadChValue(std::to_string(ch), DIGIPARA::CH::ChannelSavedCount);
      acceptRate[ch] = atoi(kaka.c_str())*1e9*1.0 / atol(time.c_str());
      //if( kaka != "0" )  printf("%s, %s | %.2f\n", time.c_str(), kaka.c_str(), acceptRate);
      leAccept[iDigi][ch]->setText(QString::number(acceptRate[ch],'f', 1));
    }
    digiMTX.unlock();

    ///============== push the trigger, acceptRate rate database
    if( influx ){
      for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch++ ){
        influx->AddDataPoint("Rate,Bd=" + std::to_string(digi[iDigi]->GetSerialNumber()) + ",Ch=" + std::to_string(ch) + " value=" + haha[ch]);
        if( !std::isnan(acceptRate[ch]) )  influx->AddDataPoint("AccpRate,Bd=" + std::to_string(digi[iDigi]->GetSerialNumber()) + ",Ch=" + std::to_string(ch) + " value=" + std::to_string(acceptRate[ch]));
      }
    }
  }

  if( influx && influx->GetDataLength() > 0 ){
    //influx->PrintDataPoints();
    influx->WriteData(DatabaseName.toStdString());
    influx->ClearDataPointsBuffer();
  }
}

//^###################################################################### Program Settings
void MainWindow::ProgramSettingsPanel(){

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

bool MainWindow::LoadProgramSettings(){

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

    DecodeIPList();
    SetupInflux();
    return true;

  }else{

    bnProgramSettings->setStyleSheet("color: red;");
    bnNewExp->setEnabled(false);
    bnOpenDigitizers->setEnabled(false);
    return false;
  }
}

void MainWindow::DecodeIPList(){
  //------- decode IPListStr
  nDigi = 0;
  IPList.clear();
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
}

void MainWindow::SetupInflux(){
  if( influx ) {
    delete influx;
    influx = NULL;
  }
  if( DatabaseIP != ""){
    influx = new InfluxDB(DatabaseIP.toStdString(), false);

    if( influx->TestingConnection() ){
      LogMsg("<font style=\"color : green;\"> InfluxDB URL (<b>"+ DatabaseIP + "</b>) is Valid </font>");

      //==== chck database exist
      std::vector<std::string> databaseList = influx->GetDatabaseList();
      bool foundDatabase = false;
      for( int i = 0; i < (int) databaseList.size(); i++){
        if( databaseList[i] == DatabaseName.toStdString() ) foundDatabase = true;
      }

      if( foundDatabase ){
        LogMsg("<font style=\"color : green;\"> Database <b>" + DatabaseName + "</b> found.");

        influx->AddDataPoint("Rate,Bd=0,Ch=0 value=1");
        influx->WriteData(DatabaseName.toStdString());
        influx->ClearDataPointsBuffer();
        if( influx->IsWriteOK() ){
          LogMsg("<font style=\"color : green;\">test write database OK.</font>");
        }else{
          LogMsg("<font style=\"color : red;\">test write database FAIL.</font>");
        }

      }else{
        LogMsg("<font style=\"color : red;\"> Database <b>" + DatabaseName + "</b> NOT found.");
        delete influx;
        influx = NULL;
      }

    }else{
      LogMsg("<font style=\"color : red;\"> InfluxDB URL (<b>"+ DatabaseIP + "</b>) is NOT Valid </font>");
      delete influx;
      influx = NULL;
    }
  }else{
    LogMsg("No database is provided.");
  }
}

void MainWindow::CheckElog(){

  WriteElog("Checking elog writing", "Testing communication", "checking");

  if( elogID > 0 ){
    LogMsg("Ckecked Elog writing. OK.");

    //TODO =========== chrome windowID
    AppendElog("Check Elog append.", chromeWindowID);
    if( elogID > 0 ){
      LogMsg("Checked Elog Append. OK.");
    }else{
      LogMsg("<font style=\"color : red;\">Checked Elog Append. FAIL. (no elog will be used.) </font>");
    }

  }else{
    LogMsg("<font style=\"color : red;\">Checked Elog Write. FAIL. (no elog will be used.) </font>");
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

  DecodeIPList();
  SetupInflux();

  LoadExpSettings();

}

//^###################################################################### Setup new exp

void MainWindow::SetupNewExpPanel(){

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

  LoadExpSettings();

  dialog.exec();

}
 
bool MainWindow::LoadExpSettings(){
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

  CheckElog();

  return true;

}

void MainWindow::CreateNewExperiment(const QString newExpName){
  
  LogMsg("Creating new Exp. : <font style=\"color: red;\">" + newExpName + "</font>");

  expName = newExpName;
  runID = -1;
  elogID = 0;

  CreateRawDataFolderAndLink();
  WriteExpNameSh();

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

  expName = newExpName;
  CreateRawDataFolderAndLink();
  LoadExpSettings();

}

void MainWindow::WriteExpNameSh(){
  //----- create the expName.sh
  QFile file2(analysisPath + "/expName.sh");
  
  file2.open(QIODevice::Text | QIODevice::WriteOnly);
  file2.write(("expName="+ expName + "\n").toStdString().c_str());
  file2.write(("rawDataPath="+ rawDataFolder + "\n").toStdString().c_str());
  file2.write(("runID="+std::to_string(runID)+"\n").c_str());
  file2.write(("elogID="+std::to_string(elogID)+"\n").c_str());
  file2.write("//------------end of file.");
  file2.close();
  LogMsg("Saved expName.sh to <b>"+ analysisPath + "/expName.sh<b>.");

}

void MainWindow::CreateRawDataFolderAndLink(){

  //----- create data folder
  rawDataFolder = dataPath + "/" + expName;
  QDir dir;
  if( !dir.exists(rawDataFolder)){
    if( dir.mkdir(rawDataFolder)){
      LogMsg("<b>" + rawDataFolder + "</b> created." );
    }else{
      LogMsg("<font style=\"color:red;\"><b>" + rawDataFolder + "</b> cannot be created. Access right problem? </font>" );
    }
  }else{
      LogMsg("<b>" + rawDataFolder + "</b> already exist." );
  }

  //----- create analysis Folder
  QDir anaDir;
  if( !anaDir.exists(analysisPath)){
    if( anaDir.mkdir(analysisPath)){
      LogMsg("<b>" + analysisPath + "</b> created." );
    }else{
      LogMsg("<font style=\"color:red;\"><b>" + analysisPath + "</b> cannot be created. Access right problem?</font>" );
    }
  }else{
    LogMsg("<b>" + analysisPath + "</b> already exist.");
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
      LogMsg("<font style=\"color:red;\">Symbolic link  <b>" + linkName +"</b> -> " + rawDataFolder + " cannot be created. </font>");
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

void MainWindow::WriteElog(QString htmlText, QString subject, QString category, int runNumber){
  
  if( elogID < 0 ) return;
  if( expName == "" ) return;

  QStringList arg;
  arg << "-h" << ElogIP << "-p" << "8080" << "-l" << expName << "-u" << "GeneralFSU" << "fsuphysics-888" 
      << "-a" << "Author=GeneralFSU" ;
  if( runNumber > 0 ) arg << "-a" << "Run=" + QString::number(runNumber);
  if( category != "" ) arg << "-a" << "Category=" + category;

  arg << "-a" << "Subject=" + subject 
      << "-n " << "2" <<  htmlText  ;

  QProcess elogBash(this);
  elogBash.start("elog", arg); 
  elogBash.waitForFinished();

  QString output = QString::fromUtf8(elogBash.readAllStandardOutput());

  int index = output.indexOf("ID=");
  if( index != -1 ){
    elogID = output.mid(index+3).toInt();
  }else{
    elogID = -1;
  }

}

void MainWindow::AppendElog(QString appendHtmlText, int screenID){
  if( elogID < 1 ) return;
  if( expName == "" ) return;
  
  QProcess elogBash(this);

  QStringList arg;
  arg << "-h" << ElogIP << "-p" << "8080" << "-l" << expName << "-u" << "GeneralFSU" << "fsuphysics-888" << "-w" << QString::number(elogID);

  //retrevie the elog
  elogBash.start("elog", arg); 
  elogBash.waitForFinished();

  QString output = QString::fromUtf8(elogBash.readAllStandardOutput());
  //qDebug() << output;

  QString separator = "========================================";

  int index = output.indexOf(separator);
  if( index != -1){

    QString originalHtml = output.mid(index + separator.length());

    arg.clear();
    arg << "-h" << ElogIP << "-p" << "8080" << "-l" << expName << "-u" << "GeneralFSU" << "fsuphysics-888" << "-e" << QString::number(elogID)
        << "-n" << "2" << originalHtml + "<br>" + appendHtmlText;

    if( screenID >= 0) {
      
      //TODO =========== chrome windowID
      
      QScreen * screen = QGuiApplication::primaryScreen();
      if( screen){
        QPixmap screenshot = screen->grabWindow(screenID);
        screenshot.save("screenshot.png");
        arg << "-f" << "screenshot.png";
      }
    }

    elogBash.start("elog", arg); 
    elogBash.waitForFinished();

    output = QString::fromUtf8(elogBash.readAllStandardOutput());
    index = output.indexOf("ID=");
    if( index != -1 ){
      elogID = output.mid(index+3).toInt();
    }else{
      elogID = -1;
    }

  }else{
    elogID = -1;
  }

}

