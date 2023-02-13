#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QMainWindow>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <qdebug.h>
#include <QDateTime>
#include <QScrollBar>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>

#include <QChart>
#include <QLineSeries>

#include <vector>
#include <time.h> // time in nano-sec

#include "ClassDigitizer2Gen.h"
#include "influxdb.h"

#include "manyThread.h"

#include "digiSettings.h"
#include "scope.h"

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
  void OpenDigitizersSettings();

  void OpenScaler();
  void SetUpScalar();
  void DeleteTriggerLineEdit();
  void UpdateScalar();

  void ProgramSettings();
  bool OpenProgramSettings();
  void SaveProgramSettings();
  void DecodeIPList();
  void OpenDirectory(int id);

  void SetupNewExp();
  bool OpenExpSettings();
  void CreateNewExperiment(const QString newExpName);
  void ChangeExperiment(const QString newExpName);
  void CreateRawDataFolderAndLink(const QString newExpName);

  void closeEvent(QCloseEvent * event){
    if( digiSetting != NULL ) digiSetting->close();
    if( scope != NULL ) scope->close();
    event->accept();
  }


signals :

private:
    
  QPushButton * bnProgramSettings;
  QPushButton * bnNewExp;
  QLineEdit   * leExpName;

  QPushButton * bnOpenDigitizers;
  QPushButton * bnCloseDigitizers;
  
  QPushButton * bnDigiSettings;
  QPushButton * bnSOLSettings;

  //@--- new scope
  Scope * scope;
  QPushButton * bnOpenScope;

  //@----- scalar;
  QMainWindow * scalar;
  QPushButton * bnOpenScalar;
  QLineEdit *** leTrigger; // need to delete manually
  QLineEdit *** leAccept; // need to delete manually
  QGridLayout * scalarLayout;
  ScalarThread * scalarThread;
  QLabel      * lbLastUpdateTime;

  //@------ ACQ things
  QPushButton * bnStartACQ;
  QPushButton * bnStopACQ;
  QCheckBox   * chkSaveRun;
  QComboBox   * cbAutoRun;
  QLineEdit   * leRunID;
  QLineEdit   * leRawDataPath;
  ReadDataThread ** readDataThread;   
  void StartACQ();
  void StopACQ();

  DigiSettings * digiSetting;

  QPlainTextEdit * logInfo;

  static Digitizer2Gen ** digi; 
  unsigned short nDigi;

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
