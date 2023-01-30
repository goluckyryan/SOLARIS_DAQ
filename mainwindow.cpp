#include "mainwindow.h"

#include <QLabel>
#include <QGridLayout>

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

  QWidget * mainLayoutWidget = new QWidget(this);
  setCentralWidget(mainLayoutWidget);
  QVBoxLayout * layoutMain = new QVBoxLayout(mainLayoutWidget);
  mainLayoutWidget->setLayout(layoutMain);


  {//====================== General
    QGroupBox * box1 = new QGroupBox("General", mainLayoutWidget);
    layoutMain->addWidget(box1);

    QGridLayout * layout1 = new QGridLayout(box1);

    bnProgramSettings = new QPushButton("Program Settings", this);
    bnProgramSettings->setEnabled(false);

    bnOpenDigitizers = new QPushButton("Open Digitizers", this);
    connect(bnOpenDigitizers, SIGNAL(clicked()), this, SLOT(bnOpenDigitizers_clicked()));

    bnCloseDigitizers = new QPushButton("Close Digitizers", this);
    bnCloseDigitizers->setEnabled(false);
    connect(bnCloseDigitizers, SIGNAL(clicked()), this, SLOT(bnCloseDigitizers_clicked()));
  
    bnDigiSettings = new QPushButton("Digitizers Settings", this);
    bnDigiSettings->setEnabled(false);
    connect(bnDigiSettings, SIGNAL(clicked()), this, SLOT(OpenDigitizersSettings()));


    layout1->addWidget(bnProgramSettings, 0, 0);
    layout1->addWidget(bnOpenDigitizers, 0, 1);
    layout1->addWidget(bnCloseDigitizers, 0, 2);
    layout1->addWidget(bnDigiSettings, 1, 1);

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
  
    layout2->addWidget(bnStartACQ, 0, 0);
    layout2->addWidget(bnStopACQ, 0, 1);

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

  delete digiSetting;

  delete bnProgramSettings;
  delete bnOpenDigitizers;
  delete bnCloseDigitizers;
  delete bnDigiSettings;
  delete logInfo;

  if( digi != NULL ){
    digi->CloseDigitizer();
    delete digi;
  }

  readDataThread->Stop();
  readDataThread->quit();
  readDataThread->wait();
  delete readDataThread;

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

    LogMsg("use a dummy.");

    digi->SetDummy();
    digiSerialNum.push_back(0000);
    nDigi ++;

  }

}

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

void MainWindow::LogMsg(QString msg){

    QString countStr = QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString("MM.dd hh:mm:ss"), msg);
    logInfo->appendPlainText(countStr);
    QScrollBar *v = logInfo->verticalScrollBar();
    v->setValue(v->maximum());
    //qDebug() << msg;
    logInfo->repaint();
}
