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
#include <QMutex>

#include <vector>
#include <time.h> // time in nano-sec

#include "digiSettings.h"

#include "ClassDigitizer2Gen.h"
#include "influxdb.h"

static QMutex digiMTX;

class ReadDataThread : public QThread{
  Q_OBJECT

public:
  ReadDataThread(Digitizer2Gen * dig, QObject * parent = 0) : QThread(parent){ 
    stop = false;
    this->digi = dig;
    readCount = 0;
  }

  void Stop() {stop = true;}

  void run(){
    clock_gettime(CLOCK_REALTIME, &ta);
    readCount = 0;

    //for( int i = 0; i < 10; i ++){
    //  emit sendMsg(QString::number(i));
    //  if( stop ) break;
    //}

    while(true){
      digiMTX.lock();
      int ret = digi->ReadData();
      digiMTX.unlock();

      if( ret == CAEN_FELib_Success){
        digi->SaveDataToFile();
      }else if(ret == CAEN_FELib_Stop){
        digi->ErrorMsg("No more data");
        break;
      }else{
        digi->ErrorMsg("ReadDataLoop()");
      }

      clock_gettime(CLOCK_REALTIME, &tb);
      //if( readCount % 1000 == 0 ) {
      if( tb.tv_sec - ta.tv_sec > 2 ) {
        emit sendMsg("FileSize : " +  QString::number(digi->GetFileSize()/1024./1024.) + " MB");
        //double duration = tb.tv_nsec-ta.tv_nsec + tb.tv_sec*1e+9 - ta.tv_sec*1e+9;
        //printf("%4d, duration : %10.0f, %6.1f\n", readCount, duration, 1e9/duration);
        ta = tb;
      }
      readCount++;

    }

  }

signals:
  void sendMsg(const QString &msg);

private:
  bool stop;
  Digitizer2Gen * digi; 
  timespec ta, tb;
  unsigned int readCount;

};

//=================================================

class MainWindow : public QMainWindow{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    
private slots:

    void OpenDigitizers();
    void CloseDigitizers();

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
