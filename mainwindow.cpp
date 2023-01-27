#include "mainwindow.h"

#include <QLabel>
#include <QGridLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent){

  setWindowTitle("SOLARIS DAQ");
  setGeometry(500, 500, 1000, 500);
  QIcon icon("SOLARIS_favicon.png");
  setWindowIcon(icon);

  nDigi = 0;
  digiSerialNum.clear();
  digiSetting = NULL;

  QWidget * mainLayoutWidget = new QWidget(this);
  setCentralWidget(mainLayoutWidget);
  QVBoxLayout * layout1 = new QVBoxLayout(mainLayoutWidget);
  mainLayoutWidget->setLayout(layout1);

  {
    QGridLayout *layout = new QGridLayout();
    layout1->addLayout(layout);
    layout1->addStretch();
    layout1->setStretchFactor(layout, 8);

    bnProgramSettings = new QPushButton("Program Settings", this);

    bnOpenDigitizers = new QPushButton("Open Digitizers", this);
    connect(bnOpenDigitizers, SIGNAL(clicked()), this, SLOT(bnOpenDigitizers_clicked()));

    bnCloseDigitizers = new QPushButton("Close Digitizers", this);
    bnCloseDigitizers->setEnabled(false);
    connect(bnCloseDigitizers, SIGNAL(clicked()), this, SLOT(bnCloseDigitizers_clicked()));
  
    bnDigiSettings = new QPushButton("Digitizers Settings", this);
    bnDigiSettings->setEnabled(false);
    connect(bnDigiSettings, SIGNAL(clicked()), this, SLOT(OpenDigitizersSettings()));

    bnStartACQ = new QPushButton("Start ACQ", this);
    bnStartACQ->setEnabled(false);
    bnStopACQ = new QPushButton("Stop ACQ", this);
    bnStopACQ->setEnabled(false);

    layout->addWidget(bnProgramSettings, 0, 0);
    layout->addWidget(bnOpenDigitizers, 0, 1);
    layout->addWidget(bnCloseDigitizers, 0, 2);
    layout->addWidget(bnDigiSettings, 1, 1);
  
    QFrame * separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    layout->addWidget(separator, 2, 0, 1, 3);

    layout->addWidget(bnStartACQ, 3, 0);
    layout->addWidget(bnStopACQ, 3, 1);

  }

  logInfo = new QPlainTextEdit(this);
  logInfo->isReadOnly();
  logInfo->setGeometry(100, 200, 500, 100);

  layout1->addWidget(logInfo);
  layout1->setStretchFactor(logInfo, 1);



  //StartRunThread = new QThread(this);
  //connect(StartRunThread, &QThread::started, this, &MainWindow::onThreadStarted);
  //connect(StartRunThread, &QThread::finished, this, &MainWindow::onThreadFinished);

  LogMsg("Welcome to SOLARIS DAQ.");

  bnOpenDigitizers_clicked();
  OpenDigitizersSettings();

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

    //StartRunThread->quit();
    //StartRunThread->wait();
    //delete StartRunThread;
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
  }
}

void MainWindow::OpenDigitizersSettings(){
  LogMsg("Open digitizers Settings Panel");

  if( digiSetting == NULL){
    digiSetting = new DigiSettings(digi, nDigi);
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
    qDebug() << msg;
    logInfo->repaint();
}
