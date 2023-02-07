#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QMainWindow>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QThread>
#include <qdebug.h>
#include <QDateTime>
#include <QScrollBar>
#include <QPushButton>
#include <QComboBox>
#include <QMutex>
#include <QChart>
#include <QLineSeries>

#include <vector>
#include <time.h> // time in nano-sec

#include "digiSettings.h"

#include "ClassDigitizer2Gen.h"
#include "influxdb.h"

static QMutex digiMTX;

//^#===================================================== ReadData Thread
class ReadDataThread : public QThread {
  Q_OBJECT
public:
  ReadDataThread(Digitizer2Gen * dig, QObject * parent = 0) : QThread(parent){ 
    this->digi = dig;
    isScopeRun = false;
  }
  void SetScopeRun(bool onOff) {this->isScopeRun = onOff;}
  void run(){
    clock_gettime(CLOCK_REALTIME, &ta);
    while(true){
      digiMTX.lock();
      int ret = digi->ReadData();
      digiMTX.unlock();

      if( ret == CAEN_FELib_Success){
        if( !isScopeRun) digi->SaveDataToFile();
      }else if(ret == CAEN_FELib_Stop){
        digi->ErrorMsg("No more data");
        break;
      }else{
        digi->ErrorMsg("ReadDataLoop()");
        digi->evt->ClearTrace();
      }

      if( !isScopeRun ){
        clock_gettime(CLOCK_REALTIME, &tb);
        if( tb.tv_sec - ta.tv_sec > 2 ) {
          emit sendMsg("FileSize : " +  QString::number(digi->GetFileSize()/1024./1024.) + " MB");
          
          //double duration = tb.tv_nsec-ta.tv_nsec + tb.tv_sec*1e+9 - ta.tv_sec*1e+9;
          //printf("%4d, duration : %10.0f, %6.1f\n", readCount, duration, 1e9/duration);
          ta = tb;
        }
      }
    }
  }
signals:
  void sendMsg(const QString &msg);
private:
  Digitizer2Gen * digi; 
  timespec ta, tb;
  bool isScopeRun;
};

//^#===================================================== UpdateTrace Thread
class UpdateTraceThread : public QThread {
  Q_OBJECT
public:
  UpdateTraceThread(QObject * parent = 0) : QThread(parent){
    waitTime = 2;
    stop = false;
  }
  void Stop() {this->stop = true;}
  void run(){
    unsigned int count = 0;
    stop = false;
    do{
      usleep(100000);
      count ++;
      if( count % waitTime == 0){
        emit updateTrace();
      }
    }while(!stop);
  }
signals:
  void updateTrace();

private:
  bool stop;
  unsigned int waitTime; //100 of milisec
};


//^#===================================================== MainWindow
class MainWindow : public QMainWindow{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

    
private slots:

  void OpenDigitizers();
  void CloseDigitizers();
  
  void OpenScope();
  void StartScope();
  void StopScope();
  void SetUpPlot();
  void UpdateScope();

  void OpenDigitizersSettings();

  void ProgramSettings();
  bool OpenProgramSettings();
  void SaveProgramSettings();
  void OpenDirectory(int id);

  void SetupNewExp();
  bool OpenExpSettings();
  void CreateNewExperiment(const QString newExpName);
  void ChangeExperiment(const QString newExpName);
  void CreateRawDataFolderAndLink(const QString newExpName);

signals :

private:
    
  QPushButton * bnProgramSettings;
  QPushButton * bnNewExp;
  QLineEdit   * leExpName;

  QPushButton * bnOpenDigitizers;
  QPushButton * bnCloseDigitizers;
  
  QPushButton * bnDigiSettings;
  QPushButton * bnSOLSettings;

  //@------ scope things
  QMainWindow * scope;
  QPushButton * bnOpenScope;
  QChart      * plot;
  QLineSeries * dataTrace[6];
  UpdateTraceThread * updateTraceThread;
  QComboBox   * cbScopeDigi;
  QComboBox   * cbScopeCh;
  QComboBox   * cbAnaProbe[2];
  QComboBox   * cbDigProbe[4];
  QSpinBox * sbRL; // record length
  QSpinBox * sbPT; // pre trigger
  QPushButton * bnScopeStart;
  QPushButton * bnScopeStop;
  bool allowChange;
  void ProbeChange(QComboBox * cb[], const int size);

  //@------ ACQ things
  QPushButton * bnStartACQ;
  QPushButton * bnStopACQ;
  QLineEdit   * leRunID;
  QLineEdit   * leRawDataPath;

  DigiSettings * digiSetting;

  QPlainTextEdit * logInfo;

  static Digitizer2Gen ** digi; 
  unsigned short nDigi;
  std::vector<unsigned short> digiSerialNum;

  void StartACQ();
  void StopACQ();

  ReadDataThread ** readDataThread;   

  void LogMsg(QString msg);
  bool logMsgHTMLMode = true;

  //---------------- Program settings
  QLineEdit * lSaveSettingPath; // only live in ProgramSettigns()
  QLineEdit * lAnalysisPath; // only live in ProgramSettigns()
  QLineEdit * lDataPath; // only live in ProgramSettigns()

  QLineEdit * lIPDomain;
  QLineEdit * lDatbaseIP;
  QLineEdit * lDatbaseName;
  QLineEdit * lElogIP;

  QString settingFilePath;
  QString analysisPath;
  QString dataPath;
  QString IPListStr;
  QStringList IPList;
  QString DatabaseIP;
  QString DatabaseName;
  QString ElogIP;

  //------------- experiment settings
  bool isGitExist;
  bool useGit;
  QString expName;
  QString rawDataFolder;
  unsigned int runID;
  unsigned int elogID;

};


#endif // MAINWINDOW_H
