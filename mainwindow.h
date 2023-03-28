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
#include <QTimer>

#include <QChart>
#include <QLineSeries>

#include <vector>
#include <time.h> // time in nano-sec

#include "macro.h"
#include "ClassDigitizer2Gen.h"
#include "influxdb.h"

#include "manyThread.h"

#include "digiSettingsPanel.h"
#include "scope.h"
#include "SOLARISpanel.h"

const int chromeWindowID = -1; // disable capture screenshot

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

  void OpenSOLARISpanel();
  bool CheckSOLARISpanelOK();

  void StartACQ();
  void StopACQ();
  void AutoRun();

  void OpenScaler();
  void SetUpScalar();
  void DeleteTriggerLineEdit();
  void UpdateScalar();

  void ProgramSettingsPanel();
  bool LoadProgramSettings();
  void SaveProgramSettings();
  void DecodeIPList();
  void SetupInflux();
  void CheckElog();
  void OpenDirectory(int id);

  void SetupNewExpPanel();
  bool LoadExpSettings();
  void CreateNewExperiment(const QString newExpName);
  void ChangeExperiment(const QString newExpName);
  void WriteExpNameSh();
  void CreateRawDataFolderAndLink();

  void closeEvent(QCloseEvent * event){
    if( digiSetting ) digiSetting->close();
    if( scope ) scope->close();
    if( solarisSetting ) solarisSetting->close();
    event->accept();
  }

  void WriteElog(QString htmlText, QString subject = "", QString category = "",  int runNumber = 0);
  void AppendElog(QString appendHtmlText, int screenID = -1);

  void WriteRunTimeStampDat(bool isStartRun);

signals :

private:

  static Digitizer2Gen ** digi; 
  unsigned short nDigi;
  unsigned short nDigiConnected = 0;

  //@----- log msg
  QPlainTextEdit * logInfo;
  void LogMsg(QString msg);
  bool logMsgHTMLMode = true;
    
  //@----- buttons
  QPushButton * bnProgramSettings;
  QPushButton * bnNewExp;
  QLineEdit   * leExpName;

  QPushButton * bnOpenDigitizers;
  QPushButton * bnCloseDigitizers;
  
  QPushButton * bnDigiSettings;
  QPushButton * bnSOLSettings;

  //@-----  scope
  Scope * scope;
  QPushButton * bnOpenScope;

  //@----- scalar;
  QMainWindow  * scalar;
  QPushButton  * bnOpenScalar;
  QLineEdit  *** leTrigger; // need to delete manually
  QLineEdit  *** leAccept; // need to delete manually
  QGridLayout  * scalarLayout;
  ScalarThread * scalarThread;
  QLabel       * lbLastUpdateTime;
  QLabel       * lbScalarACQStatus;
  InfluxDB     * influx;

  //@------ ACQ things
  QPushButton * bnStartACQ;
  QPushButton * bnStopACQ;
  QCheckBox   * chkSaveRun;
  QComboBox   * cbAutoRun;
  QLineEdit   * leRunID;
  QLineEdit   * leRawDataPath;
  QLineEdit   * leRunComment;
  ReadDataThread ** readDataThread;   
  QString startComment;
  QString stopComment;
  QString appendComment;
  bool needManualComment;
  bool isRunning;
  QTimer * runTimer;
  unsigned int autoRunStartRunID;

  //@----- digi Setting panel
  DigiSettingsPanel * digiSetting;

  //@----- SOLARIS setting panel
  SOLARISpanel * solarisSetting;
  std::vector<std::vector<int>> mapping;
  QStringList detType;
  std::vector<int> detMaxID;

  //@----- Program settings
  QLineEdit * lSaveSettingPath; // only live in ProgramSettigns()
  QLineEdit * lAnalysisPath; // only live in ProgramSettigns()
  QLineEdit * lDataPath; // only live in ProgramSettigns()
  QLineEdit * lRootDataPath; // only live in ProgramSettigns()

  QLineEdit * lIPDomain;
  QLineEdit * lDatbaseIP;
  QLineEdit * lDatbaseName;
  QLineEdit * lElogIP;

  QString settingFilePath;
  QString analysisPath;
  QString dataPath;
  QString rootDataPath;
  QString IPListStr;
  QStringList IPList;
  QString DatabaseIP;
  QString DatabaseName;
  QString ElogIP;

  //@------ experiment settings
  bool isGitExist;
  bool useGit;
  QString expName;
  QString rawDataFolder;
  QString rootDataFolder;
  int runID;
  QString runIDStr;
  int elogID;  // 0 = ready, -1 = disable, >1 = elogID

  //@------ calculate instant accept Rate
  unsigned long oldSavedCount[MaxNumberOfDigitizer][MaxNumberOfChannel];
  unsigned long oldTimeStamp[MaxNumberOfDigitizer][MaxNumberOfChannel];

  //@------ connection between pannels
  void UpdateAllPanel(int panelID);

  //@------ custom comment;
  QPushButton * bnComment;
  void AppendComment();
};


#endif // MAINWINDOW_H
