#include "mainwindow.h"

#include <QLabel>
#include <QGridLayout>
#include <QDialog>
#include <QFileDialog>
#include <QStorageInfo>
#include <QDir>
#include <QFile>

#include <unistd.h>

//------ static memeber
Digitizer2Gen * MainWindow::digi = NULL;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent){

  setWindowTitle("SOLARIS DAQ");
  setGeometry(500, 100, 1000, 500);
  QIcon icon("SOLARIS_favicon.png");
  setWindowIcon(icon);

  nDigi = 0;
  digiSerialNum.clear();
  digiSetting = NULL;
  readDataThread = NULL;

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

    bnOpenDigitizers = new QPushButton("Open Digitizers", this);
    connect(bnOpenDigitizers, SIGNAL(clicked()), this, SLOT(bnOpenDigitizers_clicked()));

    bnCloseDigitizers = new QPushButton("Close Digitizers", this);
    bnCloseDigitizers->setEnabled(false);
    connect(bnCloseDigitizers, SIGNAL(clicked()), this, SLOT(bnCloseDigitizers_clicked()));
  
    bnDigiSettings = new QPushButton("Digitizers Settings", this);
    bnDigiSettings->setEnabled(false);
    connect(bnDigiSettings, SIGNAL(clicked()), this, SLOT(OpenDigitizersSettings()));

    QPushButton * bnSOLSettings = new QPushButton("SOLARIS Settings", this);
    bnSOLSettings->setEnabled(false);

    QPushButton * bnOpenScope = new QPushButton("Open scope", this);
    bnOpenScope->setEnabled(false);

    bnNewExp = new QPushButton("New/Change Exp", this);
    connect(bnNewExp, &QPushButton::clicked, this, &MainWindow::SetupNewExp);

    QLineEdit * lExpName = new QLineEdit("<Exp Name>", this);
    lExpName->setReadOnly(true);

    layout1->addWidget(bnProgramSettings, 0, 0);
    layout1->addWidget(bnNewExp, 0, 1);
    layout1->addWidget(lExpName, 0, 2);

    layout1->addWidget(bnOpenScope, 1, 0);
    layout1->addWidget(bnOpenDigitizers, 1, 1);
    layout1->addWidget(bnCloseDigitizers, 1, 2);
    layout1->addWidget(bnDigiSettings, 2, 1);
    layout1->addWidget(bnSOLSettings, 2, 2);


    for( int i = 0; i < layout1->columnCount(); i++) layout1->setColumnStretch(i, 1);

  }


  {//====================== ACD control
    QGroupBox * box2 = new QGroupBox("ACQ control", mainLayoutWidget);
    layoutMain->addWidget(box2);

    QGridLayout * layout2 = new QGridLayout(box2);
    
    bnStartACQ = new QPushButton("Start ACQ", this);
    bnStartACQ->setEnabled(false);
    connect(bnStartACQ, &QPushButton::clicked, this, &MainWindow::StartACQ);
    
    bnStopACQ = new QPushButton("Stop ACQ", this);
    bnStopACQ->setEnabled(false);
    connect(bnStopACQ, &QPushButton::clicked, this, &MainWindow::StopACQ);

    QLabel * lbRunID = new QLabel("Run ID : ", this);
    lbRunID->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    QLineEdit * runID = new QLineEdit(this);
    runID->setReadOnly(true);
    
    QLabel * lbRunComment = new QLabel("Run Comment : ", this);
    lbRunComment->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    QLineEdit * runComment = new QLineEdit(this);
    runComment->setReadOnly(true);
    
    layout2->addWidget(lbRunID, 0, 0);
    layout2->addWidget(runID, 0, 1);
    layout2->addWidget(bnStartACQ, 0, 2);
    layout2->addWidget(bnStopACQ, 0, 3);
    layout2->addWidget(lbRunComment, 1, 0);
    layout2->addWidget(runComment, 1, 1, 1, 3);

    layout2->setColumnStretch(0, 0.3);
    for( int i = 0; i < layout2->columnCount(); i++) layout2->setColumnStretch(i, 1);

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

  bool isSettingOK = OpenProgramSettings();
  if( isSettingOK == false){
    bnProgramSettings->setStyleSheet("color: red;");

    bnNewExp->setEnabled(false);
    bnOpenDigitizers->setEnabled(false);
  }

  //bnOpenDigitizers_clicked();
  //OpenDigitizersSettings();

}

MainWindow::~MainWindow(){

  //---- may be no need to delete as thay are child of this
  //delete bnProgramSettings;
  //delete bnOpenDigitizers;
  //delete bnCloseDigitizers;
  //delete bnDigiSettings;
  //delete bnNewExp;
  //delete logInfo;

  //---- need manually delete
  if( digiSetting != NULL ) delete digiSetting;

  if( digi != NULL ){
    digi->CloseDigitizer();
    delete digi;
  }

  if( readDataThread != NULL){
    readDataThread->Stop();
    readDataThread->quit();
    readDataThread->wait();
    delete readDataThread;
  }

}

//################################################################
void MainWindow::StartACQ(){

  digi->Reset();
  digi->ProgramPHA(false);
  digi->SetPHADataFormat(1);// only save 1 trace
  remove("haha_000.sol"); // remove file
  digi->OpenOutFile("haha");// haha_000.sol
  digi->StartACQ();

  LogMsg("Start Run....");

  readDataThread->start();

  bnStartACQ->setEnabled(false);
  bnStopACQ->setEnabled(true);

  LogMsg("end of " + QString::fromStdString(__func__));
}

void MainWindow::StopACQ(){

  digi->StopACQ();
  
  //readDataThread->Stop();

  readDataThread->quit();
  readDataThread->wait();

  digi->CloseOutFile();
  
  LogMsg("Stop Run");

  bnStartACQ->setEnabled(true);
  bnStopACQ->setEnabled(false);

}


void MainWindow::bnOpenDigitizers_clicked(){
  LogMsg("Opening digitizer.....");

  digi = new Digitizer2Gen();

  digi->OpenDigitizer("dig2://192.168.0.100/");

  if(digi->IsConnected()){

    digiSerialNum.push_back(digi->GetSerialNumber());
    nDigi ++;

    LogMsg("Opened digitizer : " + QString::number(digi->GetSerialNumber()));
    bnOpenDigitizers->setEnabled(false);
    bnCloseDigitizers->setEnabled(true);
    bnDigiSettings->setEnabled(true);
    bnStartACQ->setEnabled(true);
    bnStopACQ->setEnabled(false);

    readDataThread = new ReadDataThread(digi, this);
    connect(readDataThread, &ReadDataThread::sendMsg, this, &MainWindow::LogMsg);

  }else{
    LogMsg("Cannot open digitizer");

    //LogMsg("use a dummy.");
    //digi->SetDummy();
    //digiSerialNum.push_back(0000);
    //nDigi ++;

    delete digi;

  }

}

//######################################################################
void MainWindow::bnCloseDigitizers_clicked(){
  if( digi != NULL ){
    digi->CloseDigitizer();
    delete digi;
    digi = NULL;
    LogMsg("Closed Digitizer : " + QString::number(digiSerialNum[0]));
    
    nDigi = 0;
    digiSerialNum.clear();

    bnOpenDigitizers->setEnabled(true);
    bnCloseDigitizers->setEnabled(false);
    bnDigiSettings->setEnabled(false);
    bnStartACQ->setEnabled(false);
    bnStopACQ->setEnabled(false);

    if( digiSetting != NULL )  digiSetting->close(); 

  }
}

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
//######################################################################

void MainWindow::SetupNewExp(){

  QDialog dialog(this);
  dialog.setWindowTitle("Setup / change Experiment");
  dialog.setGeometry(0, 0, 500, 500);

  QVBoxLayout * layout = new QVBoxLayout(&dialog);

  //------- instruction
  QLabel *label = new QLabel("Here list the pass experiments. ", &dialog);
  layout->addWidget(label);

  //------- get and list the git repository
  QPlainTextEdit * gitList = new QPlainTextEdit(&dialog);
  layout->addWidget(gitList);
  gitList->setReadOnly(true);

  //------- get harddisk space;
  //QStorageInfo storage("/path/to/drive");
  QStorageInfo storage = QStorageInfo::root();
  qint64 availableSpace = storage.bytesAvailable();

  QLabel * lbDiskSpace = new QLabel("Disk space avalible " + QString::number(availableSpace/1024./1024./1024.) + " [GB]", &dialog);
  layout->addWidget(lbDiskSpace);

  //------- type existing or new experiment
  QLineEdit * input = new QLineEdit(&dialog);
  layout->addWidget(input);


  QPushButton *button1 = new QPushButton("OK", &dialog);
  layout->addWidget(button1);
  QObject::connect(button1, &QPushButton::clicked, &dialog, &QDialog::accept);

  dialog.exec();

}

void MainWindow::ProgramSettings(){

  LogMsg("Open <b>Program Settings</b>.");

  QDialog dialog(this);
  dialog.setWindowTitle("Program Settings");
  dialog.setGeometry(0, 0, 700, 450);

  QGridLayout * layout = new QGridLayout(&dialog);
  layout->setVerticalSpacing(0);

  unsigned int rowID = 0;

  //-------- Instruction
  QPlainTextEdit * helpInfo = new QPlainTextEdit(&dialog);
  helpInfo->setReadOnly(true);
  helpInfo->setLineWrapMode(QPlainTextEdit::LineWrapMode::WidgetWidth);
  helpInfo->appendHtml("These setting will be saved at the <font style=\"color : red;\"> Settings Save Path </font> as <b>programSettings.txt</b>. If no such file exist, the program will create it.");
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : red;\">  Analysis Path  </font> is the path of the folder of the analysis code. e.g. /home/<user>/analysis/");
  helpInfo->appendHtml("<font style=\"color : red;\">  Data Path  </font> is the path of the <b>parents folder</b> of Raw data will store. e.g. /mnt/data0/, experiment data will be saved under this folder. e.g. /mnt/data1/exp1");
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("These 2 paths will be used when <font style=\"color : blue;\">  New/Change Exp </font>");
  helpInfo->appendHtml("<p></p>");
  helpInfo->appendHtml("<font style=\"color : red;\">  Digitizers IP Domain </font> is the frist 6 digi of the digitizers IP. The program will search for all digitizers under this domain.");
  helpInfo->appendHtml("<p></p>");

  layout->addWidget(helpInfo, rowID, 0, 1, 4);

  //-------- analysis Path
  rowID ++;
  QLabel *lbSaveSettingPath = new QLabel("Settings Save Path", &dialog);
  lbSaveSettingPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbSaveSettingPath, rowID, 0);
  lSaveSettingPath = new QLineEdit(QDir::current().absolutePath(), &dialog); layout->addWidget(lSaveSettingPath, rowID, 1, 1, 2);

  QPushButton * bnSaveSettingPath = new QPushButton("browser", &dialog); layout->addWidget(bnSaveSettingPath, rowID, 3);
  connect(bnSaveSettingPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(0);});

  //-------- analysis Path
  rowID ++;
  QLabel *lbAnalysisPath = new QLabel("Analysis Path", &dialog);
  lbAnalysisPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbAnalysisPath, rowID, 0);
  lAnalysisPath = new QLineEdit(QDir::home().absolutePath() + "/analysis", &dialog); layout->addWidget(lAnalysisPath, rowID, 1, 1, 2);

  QPushButton * bnAnalysisPath = new QPushButton("browser", &dialog); layout->addWidget(bnAnalysisPath, rowID, 3);
  connect(bnAnalysisPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(1);});

  //-------- data Path
  rowID ++;
  QLabel *lbDataPath = new QLabel("Data Path", &dialog); 
  lbDataPath->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDataPath, rowID, 0);
  lDataPath = new QLineEdit("/mnt/data1", &dialog); layout->addWidget(lDataPath, rowID, 1, 1, 2);

  QPushButton * bnDataPath = new QPushButton("browser", &dialog); layout->addWidget(bnDataPath, rowID, 3);
  connect(bnDataPath, &QPushButton::clicked, this, [=](){this->OpenDirectory(2);});

  //-------- IP Domain
  rowID ++;
  QLabel *lbIPDomain = new QLabel("Digitizers IP Domain", &dialog); 
  lbIPDomain->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbIPDomain, rowID, 0);
  lIPDomain = new QLineEdit("192.168.0", &dialog); layout->addWidget(lIPDomain, rowID, 1, 1, 2);
  //-------- DataBase IP
  rowID ++;
  QLabel *lbDatbaseIP = new QLabel("Database IP", &dialog); 
  lbDatbaseIP->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDatbaseIP, rowID, 0);
  lDatbaseIP = new QLineEdit("https://localhost:8086", &dialog); layout->addWidget(lDatbaseIP, rowID, 1, 1, 2);
  //-------- DataBase name
  rowID ++;
  QLabel *lbDatbaseName = new QLabel("Database Name", &dialog);
  lbDatbaseName->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbDatbaseName, rowID, 0);
  lDatbaseName = new QLineEdit("SOLARIS", &dialog); layout->addWidget(lDatbaseName, rowID, 1, 1, 2);
  //-------- Elog IP
  rowID ++;
  QLabel *lbElogIP = new QLabel("Elog IP", &dialog);
  lbElogIP->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  layout->addWidget(lbElogIP, rowID, 0);
  lElogIP = new QLineEdit("https://localhost:8080", &dialog); layout->addWidget(lElogIP, rowID, 1, 1, 2);

  rowID ++;
  QPushButton *button1 = new QPushButton("OK and Save", &dialog);
  layout->addWidget(button1, rowID, 1);
  QObject::connect(button1, &QPushButton::clicked, this, &MainWindow::SaveProgramSettings);
  QObject::connect(button1, &QPushButton::clicked, &dialog, &QDialog::accept);
  
  QPushButton *button2 = new QPushButton("Cancel", &dialog);
  layout->addWidget(button2, rowID, 2);
  QObject::connect(button2, &QPushButton::clicked, this, [=](){this->LogMsg("<b>Cancel Program Settings</b>");});
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

  qDebug() << fileDialog.selectedFiles();
  
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

  if( !file.open(QIODevice::Text | QIODevice::ReadOnly) ) {
    LogMsg("<b>" + settingFile + "</b> not found.");
    LogMsg("Please Open the <font style=\"color : red;\">Program Settings </font>");
    return false;
  }
  
  QTextStream in(&file);
  QString line = in.readLine();

  int count = 0;
  while( !line.isNull()){
    if( line.left(6) == "//----") break;

    switch (count){
      case 0 : settingFilePath = line; break;
      case 1 : analysisPath    = line; break;
      case 2 : dataPath        = line; break;
      case 3 : IPDomain        = line; break;
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
    LogMsg("  Digi. IP Domain : " + IPDomain);
    LogMsg("      Database IP : " + DatabaseIP);
    LogMsg("    Database Name : " + DatabaseName);
    LogMsg("           ElogIP : " + ElogIP);
    logMsgHTMLMode = true;
    return true;
  }else{
    LogMsg("Settings are not complete.");
    LogMsg("Please Open the <font style=\"color : red;\">Program Settings </font>");
    return false;
  }
}

void MainWindow::SaveProgramSettings(){

  IPDomain = lIPDomain->text();
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
  file.write((IPDomain+"\n").toStdString().c_str());
  file.write((DatabaseIP+"\n").toStdString().c_str());
  file.write((DatabaseName+"\n").toStdString().c_str());
  file.write((ElogIP+"\n").toStdString().c_str());
  file.write("//------------end of file.");
  
  file.close();

  LogMsg("Saved program settings to <b>"+settingFilePath + "/programSettings.txt<b>.");
}

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
