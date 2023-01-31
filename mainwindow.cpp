#include "mainwindow.h"

#include <QLabel>
#include <QGridLayout>
#include <QDialog>
#include <QStorageInfo>

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
    layout1->addWidget(bnOpenDigitizers, 0, 1);
    layout1->addWidget(bnCloseDigitizers, 0, 2);
    layout1->addWidget(bnDigiSettings, 1, 1);
    layout1->addWidget(bnSOLSettings, 1, 2);

    layout1->addWidget(bnNewExp, 2, 0);
    layout1->addWidget(lExpName, 2, 1);
    layout1->addWidget(bnOpenScope, 2, 2);

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

    QGridLayout * layout3 = new QGridLayout(box3);

    logInfo = new QPlainTextEdit(this);
    logInfo->isReadOnly();
    logInfo->setGeometry(100, 200, 500, 100);

    layout3->addWidget(logInfo);

  }
  
  LogMsg("Welcome to SOLARIS DAQ.");

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

  QDialog dialog(this);
  dialog.setWindowTitle("Program Settings");
  dialog.setGeometry(0, 0, 500, 500);

  QGridLayout * layout = new QGridLayout(&dialog);

  //-------- data Path
  QLabel *lbDataPath = new QLabel("Data Path", &dialog); layout->addWidget(lbDataPath, 0, 0);
  QLineEdit * lDataPath = new QLineEdit("/path/to/data", &dialog); layout->addWidget(lDataPath, 0, 1, 1, 2);
  //-------- analysis Path

  //-------- IP search range

  //-------- DataBase IP

  //-------- DataBase name
  
  //-------- Elog IP

  QPushButton *button1 = new QPushButton("OK", &dialog);
  layout->addWidget(button1, 2, 1);
  QObject::connect(button1, &QPushButton::clicked, &dialog, &QDialog::accept);

  dialog.exec();
}

void MainWindow::LogMsg(QString msg){

    QString countStr = QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString("MM.dd hh:mm:ss"), msg);
    logInfo->appendPlainText(countStr);
    QScrollBar *v = logInfo->verticalScrollBar();
    v->setValue(v->maximum());
    //qDebug() << msg;
    logInfo->repaint();
}
