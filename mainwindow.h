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
#include <QThread>

#include <QChart>
#include <QLineSeries>

#include <vector>
#include <time.h> // time in nano-sec

#include "macro.h"
#include "ClassDigitizer2Gen.h"
// #include "influxdb.h"
#include "ClassInfluxDB.h"

#include "CustomThreads.h"

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
  void OpenSyncHelper();

  void OpenSOLARISpanel();
  bool CheckSOLARISpanelOK();

  int  StartACQ(); // return 1 when ACQ start
  void StopACQ();
  void AutoRun();

  void OpenScaler();
  void SetUpScalar();
  void CleanUpScalar();
  void UpdateScalar();

  void ProgramSettingsPanel();
  bool LoadProgramSettings();
  void SaveProgramSettings();
  void DecodeIPList();
  void SetupInflux();
  void CheckElog();
  void OpenDirectory(int id);

  void SetupNewExpPanel();
  bool LoadExpNameSh();
  void CreateNewExperiment(const QString newExpName);
  void ChangeExperiment(const QString newExpName);
  void WriteExpNameSh();
  void CreateFolder(QString path, QString AdditionalMsg);
  void CreateRawDataFolder();
  void CreateDataSymbolicLink();

  void closeEvent(QCloseEvent * event){
    if( digiSetting ) digiSetting->close();
    if( scope ) scope->close();
    if( solarisSetting ) solarisSetting->close();
    event->accept();
  }

  void WriteElog(QString htmlText, QString subject = "", QString category = "",  int runNumber = 0);
  void AppendElog(QString appendHtmlText, int screenID = -1);

  void WriteRunTimeStampDat(bool isStartRun, QString timeStr);

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

  QPushButton * bnSyncHelper;
  QPushButton * bnOpenDigitizers;
  QPushButton * bnCloseDigitizers;
  
  QPushButton * bnDigiSettings;
  QPushButton * bnSOLSettings;


  //@-----  scope
  Scope * scope;
  QPushButton * bnOpenScope;

  //@----- scalar;
  QMainWindow  * scalar;
  QGridLayout  * scalarLayout;
  TimingThread * scalarThread;
  QPushButton  * bnOpenScalar;
  QLabel      ** lbFileSize;// need to delete manually
  QLineEdit  *** leTrigger; // need to delete manually
  QLineEdit  *** leAccept; // need to delete manually
  QLabel       * lbLastUpdateTime;
  QLabel       * lbScalarACQStatus;
  bool           scalarOutputInflux;

  InfluxDB     * influx;

  //@------ ACQ things
  QPushButton * bnStartACQ;
  QPushButton * bnStopACQ;
  QCheckBox   * chkSaveRun;
  QComboBox   * cbAutoRun;
  QComboBox * cbDataFormat;
  QLineEdit   * leRunID;
  QLineEdit   * leRawDataPath;
  QLineEdit   * leRunComment;
  ReadDataThread ** readDataThread;   
  QString startComment;
  QString stopComment;
  QString appendComment;
  bool needManualComment;
  bool isACQRunning;
  QTimer * runTimer;
  QElapsedTimer elapsedTimer;
  unsigned int autoRunStartRunID;

  bool ACQStopButtonPressed;

  //@----- digi Setting panel
  DigiSettingsPanel * digiSetting;

  //@----- SOLARIS setting panel
  SOLARISpanel * solarisSetting;
  std::vector<std::vector<int>> mapping;
  QStringList detType;
  std::vector<int> detMaxID;
  std::vector<int> detGroupID;
  QStringList detGroupName;

  //@----- Program settings
  QLineEdit * lAnalysisPath; //for git
  QLineEdit * lExpDataPath; 

  QLineEdit * lExpName;

  QLineEdit * lIPDomain;
  QLineEdit * lDatbaseIP;
  QLineEdit * lDatbaseName;
  QLineEdit * lDatbaseToken;
  QLineEdit * lElogIP;
  QLineEdit * lElogUser;
  QLineEdit * lElogPWD;

  QCheckBox * chkSaveSubFolder;
  bool isSaveSubFolder;

  QStringList existGitBranches;

  QString programPath;
  QString analysisPath;
  QString masterExpDataPath;
  QString expDataPath;
  QString rawDataPath;
  QString rootDataPath;
  QString IPListStr;
  QStringList IPList;
  QString DatabaseIP;
  QString DatabaseName;
  QString DatabaseToken;
  QString ElogIP;
  QString ElogUser;
  QString ElogPWD;

  //@------ experiment settings
  bool isGitExist;
  bool useGit;
  QString expName;
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

  QString maskText(const QString &password) {
    if (password.length() <= 3) {
      return password; // No masking needed for short passwords
    } else if (password.length() <= 10) {
      QString maskedPassword = password.left(3);
      maskedPassword += QString("*").repeated(password.length() - 3);
      return maskedPassword;
    } else {
      return password.left(3) + QString("*").repeated(7);
    }
  }

};


#endif // MAINWINDOW_H
