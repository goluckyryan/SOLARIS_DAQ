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
#include <QCoreApplication>
//#include <QChartView>
//#include <QValueAxis>
#include <QStandardItemModel>
#include <QApplication>
#include <QDateTime>
#include <QProcess>
#include <QScreen>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <unistd.h>

//------ static memeber
Digitizer2Gen ** MainWindow::digi = nullptr;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent){

  setWindowTitle("FSU SOLARIS DAQ");
  setGeometry(500, 100, 1000, 500);
  QIcon icon("SOLARIS_favicon.png");
  setWindowIcon(icon);

  programPath = QDir::currentPath();

  nDigi = 0;
  nDigiConnected = 0;
  digiSetting = nullptr;
  influx = nullptr;
  readDataThread = nullptr;

  runTimer = new QTimer();
  needManualComment = true;
  ACQStopButtonPressed = false;
  isACQRunning = false;

  {
    scalarOutputInflux = false;
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
    scalarLayout->setAlignment(Qt::AlignTop);

    leTrigger = nullptr;
    leAccept = nullptr;
    lbFileSize = nullptr;

    scalarThread = new TimingThread(); // 2 sec is default
    //scalarThread->SetWaitTimeSec(2); 
    connect(scalarThread, &TimingThread::TimeUp, this, &MainWindow::UpdateScalar);

  }
  
  solarisSetting = nullptr;
  scope = nullptr;
  digiSetting = nullptr;

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
    connect(bnOpenDigitizers, &QPushButton::clicked, this, &MainWindow::OpenDigitizers);

    bnCloseDigitizers = new QPushButton("Close Digitizers", this);
    bnCloseDigitizers->setEnabled(false);
    connect(bnCloseDigitizers, &QPushButton::clicked, this, &MainWindow::CloseDigitizers);
  
    bnDigiSettings = new QPushButton("Digitizers Settings", this);
    bnDigiSettings->setEnabled(false);
    connect(bnDigiSettings, &QPushButton::clicked, this, &MainWindow::OpenDigitizersSettings);

    bnSOLSettings = new QPushButton("SOLARIS Settings", this);
    bnSOLSettings->setEnabled(false);
    connect(bnSOLSettings, &QPushButton::clicked, this, &MainWindow::OpenSOLARISpanel);

    bnSyncHelper = new QPushButton("Sync Helper (Not Set)", this);
    bnSyncHelper->setEnabled(false);
    connect(bnSyncHelper, &QPushButton::clicked, this, &MainWindow::OpenSyncHelper);

    layout1->addWidget(bnProgramSettings, 0, 0);
    layout1->addWidget(bnNewExp, 0, 1);
    layout1->addWidget(lExpName, 0, 2);
    layout1->addWidget(leExpName, 0, 3);

    layout1->addWidget(bnSyncHelper, 1, 0);
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
    chkSaveRun->setChecked(false);
    chkSaveRun->setEnabled(false);
    connect(chkSaveRun, &QCheckBox::clicked, this, [=]() { cbAutoRun->setEnabled(chkSaveRun->isChecked()); });

    cbAutoRun = new QComboBox(this);
    cbAutoRun->addItem("Single infinte",  0);
    cbAutoRun->addItem("Single  1 min",   1);
    cbAutoRun->addItem("Single 30 mins", 30);
    cbAutoRun->addItem("Single 60 mins", 60);
    cbAutoRun->addItem("Single 2 hrs",  120);
    cbAutoRun->addItem("Single 3 hrs",  180);
    cbAutoRun->addItem("Single 5 hrs",  300);
    cbAutoRun->addItem("Every  1 mins",  -1);
    cbAutoRun->addItem("Every 30 mins", -30);
    cbAutoRun->addItem("Every 60 mins", -60);
    cbAutoRun->addItem("Every 2 hrs",  -120);
    cbAutoRun->addItem("Every 3 hrs",  -180);
    cbAutoRun->addItem("Every 5 hrs",  -300);
    cbAutoRun->setEnabled(false);

    cbDataFormat = new QComboBox(this);
    cbDataFormat->addItem("Everything", DataFormat::ALL);
    cbDataFormat->addItem("1 trace", DataFormat::OneTrace);
    cbDataFormat->addItem("No trace", DataFormat::NoTrace);
    cbDataFormat->addItem("Minimum", DataFormat::Minimum);
    cbDataFormat->addItem("Min + fineTimestamp", DataFormat::MiniWithFineTime);
    cbDataFormat->setCurrentIndex(3);
    cbDataFormat->setEnabled(false);

    bnStartACQ = new QPushButton("Start ACQ", this);
    bnStartACQ->setEnabled(false);
    //connect(bnStartACQ, &QPushButton::clicked, this, &MainWindow::StartACQ);
    connect(bnStartACQ, &QPushButton::clicked, this, &MainWindow::AutoRun);
    
    bnStopACQ = new QPushButton("Stop ACQ", this);
    bnStopACQ->setEnabled(false);
    connect(bnStopACQ, &QPushButton::clicked, this, [=](){

      LogMsg("@@@@@@@@@ Stop ACQ Button Pressed.");
      ACQStopButtonPressed = true;
      needManualComment = true;
      runTimer->stop();
      StopACQ();

      if( !isACQRunning ){
        bnStartACQ->setEnabled(true);
        bnStopACQ->setEnabled(false);
        bnComment->setEnabled(false);
        bnOpenScope->setEnabled(true);
        chkSaveRun->setEnabled(true);
        cbDataFormat->setEnabled(true);
        if(chkSaveRun->isChecked() ) cbAutoRun->setEnabled(true);
        if( digiSetting ) digiSetting->EnableControl();
      }

    });

    QLabel * lbRunComment = new QLabel("Run Comment : ", this);
    lbRunComment->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    leRunComment = new QLineEdit(this);
    leRunComment->setReadOnly(true);
    leRunComment->setStyleSheet("background-color: #F3F3F3;");

    bnComment = new QPushButton("Append Comment", this);
    connect(bnComment, &QPushButton::clicked, this, &MainWindow::AppendComment);
    bnComment->setEnabled(false);
    
    layout2->addWidget(lbRawDataPath, 0, 0);
    layout2->addWidget(leRawDataPath, 0, 1, 1, 5);
    layout2->addWidget(bnOpenScalar, 0, 6);

    layout2->addWidget(lbRunID,       1, 0);
    layout2->addWidget(leRunID,       1, 1);
    layout2->addWidget(chkSaveRun,    1, 2);
    layout2->addWidget(cbAutoRun,     1, 3);
    layout2->addWidget(cbDataFormat,  1, 4);
    layout2->addWidget(bnStartACQ,    1, 5);
    layout2->addWidget(bnStopACQ,     1, 6);

    layout2->addWidget(lbRunComment, 2, 0);
    layout2->addWidget(leRunComment,   2, 1, 1, 5);
    layout2->addWidget(bnComment, 2, 6);

    layout2->setColumnStretch(0, 1);
    layout2->setColumnStretch(1, 1);
    layout2->setColumnStretch(2, 1);
    layout2->setColumnStretch(3, 1);
    layout2->setColumnStretch(4, 1);
    layout2->setColumnStretch(5, 3);
    layout2->setColumnStretch(6, 3);

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

  if( LoadProgramSettings() )  LoadExpNameSh();

}

MainWindow::~MainWindow(){

  printf("- %s\n", __func__);

  LogMsg("Closing SOLARIS DAQ.");

  if( !expDataPath.isEmpty() ){
    QDir dir(expDataPath + "/Logs/");
    if( !dir.exists() ) dir.mkpath(".");

    QFile file(expDataPath + "/Logs/Log_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".dat");
    printf("-------- Save log msg to %s\n", file.fileName().toStdString().c_str());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream stream(&file);
      stream << logInfo->toPlainText();
      file.close();
    }
  }

  printf("-------- remove %s\n", DAQLockFile);
  remove(DAQLockFile);

  printf("-------- delete Solaris panel\n");
  if( solarisSetting ) {
    delete solarisSetting;
    solarisSetting = nullptr;
  }
  printf("-------- delete scope\n");
  if( scope ) {
    delete scope;
    scope = nullptr;
  } 
  printf("-------- delete digiSetting\n");
  if( digiSetting  ) {
    delete digiSetting;
    digiSetting = nullptr;
  }

  printf("-------- delete readData Thread\n");
  if( digi ){
    for( int i = 0; i < nDigi ; i++){
      if( digi[i]->IsDummy()) continue;
      //printf("=== %d %p\n", i, readDataThread[i]);
      if( readDataThread[i]->isRunning()) StopACQ();
    }
  }
  CloseDigitizers(); // SOlaris panel, digiSetting, scope are also deleted.

  printf("-------- delete scalar Thread\n");
  if( scalarThread->isRunning()){
    scalarThread->Stop();
    scalarThread->quit();
    scalarThread->wait();
  }
  CleanUpScalar();
  delete scalarThread;
  
  printf("-------- delete influx\n");
  if( influx != NULL ) {    
    influx->ClearDataPointsBuffer();
    influx->AddDataPoint("ProgramStart value=0");
    influx->WriteData(DatabaseName.toStdString());
    delete influx;
  }

  printf("--- end of %s\n", __func__);

}

//*################################################################ 
//*################################################################ ACQ control 
int MainWindow::StartACQ(){

  ACQStopButtonPressed = false;

  if( chkSaveRun->isChecked() ){
    runID ++;
    leRunID->setText(QString::number(runID));

    runIDStr = QString::number(runID).rightJustified(3, '0');
    LogMsg("=========================== Start <b><font style=\"color : red;\">Run-" + runIDStr + "</font></b>");

    if( needManualComment  ){
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
        startComment = "Start Comment: " + startComment;
      }else{
        LogMsg("Start Run aborted. ");
        runID --;
        leRunID->setText(QString::number(runID));
        return 0 ;
      }

      if( cbAutoRun -> currentData().toInt() != 0 ){
        startComment = startComment +  ". AutoRun for " + cbAutoRun->currentText();
      }

      leRunComment->setText(startComment);

    }else{
      //==========TODO auto run comment
      startComment = "AutoRun for " + cbAutoRun->currentText();
      leRunComment->setText(startComment);
    }  

  }else{
    LogMsg("=========================== Start no-save Run");
  }

  //============================= start digitizer
  for( int i = nDigi-1 ; i >= 0; i --){
    if( digi[i]->IsDummy () ) continue;

    for( int ch = 0; ch < (int) digi[i]->GetNChannels(); ch ++) oldTimeStamp[i][ch] = 0;

    //digi[i]->SetPHADataFormat(1);// only save 1 trace
    int dataFormatID = cbDataFormat->currentData().toInt();
    digi[i]->SetDataFormat(dataFormatID);

    if( dataFormatID == DataFormat::ALL || dataFormatID == DataFormat::OneTrace ){
      digi[i]->WriteValue(PHA::CH::WaveSaving, "Always", -1);
    }else{
      digi[i]->WriteValue(PHA::CH::WaveSaving, "OnRequest", -1);
    }

    //Additional settings, it is better user to control
    //if( cbDataFormat->currentIndex() <  2 )  {
    //  digi[i]->WriteValue("/ch/0..63/par/WaveAnalogProbe0", "ADCInput");
    //  digi[i]->WriteValue(PHA::CH::WaveSaving, "True", -1);
    //}

    if( chkSaveRun->isChecked() ){

      QString runFolder = rawDataPath + "/";
      if( isSaveSubFolder ) {
        runFolder += "run" + runIDStr + "/";
        CreateFolder(runFolder, "for " + runIDStr);
      }

      //Save setting to raw data with run ID
      QString fileSetting =  runFolder + expName + "_" + runIDStr + "XSetting_" + QString::number(digi[i]->GetSerialNumber()) + ".dat";
      // name should be [ExpName]_[runID]_[digiID]_[digiSerialNumber]_[acculmulate_count].sol
      QString outFileName =  runFolder + expName + "_" 
                                       + runIDStr + "_" 
                                       + QString::number(i).rightJustified(2, '0')  + "_" 
                                       + QString::number(digi[i]->GetSerialNumber());

      digi[i]->SaveSettingsToFile(fileSetting.toStdString().c_str());
      qDebug() << outFileName;
      digi[i]->OpenOutFile(outFileName.toStdString());// overwrite
    }
    digi[i]->StartACQ();

    //TODO ========================== Sync start.
    readDataThread[i]->SetSaveData(chkSaveRun->isChecked());
    readDataThread[i]->start();
  }

  if(chkSaveRun->isChecked() ){
    QString startTimeStr = QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss");
    LogMsg("<font style=\"color : blue;\"> All Digitizers started. </font>");
    // ============ elog
    QString elogMsg = "=============== Run-" + runIDStr + "<br />"
                    +  startTimeStr + "<br />"
                    + "comment : " + startComment + "<br />" + 
                    + "----------------------------------------------";
    WriteElog(elogMsg, "Run-" + runIDStr, "Run", runID);
    // ============ update expName.sh
    WriteExpNameSh();

    WriteRunTimeStampDat(true, startTimeStr);
  }

  if( influx ){
    influx->ClearDataPointsBuffer();
    if( chkSaveRun->isChecked() ){
      influx->AddDataPoint("RunID,start=1 value=" + std::to_string(runID) + ",expName=\"" + expName.toStdString() + "\",comment=\"" + startComment.replace(' ', '_').toStdString() + "\"");
    }
    influx->AddDataPoint("StartStop value=1");
    influx->WriteData(DatabaseName.toStdString());
  }

  if( !scalar->isVisible() ) {
    scalar->show();
    if( !scalarThread->isRunning() ) scalarThread->start();
  }
  isACQRunning = True;
  lbScalarACQStatus->setText("<font style=\"color: green;\"><b>ACQ On</b></font>");
  //scalarThread->start();
  scalarOutputInflux = true;

  return 1;

}

void MainWindow::StopACQ(){

  if( !isACQRunning ) return;

  if( chkSaveRun->isChecked() ){
    //============ stop comment
    if( needManualComment ){
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
        stopComment = "Stop Comment: " + stopComment;
        leRunComment->setText(stopComment);
      }else{
        LogMsg("Cancel Run aborted. ");
        return;
      }
    }else{
      stopComment = "End of AutoRun for " + cbAutoRun->currentText();
      leRunComment->setText(stopComment);
    }
  }

  //=============== Stop digitizer
  for( int i = nDigi - 1; i >= 0; i--){
    if( digi[i]->IsDummy () ) continue;
    digiMTX[i].lock();
    digi[i]->StopACQ();
    // readDataThread[i]->SuppressFileSizeMsg();
    digi[i]->WriteValue(PHA::CH::WaveSaving, "OnRequest", -1);
    digiMTX[i].unlock();
  }
  isACQRunning = false;
  lbScalarACQStatus->setText("<font style=\"color: red;\"><b>ACQ Off</b></font>");

  QString stopTimeStr = QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss");
  scalarOutputInflux = false;

  if( chkSaveRun->isChecked() ){   
    LogMsg("===========================  <b><font style=\"color : red;\">Run-" + runIDStr + "</font></b> stopped.");
    LogMsg("<font style=\"color : blue;\">Please wait for collecting all remaining data.</font>");
    WriteRunTimeStampDat(false, stopTimeStr);

    // ============= elog
    QString msg = stopTimeStr + "<br />";
    for( int i = 0; i < nDigi; i++){
      if( digi[i]->IsDummy () ) continue;
      msg += "FileSize ("+ QString::number(digi[i]->GetSerialNumber()) +"): " +  QString::number(digi[i]->GetTotalFilesSize()/1024./1024.) + " MB <br />";
    }
    msg += "comment : " + stopComment + "<br />"
        + "======================";
    AppendElog(msg, chromeWindowID);

  }else{
    LogMsg("===========================  no-Save Run stopped.");
  }

  if( influx ){
    influx->ClearDataPointsBuffer();
    if( chkSaveRun->isChecked() ){
      influx->AddDataPoint("RunID,start=0 value=" + std::to_string(runID) + ",expName=\"" + expName.toStdString()+ "\",comment=\"" + stopComment.replace(' ', '_').toStdString() + "\"");
    }
    influx->AddDataPoint("StartStop value=0");
    influx->WriteData(DatabaseName.toStdString());
  }

  if( chkSaveRun->isChecked() ) LogMsg("Collecting remaining data from the digitizers... ");
  for( int i = nDigi -1; i >=0; i--){
    if( readDataThread[i]->isRunning()){
      if( !chkSaveRun->isChecked() ) readDataThread[i]->Stop(); // if it is a save run, don't force stop the readDataThread, wait for it.
      readDataThread[i]->quit();
      readDataThread[i]->wait();
    }
    if( chkSaveRun->isChecked() ) {
       digi[i]->CloseOutFile();
       LogMsg("Digi-" + QString::number(digi[i]->GetSerialNumber()) + " is done collecting all data.");
    }
  }

  if( chkSaveRun->isChecked() ){
    LogMsg("Run " + programPath + "/scripts/endRunScript.sh" );
    QProcess::startDetached(programPath + "/scripts/endRunScript.sh");
  }
  
  LogMsg("<b><font style=\"color : green;\">SOLARIS DAQ is ready for next run.</font></b>");

}

void MainWindow::AutoRun(){

  if( chkSaveRun->isChecked() == false){
    if( StartACQ() ){  
      bnStartACQ->setEnabled(false);
      bnStopACQ->setEnabled(true);
      bnComment->setEnabled(false);
      bnOpenScope->setEnabled(false);
      chkSaveRun->setEnabled(false);
      cbAutoRun->setEnabled(false);
      cbDataFormat->setEnabled(false);
      if( digiSetting ) digiSetting->EnableControl();
    }
    return;
  }

  needManualComment = true;
  int isRun = 0;
  ///=========== infinite single run
  if( cbAutoRun->currentData().toInt() == 0 ){
    isRun = StartACQ();
    disconnect(runTimer,  &QTimer::timeout, nullptr, nullptr);
  }else{
    isRun = StartACQ();
    connect(runTimer, &QTimer::timeout, this, [=](){
      StopACQ();
      if( cbAutoRun->currentData().toInt() > 0 ) {
        bnStartACQ->setEnabled(true);
        bnStopACQ->setEnabled(false);
        bnComment->setEnabled(false);
        bnOpenScope->setEnabled(true);
        chkSaveRun->setEnabled(true);
        cbAutoRun->setEnabled(true);
        cbDataFormat->setEnabled(true);
        if( digiSetting ) digiSetting->EnableControl();
      }else{
        LogMsg("Wait for 10 sec for next Run....");
        elapsedTimer.invalidate();
        elapsedTimer.start();
        while(elapsedTimer.elapsed() < 10000) {
          QCoreApplication::processEvents();
          if( ACQStopButtonPressed ) {
            ACQStopButtonPressed = false;
            return;
          }
        }
        StartACQ();
      } 
    });
  }

  if( isRun == 0 ) return;

  int timeMiliSec = cbAutoRun->currentData().toInt() * 60 * 1000;

  ///=========== single timed run
  if( cbAutoRun->currentData().toInt() > 0 ){
    runTimer->setSingleShot(true);
    runTimer->start(timeMiliSec);
    needManualComment = false;
  }

  ///=========== infinite timed run
  if( cbAutoRun->currentData().toInt() < 0 ){
    runTimer->setSingleShot(false);
    runTimer->start(abs(timeMiliSec));
    needManualComment = false;
  }

  bnStartACQ->setEnabled(false);
  bnStopACQ->setEnabled(true);
  if(chkSaveRun->isChecked()) bnComment->setEnabled(true);
  bnOpenScope->setEnabled(false);
  chkSaveRun->setEnabled(false);
  cbDataFormat->setEnabled(false);
  cbAutoRun->setEnabled(false);
  if( digiSetting ) digiSetting->EnableControl();

}

//*###################################################################### 
//*###################################################################### open and close digitizer
void MainWindow::OpenDigitizers(){

  LogMsg("<font style=\"color:blue;\">Opening " + QString::number(nDigi) + " Digitizers..... </font>");

  digi = new Digitizer2Gen*[nDigi];
  readDataThread = new ReadDataThread*[nDigi];

  nDigiConnected = 0;

  //Check path exist
  QDir dir(expDataPath + "/Settings/");
  if( !dir.exists() ) dir.mkpath(".");

  for( int i = 0; i < nDigi; i++){

    LogMsg("IP : " + IPList[i] + " | " + QString::number(i+1) + "/" + QString::number(nDigi));

    digi[i] = new Digitizer2Gen();
    digi[i]->OpenDigitizer(("dig2://" + IPList[i]).toStdString().c_str());

    if(digi[i]->IsConnected()){

      LogMsg("Opened digitizer : <font style=\"color:red;\">" + QString::number(digi[i]->GetSerialNumber()) + "</font>");

      readDataThread[i] = new ReadDataThread(digi[i], i, this);
      connect(readDataThread[i], &ReadDataThread::sendMsg, this, &MainWindow::LogMsg);

      //*------ search for settings_XXX_YYY.dat, YYY is DPP-type, XXX is serial number
      QString settingFile = expDataPath + "/Settings/setting_" + QString::number(digi[i]->GetSerialNumber()) + "_" + QString::fromStdString(digi[i]->GetFPGAType().substr(4)) + ".dat";
      if( digi[i]->LoadSettingsFromFile( settingFile.toStdString().c_str() ) ){
        LogMsg("Found setting file <b>" + settingFile + "</b> and loading. please wait.");
        digi[i]->SetSettingFileName(settingFile.toStdString());
        LogMsg("done settings.");
      }else{
        LogMsg("<font style=\"color: red;\">Unable to found setting file <b>" + settingFile + "</b>. </font>");
        digi[i]->SetSettingFileName("");
        //LogMsg("Reset digitizer And set default PHA settings.");        
        //digi[i]->Reset();
        //digi[i]->ProgramBoard(false);
      }
      
      nDigiConnected ++;

      if( maxNumChannelAcrossDigitizer < digi[i]->GetNChannels()) maxNumChannelAcrossDigitizer = digi[i]->GetNChannels();

      for( int ch = 0; ch < (int) digi[i]->GetNChannels(); ch++) {
        oldTimeStamp[i][ch] = 0;
        oldSavedCount[i][ch] = 0;
      }
    }else{
      digi[i]->SetDummy(i);
      LogMsg("Cannot open digitizer. Use a dummy with serial number " + QString::number(i) + " and " + QString::number(digi[i]->GetNChannels()) + " ch.");

      readDataThread[i] = NULL;
    }
    QCoreApplication::processEvents(); // to prevent application busy.
  }

  if( nDigiConnected > 0 ){
    SetUpScalar();
    bnStartACQ->setEnabled(true);
    bnStopACQ->setEnabled(false);
    bnComment->setEnabled(false);
    bnOpenScope->setEnabled(true);
    chkSaveRun->setEnabled(true);
    if( nDigiConnected == 1 ) {
      bnSyncHelper->setEnabled(false);
    }else{
      bnSyncHelper->setEnabled(true);
    }
    bnOpenDigitizers->setEnabled(false);
    bnOpenDigitizers->setStyleSheet("");
    cbAutoRun->setEnabled(true);
    cbDataFormat->setEnabled(true);
    bnOpenScalar->setEnabled(true);
  }

  bnDigiSettings->setEnabled(true);
  bnCloseDigitizers->setEnabled(true);

  bnProgramSettings->setEnabled(false);
  bnNewExp->setEnabled(false);

  bnSOLSettings->setEnabled(CheckSOLARISpanelOK());

}

void MainWindow::CloseDigitizers(){

  if( digi == NULL) return;

  if( scope ){
    scope->close();
    delete scope;
    scope = NULL;
  }

  if(scalar && nDigiConnected > 0 ){ // scalar is child of this, This MUST after scope, because scope tell scalar to update ACQ status
    scalar->close();
    if( scalarThread->isRunning()){
      scalarThread->Stop();
      scalarThread->quit();
      scalarThread->wait();
    }
    CleanUpScalar(); // this use digi->GetNChannels(); 
  }
  
  if( digiSetting ){
    digiSetting->close();
    delete digiSetting;
    digiSetting = NULL;
  }

  if( solarisSetting ){
    solarisSetting->close();
    delete solarisSetting;
    solarisSetting = NULL;
  }

  for( int i = 0; i < nDigi; i++){    
    if( digi[i] == NULL) return;

    if( digi[i]->IsConnected() ){
      int digiSN = digi[i]->GetSerialNumber();
      LogMsg("Save digi-"+ QString::number(digiSN) + " Settings to " + programPath + "/tempSettings/");
      digi[i]->SaveSettingsToFile((programPath + "/tempSettings/Setting_" + QString::number(digiSN)).toStdString().c_str());
    }
    digi[i]->CloseDigitizer();
    delete digi[i];

    LogMsg("Closed Digitizer : " + QString::number(digi[i]->GetSerialNumber()));

    if( readDataThread[i] != NULL ){
      LogMsg("Waiting for readData Thread .....");
      readDataThread[i]->Stop();
      readDataThread[i]->quit();
      readDataThread[i]->wait();
      delete readDataThread[i];
    }
  }
  delete [] digi;
  delete [] readDataThread;
  digi = NULL;
  readDataThread = NULL;

  bnSyncHelper->setEnabled(false);
  bnOpenDigitizers->setEnabled(true);
  bnOpenDigitizers->setFocus();
  bnCloseDigitizers->setEnabled(false);
  bnDigiSettings->setEnabled(false);
  bnSOLSettings->setEnabled(false);
  bnStartACQ->setEnabled(false);
  bnStopACQ->setEnabled(false);
  bnComment->setEnabled(false);
  bnOpenScope->setEnabled(false);
  bnOpenScalar->setEnabled(false);
  chkSaveRun->setEnabled(false);
  cbAutoRun->setEnabled(false);
  cbDataFormat->setEnabled(false);

  bnProgramSettings->setEnabled(true);
  bnNewExp->setEnabled(true);

  LogMsg("Closed all digitizers and readData Threads.");

}

void MainWindow::OpenSyncHelper(){
  LogMsg("Open <b>Sync Helper</b>.");

  QDialog dialog(this);
  dialog.setWindowTitle("Sync Helper");
  dialog.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint);

  QVBoxLayout * layout = new QVBoxLayout(&dialog);

  QPushButton * bnNoSync = new QPushButton("No Sync", &dialog);
  QPushButton * bnMethod1 = new QPushButton("Software CLK-OUT --> CLK-IN\n(Master = 1st Digi)", &dialog);

  layout->addWidget( bnNoSync, 1);
  layout->addWidget(bnMethod1, 2);

  bnNoSync->setFixedHeight(40);
  bnMethod1->setFixedHeight(40);

  connect(bnNoSync, &QPushButton::clicked, [&](){
    for(unsigned int i = 0; i < nDigi; i++){
      digi[i]->WriteValue(PHA::DIG::ClockSource, "Internal"); 
      digi[i]->WriteValue(PHA::DIG::StartSource, "SWcmd");
      digi[i]->WriteValue(PHA::DIG::SyncOutMode, "Disabled");
    }

    if( digiSetting ) digiSetting->UpdatePanelFromMemory();

    bnSyncHelper->setText("Sync Helper (No Sync)");

    dialog.accept();
  });

  connect(bnMethod1, &QPushButton::clicked, [&](){
    digi[0]->WriteValue(PHA::DIG::ClockSource, "Internal"); 
    digi[0]->WriteValue(PHA::DIG::EnableClockOutFrontPanel, "True"); 
    digi[0]->WriteValue(PHA::DIG::StartSource, "SWcmd");
    digi[0]->WriteValue(PHA::DIG::SyncOutMode, "Run");

    for(unsigned int i = 1; i < nDigi; i++){
      digi[i]->WriteValue(PHA::DIG::ClockSource, "FPClkIn"); 
      digi[i]->WriteValue(PHA::DIG::EnableClockOutFrontPanel, "True"); 
      digi[i]->WriteValue(PHA::DIG::StartSource, "EncodedClkIn");
      digi[i]->WriteValue(PHA::DIG::SyncOutMode, "SyncIn");
    }

    if( digiSetting ) digiSetting->UpdatePanelFromMemory();
    
    bnSyncHelper->setText("Sync Helper (Software)");

    dialog.accept();
  });


  dialog.exec();

}

//*######################################################################
//*###################################################################### Open Scope
void MainWindow::OpenScope(){
  if( digi ){
    if( !scope ){
      scope = new Scope(digi, nDigi, readDataThread);
      connect(scope, &Scope::CloseWindow, this, [=](){ bnStartACQ->setEnabled(true); });
      //connect(scope, &Scope::UpdateScalar, this, &MainWindow::UpdateScalar);
      connect(scope, &Scope::SendLogMsg, this, &MainWindow::LogMsg);
      connect(scope, &Scope::UpdateOtherPanels, this, [=](){ UpdateAllPanel(0);});
      connect(scope, &Scope::TellACQOnOff, this, [=](const bool onOff){
        if( influx ){
          influx->ClearDataPointsBuffer();
          influx->AddDataPoint(onOff ? "StartStop value=1" : "StartStop value=0");
          influx->WriteData(DatabaseName.toStdString());
        }
        if( onOff){
          lbScalarACQStatus->setText("<font style=\"color: green;\"><b>ACQ On</b></font>");
        }else{
          lbScalarACQStatus->setText("<font style=\"color: red;\"><b>ACQ Off</b></font>");
        }
      });

      if( influx ){
        influx->ClearDataPointsBuffer();
        influx->AddDataPoint("StartStop value=1");
        influx->WriteData(DatabaseName.toStdString());
      }
      if( digiSetting) {
        connect(scope, &Scope::TellSettingsPanelControlOnOff, digiSetting, &DigiSettingsPanel::EnableControl);
        digiSetting->EnableControl();
      }  

      //scope->StartScope();

    }else{
      scope->show();
      if( scope->isVisible() ) scope->activateWindow();
      //scope->StartScope();
      if( digiSetting ) digiSetting->EnableControl();
    }
  }

  bnStartACQ->setEnabled(false);
}

//^###################################################################### Open digitizer setting panel
void MainWindow::OpenDigitizersSettings(){
  LogMsg("Open digitizers Settings Panel");

  if( digiSetting == NULL){
    digiSetting = new DigiSettingsPanel(digi, nDigi, expDataPath + "/Settings/");
    connect(digiSetting, &DigiSettingsPanel::SendLogMsg, this, &MainWindow::LogMsg);
    connect(digiSetting, &DigiSettingsPanel::UpdateOtherPanels, this, [=](){ UpdateAllPanel(1);});

  }else{
    digiSetting->show();
    if( digiSetting->isVisible() ) digiSetting->activateWindow();
  }
  digiSetting->UpdatePanelFromMemory();
}

//^###################################################################### Open SOLARIS setting panel
void MainWindow::OpenSOLARISpanel(){
  LogMsg("Open SOLARIS Panel.");
  solarisSetting->show();
  solarisSetting->UpdatePanelFromMemory();
  if( solarisSetting->isVisible() ) solarisSetting->activateWindow();
}

bool MainWindow::CheckSOLARISpanelOK(){

  QFile file(analysisPath + "/working/Mapping.h");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    LogMsg("Fail to open <b>" + file.fileName() + "</b>. SOLARIS panel disabled.");

    //TODO ----- Create a template of the mapping

    return false;
  }
  LogMsg("Found <b>" + file.fileName() + "</b>. Setting up SOLARIS panel.");
  mapping.clear();
  std::vector<int> singleDigiMap;
  detType.clear();
  detGroupID.clear();
  detMaxID.clear();
  detGroupName.clear();

  bool startRecord = false;
  QTextStream in(&file);
  while (!in.atEnd()) {
    QString line = in.readLine();

    if( line.contains("//^")) continue;
    if( line.contains("// //")) continue;
    if( line.contains("////")) continue;

    if( line.contains("//C=")){ // detType
      int in1 = line.indexOf("{");
      int in2 = line.lastIndexOf("}");
      if( in2 > in1){
        QString subLine = line.mid(in1+1, in2 - in1 -1).trimmed().remove(QRegularExpression("[\"\\\\]"));
        detType = subLine.split(",");
      }else{
        LogMsg("Problem Found for the Mapping.h.");
        return false;
      }
    }
    if( line.contains("//C%")){ // groupName
      int in1 = line.indexOf("{");
      int in2 = line.lastIndexOf("}");
      if( in2 > in1){
        QString subLine = line.mid(in1+1, in2 - in1 -1).trimmed().remove(QRegularExpression("[\"\\\\]"));
        detGroupName = subLine.split(",");
      }else{
        LogMsg("Problem Found for the Mapping.h.");
        return false;
      }
    }
    if( line.contains("//C&")){ //groupID
      int in1 = line.indexOf("{");
      int in2 = line.lastIndexOf("}");
      if( in2 > in1){
        QString subLine = line.mid(in1+1, in2 - in1 -1).trimmed().remove(QRegularExpression("[\"\\\\]"));
        QStringList haha = subLine.split(",");
        for( int i = 0; i < haha.size(); i++) detGroupID.push_back(haha[i].toInt());
      }else{
        LogMsg("Problem Found for the Mapping.h.");
        return false;
    }
      }
    if( line.contains("//C#")){ //detMaxID
      int in1 = line.indexOf("{");
      int in2 = line.lastIndexOf("}");
      if( in2 > in1){
        QString subLine = line.mid(in1+1, in2 - in1 -1).trimmed().remove(QRegularExpression("[\"\\\\]"));
        QStringList haha = subLine.split(",");
        for( int i = 0; i < haha.size(); i++) detMaxID.push_back(haha[i].toInt());
      }else{
        LogMsg("Problem Found for the Mapping.h.");
        return false;
      }
    }
    if( line.contains("//C ") ) {
      startRecord = true;
      singleDigiMap.clear();
      continue;
    }
    if( startRecord && line.contains("//C----")){
      startRecord = false;
      mapping.push_back(singleDigiMap);
      continue;
    }
    if( startRecord ){
      int index = line.lastIndexOf("///");
      if( index != -1 ) {
        line = line.left(index).trimmed();
        if( line.endsWith(",") ){
          line.remove( line.length() -1, 1);
        }
      }
      QStringList list = line.replace(' ', "").split(",");
      for( int i = 0; i < list.size() ; i ++){
        singleDigiMap.push_back(list[i].toInt());
      }
    }
  }
  file.close();

  LogMsg("Mapping.h | Num. Digi : " + QString::number(mapping.size()));
  for( int i = 0 ; i < (int) mapping.size(); i ++){
    if( i < nDigi ){
      LogMsg("      Digi-" + QString::number(i) + " : " + QString::number(mapping[i].size()) + " Ch. | Digi-" 
               +  QString::number(digi[i]->GetSerialNumber()) + " : " 
                + QString::number(digi[i]->GetNChannels()) + " Ch.");
    }else{
      LogMsg("      Digi-" + QString::number(i) + " : " + QString::number(mapping[i].size()) + " Ch. | No Conneted Digitizer" );
    }
  }

  if( (int) detMaxID.size() != detType.size() ){
    LogMsg("Size of detector Name and detctor max ID does not match.");
    return false;
  }

  if( nDigiConnected == 0 ) return false;

  //@============= Create SOLAIRS panel
  solarisSetting = new SOLARISpanel(digi, nDigi, analysisPath, mapping, detType, detGroupName, detGroupID, detMaxID);
  connect(solarisSetting, &SOLARISpanel::SendLogMsg, this, &MainWindow::LogMsg);
  connect(solarisSetting, &SOLARISpanel::UpdateOtherPanels, this, [=](){ UpdateAllPanel(2);});

  if( solarisSetting == nullptr) return false;

  return true;
}

void MainWindow::UpdateAllPanel(int panelID){
  
  printf("%s  %d\n", __func__, panelID);

  switch (panelID) {
    case 0 :{
      if( digiSetting && digiSetting->isVisible()  ) digiSetting->UpdatePanelFromMemory();
      if( solarisSetting && solarisSetting->isVisible() ) solarisSetting->UpdatePanelFromMemory();
    };break;
    case 1 :{
      if( scope && scope->isVisible() ) scope->ReadScopeSettings();
      if( solarisSetting && solarisSetting->isVisible() ) solarisSetting->UpdatePanelFromMemory();
    };break;
    case 2 :{
      if( scope && scope->isVisible() ) scope->ReadScopeSettings();
      if( digiSetting && digiSetting->isVisible() ) digiSetting->UpdatePanelFromMemory();
    }
  }
    
}

//^###################################################################### Open Scaler, when DAQ is running
void MainWindow::OpenScaler(){
  scalar->show();
  if( isACQRunning ) {
    lbScalarACQStatus->setText("<font style=\"color: green;\"><b>ACQ On</b></font>");
  }else{
    lbScalarACQStatus->setText("<font style=\"color: red;\"><b>ACQ Off</b></font>");
  }

  scalarThread->start();

  if( scalar->isVisible() ) scalar->activateWindow();
}

void MainWindow::SetUpScalar(){

  scalar->setGeometry(0, 0, 10 + nDigi * 230, (maxNumChannelAcrossDigitizer + 5) * 26 );

  lbLastUpdateTime = new QLabel("Last update : ", scalar);
  lbLastUpdateTime->setAlignment(Qt::AlignCenter);
  scalarLayout->removeWidget(lbLastUpdateTime);
  scalarLayout->addWidget(lbLastUpdateTime, 0, 1, 1, 1 + nDigi);

  lbScalarACQStatus = new QLabel("ACQ status", scalar);
  lbScalarACQStatus->setAlignment(Qt::AlignCenter);
  scalarLayout->removeWidget(lbScalarACQStatus);
  scalarLayout->addWidget(lbScalarACQStatus, 1, 1, 1, 1 + nDigi);

  // QPushButton * bnUpdateScaler = new QPushButton("Manual Update", scalar);
  // scalarLayout->addWidget(bnUpdateScaler, 2, 1, 1, 1 + nDigi);
  // connect(bnUpdateScaler, &QPushButton::clicked, this, &MainWindow::UpdateScalar);

  ///==== create the 1st row
  int rowID = 5;

  for( int ch = 0; ch < maxNumChannelAcrossDigitizer; ch++){

    if( ch == 0 ){
      QLabel * lbCH_H = new QLabel("Ch", scalar); 
      scalarLayout->addWidget(lbCH_H, rowID, 0);
    }  

    rowID ++;
    QLabel * lbCH = new QLabel(QString::number(ch), scalar);
    lbCH->setAlignment(Qt::AlignCenter);
    scalarLayout->addWidget(lbCH, rowID, 0);
  }
  
  ///===== create the trigger and accept
  leTrigger = new QLineEdit**[nDigi];
  leAccept = new QLineEdit**[nDigi];
  lbFileSize = new QLabel *[nDigi];
  for( int iDigi = 0; iDigi < nDigi; iDigi++){
    rowID = 3;
    lbFileSize[iDigi] = new QLabel("file Size", scalar);
    lbFileSize[iDigi]->setAlignment(Qt::AlignCenter);
    leTrigger[iDigi] = new QLineEdit *[digi[iDigi]->GetNChannels()];
    leAccept[iDigi] = new QLineEdit *[digi[iDigi]->GetNChannels()];
    for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch++){

      if( ch == 0 ){
          QLabel * lbDigi = new QLabel("Digi-" + QString::number(digi[iDigi]->GetSerialNumber()), scalar); 
          lbDigi->setAlignment(Qt::AlignCenter);
          scalarLayout->addWidget(lbDigi, rowID, 2*iDigi+1, 1, 2);
          rowID ++;

          scalarLayout->addWidget(lbFileSize[iDigi], rowID, 2*iDigi+1, 1, 2);
          rowID ++;

          QLabel * lbA = new QLabel("Input [Hz]", scalar);
          lbA->setAlignment(Qt::AlignCenter);
          scalarLayout->addWidget(lbA, rowID, 2*iDigi+1);
          QLabel * lbB = new QLabel("Trig. [Hz]", scalar);
          lbB->setAlignment(Qt::AlignCenter);
          scalarLayout->addWidget(lbB, rowID, 2*iDigi+2);
      }
    
      rowID ++;
      
      leTrigger[iDigi][ch] = new QLineEdit(scalar);
      leTrigger[iDigi][ch]->setReadOnly(true);
      leTrigger[iDigi][ch]->setAlignment(Qt::AlignRight);
      scalarLayout->addWidget(leTrigger[iDigi][ch], rowID, 2*iDigi+1);

      leAccept[iDigi][ch] = new QLineEdit(scalar);
      leAccept[iDigi][ch]->setReadOnly(true);
      leAccept[iDigi][ch]->setAlignment(Qt::AlignRight);
      leAccept[iDigi][ch]->setStyleSheet("background-color: #F0F0F0;");
      scalarLayout->addWidget(leAccept[iDigi][ch], rowID, 2*iDigi+2);
    }
  }

}


void MainWindow::CleanUpScalar(){

  if( leTrigger == nullptr ) return;

  for( int i = 0; i < nDigi; i++){
    for( int ch = 0; ch < digi[i]->GetNChannels(); ch ++){
      delete leTrigger[i][ch];
      delete leAccept[i][ch];
    }
    delete lbFileSize[i];
    delete [] leTrigger[i];
    delete [] leAccept[i];
  }
  delete [] lbFileSize;
  delete [] leTrigger;
  delete [] leAccept;
  lbFileSize = nullptr;
  leTrigger = nullptr;
  leAccept = nullptr;

  //Clean up QLabel
  QList<QLabel *> labelChildren = scalar->findChildren<QLabel *>();
  for( int i = 0; i < labelChildren.size(); i++) delete labelChildren[i];

}

void MainWindow::UpdateScalar(){
  if( !digi ) return;
  if( scalar == NULL ) return;
  if( scalar->isVisible() == false ) return;

  lbLastUpdateTime->setText("Last update: " + QDateTime::currentDateTime().toString("MM.dd hh:mm:ss"));

  if( influx && scalarOutputInflux) influx->ClearDataPointsBuffer();
  std::string haha[MaxNumberOfChannel] = {""};
  double acceptRate[MaxNumberOfChannel] = {0};

  ///===== Get trigger for all channel
  unsigned long totalFileSize  = 0;
  for( int iDigi = 0; iDigi < nDigi; iDigi ++ ){
    if( digi[iDigi]->IsDummy() ) return;

    //=========== another method, directly readValue
    for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch ++){
      // digiMTX[iDigi].lock();
      std::string timeStr = digi[iDigi]->ReadValue(PHA::CH::ChannelRealtime, ch); // for refreashing SelfTrgRate and SavedCount
      haha[ch] = digi[iDigi]->ReadValue(PHA::CH::SelfTrgRate, ch);
      std::string kakaStr = digi[iDigi]->ReadValue(PHA::CH::ChannelSavedCount, ch);
      // digiMTX[iDigi].unlock();
      
      unsigned long kaka = std::stoul(kakaStr.c_str()) ;
      unsigned long time = std::stoul(timeStr.c_str()) ;
      ///* it seems that the ChannelRealtime is not in ns for VX2730
      if( digi[iDigi]->GetModelName() == "VX2730" ){ time = time / 4;}

      leTrigger[iDigi][ch]->setText(QString::fromStdString(haha[ch]));
      
      if( oldTimeStamp[iDigi][ch] >  0 && time - oldTimeStamp[iDigi][ch] > 1e9 && kaka > oldSavedCount[iDigi][ch]){
        acceptRate[ch] = (kaka - oldSavedCount[iDigi][ch]) * 1e9 *1.0 / (time - oldTimeStamp[iDigi][ch]);
      }else{
        acceptRate[ch] = 0;
      }

      //if( acceptRate[ch] > 10000 ) printf("%d-%2d | old (%lu, %lu), new (%lu, %lu)\n", iDigi, ch, oldTimeStamp[iDigi][ch], oldSavedCount[iDigi][ch], time, kaka);
      // if( ch == 3){
      //   printf("time: %lu (%lu) = %12.10f, Channel Saved Count %lu (%lu) = %lu | accepted Rate %f\n", 
      //     time, oldTimeStamp[iDigi][ch], (time - oldTimeStamp[iDigi][ch])/1e9, 
      //     kaka, oldSavedCount[iDigi][ch], (kaka - oldSavedCount[iDigi][ch]),
      //     acceptRate[ch]);
      // }

      oldSavedCount[iDigi][ch] = kaka;
      oldTimeStamp[iDigi][ch] = time; 
      //if( kaka != "0" )  printf("%s, %s | %.2f\n", time.c_str(), kaka.c_str(), acceptRate);
      leAccept[iDigi][ch]->setText(QString::number(acceptRate[ch],'f', 1));

      lbFileSize[iDigi]->setText(QString::number(digi[iDigi]->GetTotalFilesSize()/1024./1024.) + " MB");

    }

    ///============== push the trigger, acceptRate rate database
    if( influx && scalarOutputInflux ){
      for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch++ ){
        influx->AddDataPoint("Rate,Bd=" + std::to_string(digi[iDigi]->GetSerialNumber()) + ",Ch=" + QString::number(ch).rightJustified(2, '0').toStdString() + " value=" + haha[ch]);
        if( !std::isnan(acceptRate[ch]) )  influx->AddDataPoint("AccpRate,Bd=" + std::to_string(digi[iDigi]->GetSerialNumber()) + ",Ch=" + QString::number(ch).rightJustified(2, '0').toStdString() + " value=" + std::to_string(acceptRate[ch]));
      }
    }
    totalFileSize +=  digi[iDigi]->GetTotalFilesSize();
  }

  if( influx && influx->GetDataLength() > 0 && scalarOutputInflux ){
    if( chkSaveRun->isChecked() ) influx->AddDataPoint("FileSize value=" + std::to_string(totalFileSize));
    //influx->PrintDataPoints();
    influx->WriteData(DatabaseName.toStdString());
    influx->ClearDataPointsBuffer();
  }

  //TODO record ADC temperature, and status. In this case, the digiSetting is only UpdateFromMemory, manually looping digitizers and get the status.
  if( digiSetting && digiSetting->isVisible() ) digiSetting->UpdateStatus();

  if( solarisSetting && solarisSetting->isVisible() ) solarisSetting->UpdateThreshold();

}

//*######################################################################
//*###################################################################### Program Settings
void MainWindow::ProgramSettingsPanel(){

  LogMsg("Open <b>Program Settings</b>.");

  QDialog dialog(this);
  dialog.setWindowTitle("Program Settings");
  dialog.setGeometry(0, 0, 700, 800);
  dialog.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint);

  QGridLayout * layout = new QGridLayout(&dialog);
  layout->setVerticalSpacing(5);

  unsigned int rowID = 0;

  //-------- Instruction
  QPlainTextEdit * helpInfo = new QPlainTextEdit(&dialog);
  helpInfo->setReadOnly(true);
  helpInfo->setStyleSheet("background-color: #F3F3F3;");
  helpInfo->setLineWrapMode(QPlainTextEdit::LineWrapMode::WidgetWidth);
  
  
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Analysis Path  </font> is the path of \
                           the folder of the analysis code. Can be omitted.");

  helpInfo->appendHtml("<p></p>");
 
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Analysis Path  </font> is the path of \
                           the folder of the analysis code. Can be omitted.");

  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Data Path  </font> is the path of the \
                             <b>parents folder</b> of data will store. ");  
  helpInfo->appendHtml("<font style=\"color : blue;\">  Exp Name  </font> is the name of the experiment and <b>Elog Folder</b>. \
                         This set the exp. folder under the <font style=\"color : blue;\">  Data Path  </font>.\
                        The experiment data will be saved under this folder. e.g. <font style=\"color : blue;\">Data Path/Exp Name</font>.");
  helpInfo->appendHtml("For User links to Analysis folder and use the New/Change/Reload/Exp button, the Exp Name will be overwriten.");

  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Save runs in sub-folders </font> means \
                            saving each run in indivuial subfolder. e.g. <font style=\"color : blue;\">Data Path/Exp Name/runXXX</font>.");


  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Digitizers IP List </font> is the list of IP \
                           digi of the digitizers IP. Break by \",\", continue by \"-\". e.g. 192.168.0.100,102  for 2 digitizers, or 192.168.0.100-102 for 3 digitizers.");
  
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Analysis Path  </font> is the path of \
                           the folder of the analysis code. Can be omitted.");
  helpInfo->appendHtml("<font style=\"color : blue;\">  Database IP </font> or <font style=\"color : blue;\">  Elog IP </font> can be empty. In that case, no database and elog will be used.");

  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml(" * items can be ommitted");

  layout->addWidget(helpInfo, rowID, 0, 1, 4);

  //-------- Program Setting Path
  // rowID ++;
  // QLabel *lbSaveSettingPath = new QLabel("Settings Save Path", &dialog);
  // lbSaveSettingPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  // layout->addWidget(lbSaveSettingPath, rowID, 0);
  // lSaveSettingPath = new QLineEdit(programSettingsPath, &dialog); layout->addWidget(lSaveSettingPath, rowID, 1, 1, 2);

  // QPushButton * bnSaveSettingPath = new QPushButton("browser", &dialog); layout->addWidget(bnSaveSettingPath, rowID, 3);
  // connect(bnSaveSettingPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(0);});

  //-------- data Path
  rowID ++;
  QLabel *lbDataPath = new QLabel("Data Path", &dialog); 
  lbDataPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDataPath, rowID, 0);
  lExpDataPath = new QLineEdit(masterExpDataPath, &dialog); layout->addWidget(lExpDataPath, rowID, 1, 1, 2);

  QPushButton * bnDataPath = new QPushButton("browser", &dialog); layout->addWidget(bnDataPath, rowID, 3);
  connect(bnDataPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(2);});

  //-------- Is Save single folder
  rowID ++;
  chkSaveSubFolder = new QCheckBox("Save runs in sub-folders", this); 
  chkSaveSubFolder->setChecked(isSaveSubFolder);
  layout->addWidget(chkSaveSubFolder, rowID, 1);

  //-------- Exp Name Temp
  rowID ++;
  QLabel *lbExpNameTemp = new QLabel("Exp Name", &dialog); 
  lbExpNameTemp->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbExpNameTemp, rowID, 0);
  lExpName = new QLineEdit(expName, &dialog); layout->addWidget(lExpName, rowID, 1, 1, 2);

  //-------- Digitizer IP
  rowID ++;
  QLabel *lbIPDomain = new QLabel("Digitizers IP List", &dialog); 
  lbIPDomain->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbIPDomain, rowID, 0);
  lIPDomain = new QLineEdit(IPListStr, &dialog); layout->addWidget(lIPDomain, rowID, 1, 1, 2);

  //------- add a separator
  rowID ++;
  QFrame * line = new QFrame;
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  layout->addWidget(line, rowID, 0, 1, 4);

  //-------- analysis Path
  rowID ++;
  QLabel *lbAnalysisPath = new QLabel("Analysis Path *", &dialog);
  lbAnalysisPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbAnalysisPath, rowID, 0);
  lAnalysisPath = new QLineEdit(analysisPath, &dialog); layout->addWidget(lAnalysisPath, rowID, 1, 1, 2);

  QPushButton * bnAnalysisPath = new QPushButton("browser", &dialog); layout->addWidget(bnAnalysisPath, rowID, 3);
  connect(bnAnalysisPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(1);});

  //-------- DataBase IP
  rowID ++;
  QLabel *lbDatbaseIP = new QLabel("Database IP *", &dialog); 
  lbDatbaseIP->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDatbaseIP, rowID, 0);
  lDatbaseIP = new QLineEdit(DatabaseIP, &dialog); layout->addWidget(lDatbaseIP, rowID, 1, 1, 2);
  //-------- DataBase name
  rowID ++;
  QLabel *lbDatbaseName = new QLabel("Database Name *", &dialog);
  lbDatbaseName->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDatbaseName, rowID, 0);
  lDatbaseName = new QLineEdit(DatabaseName, &dialog); layout->addWidget(lDatbaseName, rowID, 1, 1, 2);
  //-------- DataBase Token
  rowID ++;
  QLabel *lbDatbaseToken = new QLabel("Database Token *", &dialog);
  lbDatbaseToken->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDatbaseToken, rowID, 0);
  lDatbaseToken = new QLineEdit(DatabaseToken, &dialog); layout->addWidget(lDatbaseToken, rowID, 1, 1, 2);

  //-------- Elog IP
  rowID ++;
  QLabel *lbElogIP = new QLabel("Elog IP *", &dialog);
  lbElogIP->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbElogIP, rowID, 0);
  lElogIP = new QLineEdit(ElogIP, &dialog); layout->addWidget(lElogIP, rowID, 1, 1, 2);

  //-------- Elog User
  rowID ++;
  QLabel *lbElogUser = new QLabel("Elog User *", &dialog);
  lbElogUser->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbElogUser, rowID, 0);
  lElogUser = new QLineEdit(ElogUser, &dialog); layout->addWidget(lElogUser, rowID, 1, 1, 2);

  //-------- Elog User
  rowID ++;
  QLabel *lbElogPWD = new QLabel("Elog Password *", &dialog);
  lbElogPWD->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbElogPWD, rowID, 0);
  lElogPWD = new QLineEdit(ElogPWD, &dialog); layout->addWidget(lElogPWD, rowID, 1, 1, 2);

  rowID ++;
  QPushButton *button1 = new QPushButton("OK and Save", &dialog);
  layout->addWidget(button1, rowID, 1);
  QObject::connect(button1, &QPushButton::clicked, this, [=](){
    
    IPListStr = lIPDomain->text();
    DatabaseIP = lDatbaseIP->text();
    DatabaseName = lDatbaseName->text();
    DatabaseToken = lDatbaseToken->text();
    analysisPath = lAnalysisPath->text();
    masterExpDataPath = lExpDataPath->text();
    expName = lExpName->text();
    ElogIP = lElogIP->text();
    ElogUser = lElogUser->text();
    ElogPWD = lElogPWD->text();

    SaveProgramSettings();

    bnProgramSettings->setStyleSheet("");
    bnNewExp->setEnabled(true);

    if( !IPListStr.isEmpty() ){
      DecodeIPList();
      bnOpenDigitizers->setEnabled(true);
    }else{
      bnProgramSettings->setStyleSheet("color: red;");
      LogMsg("<font style=\"color : red;\">Digitizer IP list is empty.</font>");
    }

    SetupInflux();
    CheckElog();

    expDataPath = masterExpDataPath + "/" + expName;
    rawDataPath = expDataPath + "/data_raw/";
    rootDataPath = expDataPath + "/root_data/";    
    QString pcName = qEnvironmentVariable("PCName");
    if( pcName == "solaris-daq" ){
      LogMsg("This is SOLARIS DAQ....");
      rawDataPath = expDataPath + "/";
      LogMsg("Raw Data Path : " + rawDataPath);
      rootDataPath = "/mnt/data1/" + expName + "/";
      LogMsg("Root Data Path : " + rootDataPath);
    }
    leExpName->setText(expName);
    leRawDataPath->setText(rawDataPath);
    
    CreateRawDataFolder();

    LoadExpNameSh();

    if(analysisPath.isEmpty()) bnNewExp->setEnabled(false);

  });
  QObject::connect(button1, &QPushButton::clicked, &dialog, &QDialog::accept);
  
  QPushButton *button2 = new QPushButton("Cancel", &dialog);
  layout->addWidget(button2, rowID, 2);
  QObject::connect(button2, &QPushButton::clicked, this, [=](){this->LogMsg("Cancel <b>Program Settings</b>");});
  QObject::connect(button2, &QPushButton::clicked, &dialog, &QDialog::reject);

  layout->setColumnStretch(0, 2);
  layout->setColumnStretch(1, 2);
  layout->setColumnStretch(2, 2);
  layout->setColumnStretch(3, 1);

  button1->setFocus();

  dialog.exec();
}

bool MainWindow::LoadProgramSettings(){

  QString settingFile = QDir::current().absolutePath() + "/programSettings.txt";

  LogMsg("Loading <b>" + settingFile + "</b> for Program Settings.");

  QFile file(settingFile);

  bool ret = false;

  //initialized
  masterExpDataPath = "";
  isSaveSubFolder = false;
  expName = "";
  IPListStr = "";
  analysisPath = "";
  DatabaseIP = "";
  DatabaseName = "";
  DatabaseToken = "";
  ElogIP = "";
  ElogUser = "";
  ElogPWD = "";

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
        case  0 : masterExpDataPath     = line; break;
        case  1 : isSaveSubFolder = (line == "SubFolder" ?  true : false); break;
        case  2 : expName         = line; break;
        case  3 : IPListStr       = line; break;
        case  4 : analysisPath    = line; break;
        case  5 : DatabaseIP      = line; break;
        case  6 : DatabaseName    = line; break;
        case  7 : DatabaseToken   = line; break;
        case  8 : ElogIP          = line; break;
        case  9 : ElogUser        = line; break;
        case 10 : ElogPWD         = line; break;
      }

      count ++;
      line = in.readLine();
      // printf("%d | %s \n", count, line.toStdString().c_str());
    }

    if( count >= 3 ) {
      logMsgHTMLMode = false;
      // LogMsg("Setting File Path : " + programSettingsPath);
      LogMsg("          Analysis Path : " + analysisPath);
      LogMsg("            Database IP : " + DatabaseIP);
      LogMsg("          Database Name : " + DatabaseName);
      LogMsg("         Database Token : " + maskText(DatabaseToken));
      LogMsg("                 ElogIP : " + ElogIP);
      LogMsg("              Elog User : " + ElogUser);
      LogMsg("          Elog Password : " + maskText(ElogPWD));
      LogMsg("          Exp Data Path : " + masterExpDataPath);
      LogMsg("Save Runs in SubFolders : " +  QString(isSaveSubFolder ? "Yes" : "No") );
      LogMsg("  Exp. Name (Elog Name) : " + expName);
      LogMsg("          Digi. IP List : " + IPListStr);
      logMsgHTMLMode = true;

      expDataPath = masterExpDataPath + "/" + expName;
      rawDataPath = expDataPath + "/data_raw/";
      rootDataPath = expDataPath + "/root_data/";

      QString pcName = qEnvironmentVariable("PCName");
      if( pcName == "solaris-daq" ){
        LogMsg("This is SOLARIS DAQ....");
        rawDataPath = expDataPath + "/";
        LogMsg("Raw Data Path : " + rawDataPath);
        rootDataPath = "/mnt/data1/" + expName + "/";
        LogMsg("Root Data Path : " + rootDataPath);
      }

      leExpName->setText(expName);
      
      ret = true;
    }else{
      LogMsg("Settings are not complete.");
      LogMsg("Please Open the <font style=\"color : red;\">Program Settings </font>");
    }
  }

  if( ret ){

    //CHeck data path exist
    QDir dir(rawDataPath);

    if (!dir.exists()) {
      LogMsg("<font style=\"color : red;\">Raw data path " +  rawDataPath + " does not exist.</font>");
      bnProgramSettings->setStyleSheet("color: red;");
      bnOpenDigitizers->setEnabled(false);
      bnNewExp->setEnabled(false);
      // return false;
    }else{
      QFileInfo dirInfo(dir.absolutePath());
      if( !dirInfo.isWritable() ){
        LogMsg("<font style=\"color : red;\">Raw data path " +  rawDataPath + " is not writable.</font>");
        bnProgramSettings->setStyleSheet("color: red;");
        bnOpenDigitizers->setEnabled(false);
        bnNewExp->setEnabled(false);
        // return false;
      }else{
        leRawDataPath->setText(rawDataPath);
        leExpName->setText(expName);
      }
    }

    if( !IPListStr.isEmpty() && dir.exists() ){
      bnOpenDigitizers->setEnabled(true);
      bnOpenDigitizers->setStyleSheet("color:red;");
      DecodeIPList();
      SetupInflux();
      CheckElog();
    }else{
      LogMsg("<font style=\"color : red;\">Digitizer IP list is empty.</font>");
      bnProgramSettings->setStyleSheet("color: red;");
      bnOpenDigitizers->setEnabled(false);
      return false;
    }

    if(analysisPath.isEmpty()) {
      LogMsg("Analysis Path is empty.");
      bnNewExp->setEnabled(false);
      // return false;
    }

    return true;

  }else{

    bnProgramSettings->setStyleSheet("color: red;");
    bnNewExp->setEnabled(false);
    bnOpenDigitizers->setEnabled(false);
    return false;
  }
}

void MainWindow::SaveProgramSettings(){

  if( masterExpDataPath.isEmpty() ){
    LogMsg("<font style=\"color : red;\">Exp Data Path is empty.</font>");
    return;
  }

  QFile file(programPath + "/programSettings.txt");
  
  file.open(QIODevice::Text | QIODevice::WriteOnly);

  // file.write((programSettingsPath+"\n").toStdString().c_str());
  file.write((masterExpDataPath+"\n").toStdString().c_str());
  file.write( chkSaveSubFolder->isChecked()  ? "SubFolder\n" : "SingleFolder\n" );
  file.write((expName+"\n").toStdString().c_str());
  file.write((IPListStr+"\n").toStdString().c_str());
  file.write((analysisPath+"\n").toStdString().c_str());
  file.write((DatabaseIP+"\n").toStdString().c_str());
  file.write((DatabaseName+"\n").toStdString().c_str());
  file.write((DatabaseToken+"\n").toStdString().c_str());
  file.write((ElogIP+"\n").toStdString().c_str());
  file.write((ElogUser+"\n").toStdString().c_str());
  file.write((ElogPWD+"\n").toStdString().c_str());
  file.write("//------------end of file.");
  
  file.close();
  LogMsg("Saved program settings to <b>"+programPath + "/programSettings.txt<b>.");

}

void MainWindow::OpenDirectory(int id){ 
  QFileDialog fileDialog(this);
  fileDialog.setFileMode(QFileDialog::Directory);
  fileDialog.exec();

  //qDebug() << fileDialog.selectedFiles();
  
  switch (id){
    // case 0 : lSaveSettingPath->setText(fileDialog.selectedFiles().at(0)); break;
    case 1 : lAnalysisPath->setText(fileDialog.selectedFiles().at(0)); break;
    case 2 : lExpDataPath->setText(fileDialog.selectedFiles().at(0)); break;
    // case 3 : lRootDataPath->setText(fileDialog.selectedFiles().at(0)); break;
  }
}

void MainWindow::DecodeIPList(){
  //------- decode IPListStr
  nDigi = 0;
  IPList.clear();
  QStringList parts = IPListStr.replace(' ', "").split(".");
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

//*######################################################################
//*###################################################################### Setup new exp

void MainWindow::SetupNewExpPanel(){

  LogMsg("Open <b>New/Change/Reload Exp</b>.");

  QDialog dialog(this);
  dialog.setWindowTitle("Setup / change Experiment");
  dialog.setGeometry(0, 0, 500, 500);
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
  instr->appendHtml("<b>1,</b> Add a new local branch in <font style=\"color:blue;\">Analysis Path</font>. User need to manually push to remote.");
  instr->appendHtml("<b>2,</b> Create folder in <font style=\"color:blue;\">Data Path</font> and <font style=\"color:blue;\">Root Data Path</font>");
  instr->appendHtml("<b>3,</b> Create Symbolic links in <font style=\"color:blue;\">Analysis Path</font>");
  instr->appendHtml("<p></p>");
  instr->appendHtml("If <font style=\"color:blue;\">Use Git</font> is <b>checked</b>, \
                    the repository <b>MUST</b> be clean. \
                    It will then create a new branch with the <font style=\"color:blue;\">New Exp Name </font> \
                    or change to pre-exist branch.");

  //------- Analysis Path
  rowID ++;
  QLabel * l1 = new QLabel("Analysis Path ", &dialog);
  l1->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(l1, rowID, 0);

  QLineEdit * le1 = new QLineEdit(analysisPath, &dialog);
  le1->setReadOnly(true);
  layout->addWidget(le1, rowID, 1, 1, 3);

  // //------- Exp Data Path
  rowID ++;
  QLabel * l2 = new QLabel("Data Path ", &dialog);
  l2->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(l2, rowID, 0);

  QLineEdit * le2 = new QLineEdit(masterExpDataPath, &dialog);
  le2->setReadOnly(true);
  layout->addWidget(le2, rowID, 1, 1, 3);

  // //------- Root Data Path
  // rowID ++;
  // QLabel * l2a = new QLabel("Root Data Path ", &dialog);
  // l2a->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  // layout->addWidget(l2a, rowID, 0);

  // QLineEdit * le2a = new QLineEdit(rootDataPath, &dialog);
  // le2a->setReadOnly(true);
  // layout->addWidget(le2a, rowID, 1, 1, 3);

  //------- get harddisk space;
  rowID ++;
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
  git.start("git", QStringList() << "fetch");
  git.waitForFinished();
  git.start("git", QStringList() << "branch" << "-a");
  git.waitForFinished();

  QByteArray output = git.readAllStandardOutput();
  existGitBranches = (QString::fromLocal8Bit(output)).split("\n");
  existGitBranches.removeAll("");

  //qDebug() << branches;

  if( existGitBranches.size() == 0) {
    isGitExist = false;
  }else{
    isGitExist = true;
  }

  QString presentBranch;
  unsigned short bID = 0; // id of the present branch
  for( unsigned short i = 0; i < existGitBranches.size(); i++){
    if( existGitBranches[i].indexOf("*") != -1 ){
      presentBranch = existGitBranches[i].remove("*").remove(" ");
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
  bnChangeBranch->setAutoDefault(false);

  connect(bnChangeBranch, &QPushButton::clicked, this, [=](){ this->ChangeExperiment(cb->currentText());  });
  connect(bnChangeBranch, &QPushButton::clicked, &dialog, &QDialog::accept);

  if( isGitExist == false ){
    cb->setEnabled(false);
    bnChangeBranch->setEnabled(false);
  }else{
    for( int i = 0; i < existGitBranches.size(); i++){
      if( i == bID ) continue;
      if( existGitBranches[i].contains("HEAD")) continue;
      if( existGitBranches[i].contains(presentBranch)) continue;
      cb->addItem(existGitBranches[i].remove(" "));
    }
    if ( cb->count() == 0) {
      cb->setEnabled(false);
      cb->addItem("no other branch");
      bnChangeBranch->setEnabled(false);
    }
  }

  connect(cbUseGit, &QCheckBox::clicked, this, [=](){
    if( cb->count() > 1 ) {
      cb->setEnabled(cbUseGit->isChecked());
      bnChangeBranch->setEnabled(cbUseGit->isChecked());
    }
  });
  
  //------- type existing or new experiment
  rowID ++;
  QLabel * lNewExp = new QLabel("New Exp Name ", &dialog);
  lNewExp->setAlignment(Qt::AlignCenter | Qt::AlignRight);
  layout->addWidget(lNewExp, rowID, 0);

  QLineEdit * newExp = new QLineEdit("type and Enter", &dialog);
  layout->addWidget(newExp, rowID, 1, 1, 2);
  //newExp->setFocus();

  QPushButton *bnCreateNewExp = new QPushButton("Create", &dialog);
  layout->addWidget(bnCreateNewExp, rowID, 3);
  bnCreateNewExp->setEnabled(false);
  bnCreateNewExp->setAutoDefault(false);
  
  connect(newExp, &QLineEdit::textChanged, this, [=](){
    newExp->setStyleSheet("color : blue;"); 
    if( newExp->text() == "") bnCreateNewExp->setEnabled(false);
  });

  connect(newExp, &QLineEdit::returnPressed, this, [=](){ 
    if( newExp->text() != "") { 
      newExp->setStyleSheet(""); 
      bnCreateNewExp->setEnabled(true);
    }

  });
  
  connect(bnCreateNewExp, &QPushButton::clicked, this, [=](){ this->CreateNewExperiment(newExp->text());});
  connect(bnCreateNewExp, &QPushButton::clicked, &dialog, &QDialog::accept);

  //----- diable all possible actions
  if( isGitExist){
    if ( !isCleanGit ){
      cbUseGit->setEnabled(false);
      cb->setEnabled(false);
      bnChangeBranch->setEnabled(false);
      newExp->setEnabled(false);
      bnCreateNewExp->setEnabled(false);
    }
  }
  //--------- cancel
  rowID ++;
  QPushButton *bnCancel = new QPushButton("Cancel/Exit", &dialog);
  bnCancel->setAutoDefault(false);
  layout->addWidget(bnCancel, rowID, 0, 1, 4);
  connect(bnCancel, &QPushButton::clicked, this, [=](){this->LogMsg("Cancel <b>New/Change/Reload Exp</b>");});
  connect(bnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);

  layout->setRowStretch(0, 1);
  for( int i = 1; i < rowID; i++) layout->setRowStretch(i, 2);

  LoadExpNameSh();

  dialog.exec();

}
 
bool MainWindow::LoadExpNameSh(){
  //this method set the analysis setting ann symbloic link to raw data
  //ONLY load file, not check the git

  if( rawDataPath == "") return false;

  QString settingFile = rawDataPath + "/expName.sh";

  LogMsg("Loading <b>" + settingFile + "</b> for Experiment.");

  QFile file(settingFile);
  if( !file.open(QIODevice::Text | QIODevice::ReadOnly) ) {
    LogMsg("<b>" + settingFile + "</b> not found. Create one.");
    // LogMsg("Please Open the <font style=\"color : red;\">New/Change/Reload Exp</font>");
    runID = -1;
    elogID = 0;
    //bnOpenDigitizers->setEnabled(false);
    //leExpName->setText("no expName found.");

    WriteExpNameSh();
    
    // return false;
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
      // case 1 : masterExpDataPath = haha; break;
      case 1 : runID = haha.toInt(); break;
      case 2 : elogID = haha.toInt(); break;
    }

    count ++;
    line = in.readLine();
  }

  // rawDataPath = masterExpDataPath + "/" + expName + "/data_raw/";
  // rootDataPath = masterExpDataPath + "/" + expName + "/root_data/";

  // leRawDataPath->setText(rawDataPath);
  leExpName->setText(expName);
  leRunID->setText(QString::number(runID));

  return true;

}

void MainWindow::WriteExpNameSh(){

  QDir dir(rawDataPath);
  if( !dir.exists() ) dir.mkpath(".");

  //----- create the expName.sh
  QFile file2(rawDataPath + "/expName.sh");
  
  file2.open(QIODevice::Text | QIODevice::WriteOnly);
  file2.write(("expName="+ expName + "\n").toStdString().c_str());
  // file2.write(("ExpDataPath="+ masterExpDataPath + "\n").toStdString().c_str());
  file2.write(("runID="+std::to_string(runID)+"\n").c_str());
  file2.write(("elogID="+std::to_string(elogID)+"\n").c_str());
  file2.write("#------------end of file.");
  file2.close();
  LogMsg("Saved expName.sh to <b>"+ rawDataPath + "/expName.sh<b>.");

}

void MainWindow::CreateNewExperiment(const QString newExpName){
  
  if( newExpName == "HEAD" || newExpName == "head") {
    LogMsg("Cannot name new exp as HEAD or head");
    return; 
  }

  if( newExpName == expName ){
    LogMsg("Already at this branch.");
    return; 
  }

  //Check if newExpName already exist, if exist, go to run ChanegExperiment()
  for( int i = 0; i < existGitBranches.size(); i++){
    if( existGitBranches[i].contains("HEAD")) continue;

    if( existGitBranches[i] == newExpName ) {
      ChangeExperiment(newExpName);
      return;
    }
  }


  LogMsg("======================================");
  LogMsg("Creating new Exp. : <font style=\"color: red;\">" + newExpName + "</font>");

  expName = newExpName;
  runID = -1;
  elogID = 0;

  expDataPath = masterExpDataPath + "/" + expName;
  rawDataPath = expDataPath + "/data_raw/";
  rootDataPath = expDataPath + "/root_data/";

  QString pcName = qEnvironmentVariable("PCName");
  if( pcName == "solaris-daq" ){
    LogMsg("This is SOLARIS DAQ....");
    rawDataPath = expDataPath + "/";
    LogMsg("Raw Data Path : " + rawDataPath);
    rootDataPath = "/mnt/data1/" + expName + "/";
    LogMsg("Root Data Path : " + rootDataPath);
  }

  CreateRawDataFolder();
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

  //check if .gitignore exist
  QFile gitIgnore(analysisPath + "/.gitignore");
  if( !gitIgnore.exists() ) {
    if( gitIgnore.open(QIODevice::Text | QIODevice::WriteOnly) ){
      gitIgnore.write("data_raw\n");
      gitIgnore.write("root_data\n");
      gitIgnore.write("*.root\n");
      gitIgnore.write("*.d\n");
      gitIgnore.write("*.so\n");
      gitIgnore.close();
    }
  }

  //----- create git branch
  if( useGit ){
    QProcess git;
    git.setWorkingDirectory(analysisPath);
    git.start("git", QStringList() << "add" << "-A");
    git.waitForFinished();
    
    git.start("git", QStringList() << "commit" << "--allow-empty" << "-m" << "initial commit.");
    git.waitForFinished();

    LogMsg("Commit branch : <b>" + expName + "</b> as \"initial commit\"");

    //check if remote exist, if exist, push to remote
    git.start("git", QStringList() << "remote" );
    git.waitForFinished();

    QString haha = QString::fromLocal8Bit(git.readAllStandardOutput());

    if( haha != ""){
      git.start("git", QStringList() << "push" << "--set-upstream" << haha.remove('\n') << expName);
      git.waitForFinished();

      qDebug() << QString::fromLocal8Bit(git.readAllStandardOutput());
      LogMsg("Pushed new branch : <b>" + expName + "</b> to remote repositiory.");
    }

  }

  //TODO is there anyway to create a new elog ?? direct edit the config.cfg??
  //CheckElog();
  logMsgHTMLMode = true;
  LogMsg("<font style=\"color red;\"> !!!! Please Create a new Elog with name <b>" + newExpName + "</b>. </font>");

  // expDataPath = masterExpDataPath + "/" + newExpName;
  // rawDataPath = expDataPath + "/data_raw/"; 
  // rootDataPath = expDataPath + "/root_data/"; 

  // CreateRawDataFolder();
  CreateDataSymbolicLink();

  leRawDataPath->setText(rawDataPath);
  leExpName->setText(expName);
  leRunID->setText(QString::number(runID));

  SaveProgramSettings();

  bnOpenDigitizers->setEnabled(true);
  bnOpenDigitizers->setStyleSheet("color:red;");

  if( influx ){
    influx->ClearDataPointsBuffer();
    startComment = "New experiment [" + expName + "] was created.";
    influx->AddDataPoint("RunID,start=0 value=" + std::to_string(runID) + ",expName=\"" + expName.toStdString() + "\",comment=\"" + startComment.replace(' ', '_').toStdString() + "\"");
    influx->WriteData(DatabaseName.toStdString());
  }

}

void MainWindow::ChangeExperiment(const QString newExpName){

  expName = newExpName;
  if( newExpName.contains("remotes")){
    QStringList haha = newExpName.split('/');
    expName = haha.last();
  }

  //@---- git must exist.
  QProcess git;
  git.setWorkingDirectory(analysisPath);
  git.start("git", QStringList() << "checkout" << expName);
  git.waitForFinished();


  LogMsg("=============================================");
  LogMsg("Swicted to branch : <b>" + expName + "</b>");

  expDataPath = masterExpDataPath + "/" + newExpName;
  rawDataPath = expDataPath + "/data_raw/"; 
  rootDataPath = expDataPath + "/root_data/"; 

  CreateRawDataFolder();
  CreateDataSymbolicLink();
  SaveProgramSettings();
  LoadExpNameSh();

  if( influx ){
    influx->ClearDataPointsBuffer();
    startComment = "Switched to experiment [" + expName + "].";
    influx->AddDataPoint("RunID,start=0 value=" + std::to_string(runID) + ",expName=\"" + expName.toStdString() + "\",comment=\"" + startComment.replace(' ', '_').toStdString() + "\"");
    influx->WriteData(DatabaseName.toStdString());
  }

}

void MainWindow::CreateFolder(QString path, QString AdditionalMsg){

  QDir dir(path);
  if( !dir.exists()){
    if( dir.mkpath(path)){
      LogMsg("Created folder <b>" + path + "</b> " + AdditionalMsg );
    }else{
      LogMsg("Folder \"<font style=\"color:red;\"><b>" + rawDataPath + "</b>\" cannot be created. Access right problem? </font>" );
    }
  }else{
      LogMsg("Folder \"<b>" + rawDataPath + "</b>\" already exist." );
  }

}

void MainWindow::CreateRawDataFolder(){

  //----- create data folder
  CreateFolder(rawDataPath, "for storing raw data.");

  //----- create root data folder
  CreateFolder(rootDataPath, "for storing root file.");

  //----- create analysis Folder
  if( !analysisPath.isEmpty() ){
    CreateFolder(analysisPath, "for analysis.");
  }

}

void MainWindow::CreateDataSymbolicLink(){
  QString linkName = analysisPath + "/data_raw";
  QFile file;

  if( file.exists(linkName)) {
    file.remove(linkName);
    LogMsg("removing existing Link");
  }

  if (file.link(rawDataPath, linkName)) {
    LogMsg("Symbolic link  <b>" + linkName +"</b> -> " + rawDataPath + " created.");
  } else {
    LogMsg("<font style=\"color:red;\">Symbolic link  <b>" + linkName +"</b> -> " + rawDataPath + " cannot be created. </font>");
  }

  linkName = analysisPath + "/root_data";
  if( file.exists(linkName)) {
    file.remove(linkName);
    LogMsg("removing existing Link");
  }

  if (file.link(rootDataPath, linkName)) {
    LogMsg("Symbolic link  <b>" + linkName +"</b> -> " + rootDataPath + " created.");
  } else {
    LogMsg("<font style=\"color:red;\">Symbolic link  <b>" + linkName +"</b> -> " + rootDataPath + " cannot be created. </font>");
  }


}

//*###################################################################### 
//*###################################################################### log msg and others

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

void MainWindow::SetupInflux(){
  if( influx ) {
    delete influx;
    influx = NULL;
  }

  if( DatabaseIP.isEmpty() || DatabaseName.isEmpty() ){
    LogMsg("No Database IP or Name. No database will be used.");
    return;
  }

  if( DatabaseIP != ""){
    influx = new InfluxDB(DatabaseIP.toStdString(), false);

    if( influx->TestingConnection() ){

      LogMsg("<font style=\"color : green;\"> InfluxDB URL (<b>"+ DatabaseIP + "</b>) is Valid. Version : " + QString::fromStdString(influx->GetVersionString())+ " </font>");

      if( influx->GetVersionNo() > 1 && DatabaseToken.isEmpty() ) {
        LogMsg("<font style=\"color : red;\">A Token is required for accessing the database.</font>");
        delete influx;
        influx = nullptr;
        return;
      }
      
      influx->SetToken(DatabaseToken.toStdString());

      //==== chck database exist
      LogMsg("List of database:");
      influx->CheckDatabases();
      std::vector<std::string> databaseList = influx->GetDatabaseList();
      bool foundDatabase = false;
      for( int i = 0; i < (int) databaseList.size(); i++){
        if( databaseList[i] == DatabaseName.toStdString() ) foundDatabase = true;
        LogMsg(QString::number(i) + "|" + QString::fromStdString(databaseList[i]));
      }

      if( foundDatabase ){
        LogMsg("<font style=\"color : green;\"> Database <b>" + DatabaseName + "</b> found.");

        influx->AddDataPoint("ProgramStart value=1");
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

  if( ElogIP.isEmpty() ) {
    LogMsg("No Elog IP. No elog will be used.");
    elogID = -1;
    return;
  }

  WriteElog("Checking elog writing", "Testing communication", "checking");

  if( elogID > 0 ){
    LogMsg("Checked Elog writing. OK.");

    AppendElog("Check Elog append.", -1);
    if( elogID > 0 ){
      LogMsg("Checked Elog Append. OK.");
    }else{
      LogMsg("<font style=\"color : red;\">Checked Elog Append. FAIL. (no elog will be used.) </font>");
    }

  }else{
    LogMsg("<font style=\"color : red;\">Checked Elog Write. FAIL. (no elog will be used.) (probably logbook <b>" + expName + "</b> does not exist) </font>");
  }

}
void MainWindow::WriteElog(QString htmlText, QString subject, QString category, int runNumber){
  
  //if( elogID < 0 ) return;
  if( expName == "" ) return;

  //TODO ===== user name and pwd load from a file.

  QStringList arg;
  arg << "-h" << ElogIP << "-p" << "8080" << "-l" << expName << "-u" << ElogUser << ElogPWD
      << "-a" << "Author=SOLARIS_DAQ" ;
  if( runNumber > 0 ) arg << "-a" << "RunNo=" + QString::number(runNumber);
  if( category != "" ) arg << "-a" << "Category=" + category;

  arg << "-a" << "Subject=" + subject 
      << "-n " << "2" <<  htmlText  ;

  // printf("Elog command: %s\n", arg.join(" ").toStdString().c_str());

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
  arg << "-h" << ElogIP << "-p" << "8080" << "-l" << expName << "-u" << ElogUser << ElogPWD << "-w" << QString::number(elogID);

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
    arg << "-h" << ElogIP << "-p" << "8080" << "-l" << expName << "-u" << ElogUser << ElogPWD << "-e" << QString::number(elogID)
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

    //TODO ========= add elog bash script to tell mac, capture screenshot and send it back.

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

void MainWindow::WriteRunTimeStampDat(bool isStartRun, QString timeStr){
  
  QString pcName = qEnvironmentVariable("PCName");
  QString data_raw_str = "/data_raw";
  if( pcName == "solaris-daq" ) data_raw_str = "";

  QFile file(masterExpDataPath + "/" + expName + data_raw_str + "/RunTimeStamp.dat");

  if( file.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Append) ){

    if( isStartRun ){
      file.write(("Start Run | " + QString::number(runID) + " | " + timeStr + " | " + startComment + "\n").toStdString().c_str());
    }else{
      file.write((" Stop Run | " + QString::number(runID) + " | " + timeStr + " | " + stopComment + "\n").toStdString().c_str());
    }
    
    file.close();
  }


  QFile fileCSV(masterExpDataPath + "/" + expName + data_raw_str + "/RunTimeStamp.csv");

  if( fileCSV.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Append) ){

    QTextStream out(&fileCSV);

    if( isStartRun){
      out << QString::number(runID) + "," + timeStr + "," + startComment;
    }else{
      out << "," + timeStr + "," + stopComment + "\n";
    }

    fileCSV.close();
  }

}

void MainWindow::AppendComment(){

  //if Started ACQ, append Comment, if ACQ stopped, disbale

  if( !chkSaveRun->isChecked() ) return;

  QDialog * dOpen = new QDialog(this);
  dOpen->setWindowTitle("Append Run Comment");
  dOpen->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
  dOpen->setMinimumWidth(600);
  connect(dOpen, &QDialog::finished, dOpen, &QDialog::deleteLater);

  QGridLayout * vlayout = new QGridLayout(dOpen);
  QLabel *label = new QLabel("Enter Append Run comment for <font style=\"color : red;\">Run-" +  runIDStr + "</font> : ", dOpen);
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
    appendComment = lineEdit->text();
    if( appendComment == "") return;

    appendComment = QDateTime::currentDateTime().toString("[MM.dd hh:mm:ss]") + appendComment;

    AppendElog(appendComment);

    leRunComment->setText("Append Comment: " + appendComment);

    if( influx ){
      influx->ClearDataPointsBuffer();
      influx->AddDataPoint("RunID,start=1 value=" + std::to_string(runID) + ",expName=\"" + expName.toStdString()+ + "\",comment=\"" + appendComment.replace(' ', '_').toStdString() + "\"");
      influx->WriteData(DatabaseName.toStdString());
    }

  }else{
    return;
  }

}
