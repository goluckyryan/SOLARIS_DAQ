#ifndef DigiSettings_H
#define DigiSettings_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QScrollArea>
#include <QTabWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QTableWidget>
#include <QDebug>
#include <QPushButton>
#include <QFrame>
#include <QSignalMapper>


#include "ClassDigitizer2Gen.h"
#include "CustomWidgets.h"

#define MaxNumberOfDigitizer 20

//^#######################################################
class DigiSettingsPanel : public QWidget{
  Q_OBJECT

public:
  DigiSettingsPanel(Digitizer2Gen ** digi, unsigned short nDigi, QWidget * parent = nullptr);
  ~DigiSettingsPanel();

private slots:  
  void onTriggerClick(int haha);
  void ReadTriggerMap();

  void SaveSettings();
  void LoadSettings();
  void SetDefaultPHASettigns();
  void RefreshSettings(); // this read digitizer and ShowSettingToPanel

public slots:
  void ShowSettingsToPanel();
  void EnableControl();

signals:

  void SendLogMsg(const QString &msg);
  void UpdateScopeSetting();

private:
  
  Digitizer2Gen ** digi;
  unsigned short nDigi;
  unsigned short ID; // index for digitizer;

  //------------ Layout/GroupBox
  QGroupBox * digiBox;
  QGroupBox * VGABox;
  QGroupBox * testPulseBox;

  QGroupBox * box0;
  QGroupBox * box1;
  QGroupBox * box3;
  QGroupBox * box4;
  QGroupBox * box5;
  QGroupBox * box6;

  QTabWidget * inputTab;
  QTabWidget * trapTab;
  QTabWidget * probeTab;
  QTabWidget * otherTab;
  QTabWidget * triggerTab;
  QTabWidget * triggerMapTab;
  QTabWidget * ICTab; // inquiry and copy

  QGroupBox * icBox1;
  QGroupBox * icBox2;


  bool enableSignalSlot;

  //---------------- Inquiry and copy
  RComboBox * cbIQDigi;
  RComboBox * cbBdSettings;
  RComboBox * cbIQCh;
  RComboBox * cbChSettings;

  QLineEdit * leBdSettingsType;
  QLineEdit * leBdSettingsRead;
  QLineEdit * leBdSettingsUnit;
  RComboBox * cbBdAns;
  RSpinBox *  sbBdSettingsWrite;
  QLineEdit * leBdSettingsWrite;

  QLineEdit * leChSettingsType;
  QLineEdit * leChSettingsRead;
  QLineEdit * leChSettingsUnit;
  RComboBox * cbChAns;
  RSpinBox *  sbChSettingsWrite;
  QLineEdit * leChSettingsWrite;

  RComboBox * cbCopyDigiFrom;
  RComboBox * cbCopyDigiTo;
  QRadioButton * rbCopyChFrom[MaxNumberOfChannel];
  QCheckBox * chkChTo[MaxNumberOfChannel];

  QPushButton * pbCopyChannel;
  QPushButton * pbCopyBoard;
  QPushButton * pbCopyDigi;
  
  //------------ status
  QLineEdit * leInfo[MaxNumberOfChannel][12];
  QPushButton * LEDStatus[MaxNumberOfDigitizer][19];
  QPushButton * ACQStatus[MaxNumberOfDigitizer][19];
  QLineEdit * leTemp[MaxNumberOfDigitizer][8];

  //------------- buttons
  QPushButton * bnReadSettngs[MaxNumberOfChannel];
  QPushButton * bnResetBd[MaxNumberOfChannel];
  QPushButton * bnDefaultSetting[MaxNumberOfChannel];
  QPushButton * bnSaveSettings[MaxNumberOfChannel];
  QPushButton * bnLoadSettings[MaxNumberOfChannel];
  QPushButton * bnClearData[MaxNumberOfChannel];
  QPushButton * bnArmACQ[MaxNumberOfChannel];
  QPushButton * bnDisarmACQ[MaxNumberOfChannel];
  QPushButton * bnSoftwareStart[MaxNumberOfChannel];
  QPushButton * bnSoftwareStop[MaxNumberOfChannel];

  //-------------- board settings
  RComboBox * cbbClockSource[MaxNumberOfDigitizer];
  QCheckBox * ckbStartSource[MaxNumberOfDigitizer][5];
  QCheckBox * ckbGlbTrgSource[MaxNumberOfDigitizer][5];
  RComboBox * cbbTrgOut[MaxNumberOfDigitizer];
  RComboBox * cbbGPIO[MaxNumberOfDigitizer];
  RComboBox * cbbAutoDisarmAcq[MaxNumberOfDigitizer];
  RComboBox * cbbBusyIn[MaxNumberOfDigitizer];
  RComboBox * cbbStatEvents[MaxNumberOfDigitizer];
  RComboBox * cbbSyncOut[MaxNumberOfDigitizer];
  RComboBox * cbbBoardVetoSource[MaxNumberOfDigitizer];
  RSpinBox  * dsbBdVetoWidth[MaxNumberOfDigitizer];
  RComboBox * cbbBdVetoPolarity[MaxNumberOfDigitizer];
  RComboBox * cbbIOLevel[MaxNumberOfDigitizer];
  RSpinBox  * spbRunDelay[MaxNumberOfDigitizer];
  RSpinBox * dsbVolatileClockOutDelay[MaxNumberOfDigitizer];
  RSpinBox * dsbClockOutDelay[MaxNumberOfDigitizer];

  //-------------- Test pulse
  RSpinBox * dsbTestPuslePeriod[MaxNumberOfDigitizer];
  RSpinBox * dsbTestPusleWidth[MaxNumberOfDigitizer];
  RSpinBox * spbTestPusleLowLevel[MaxNumberOfDigitizer];
  RSpinBox * spbTestPusleHighLevel[MaxNumberOfDigitizer];

  //-------------- VGA
  RSpinBox * VGA[MaxNumberOfDigitizer][4];

  //--------------- trigger map
  RComboBox * cbAllEvtTrigger;
  RComboBox * cbAllWaveTrigger;
  RComboBox * cbAllCoinMask;
  RComboBox * cbAllAntiCoinMask;
  RSpinBox  * sbAllCoinLength;
  QPushButton * trgMap[MaxNumberOfChannel][MaxNumberOfChannel];
  bool trgMapClickStatus[MaxNumberOfChannel][MaxNumberOfChannel];

  //--------------- Channel status
  QPushButton * chStatus[MaxNumberOfDigitizer][MaxNumberOfChannel][9];
  QLineEdit   * chGainFactor[MaxNumberOfDigitizer][MaxNumberOfChannel];
  QLineEdit   * chADCToVolts[MaxNumberOfDigitizer][MaxNumberOfChannel];

  //--------------- Channel settings
  RComboBox * cbChPick[MaxNumberOfDigitizer];

  RComboBox * cbbOnOff[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbRecordLength[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbPreTrigger[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbDCOffset[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbThreshold[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RComboBox * cbbParity[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbWaveSource[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbWaveRes[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbWaveSave[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RSpinBox  * spbInputRiseTime[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbTriggerGuard[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RSpinBox  * spbTrapRiseTime[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbTrapFlatTop[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbTrapPoleZero[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RSpinBox  * spbPeaking[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbBaselineGuard[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbPileupGuard[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RComboBox * cbbPeakingAvg[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbBaselineAvg[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbFineGain[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbLowFilter[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RComboBox * cbbAnaProbe0[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbAnaProbe1[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RComboBox * cbbDigProbe0[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbDigProbe1[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbDigProbe2[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbDigProbe3[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  
  RComboBox * cbbEvtTrigger[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbWaveTrigger[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbChVetoSrc[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RComboBox * cbbEventSelector[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbWaveSelector[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RComboBox * cbbCoinMask[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RComboBox * cbbAntiCoinMask[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RSpinBox  * spbCoinLength[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbADCVetoWidth[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  RSpinBox  * spbEnergySkimLow[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  RSpinBox  * spbEnergySkimHigh[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];


  //-------------------------
  QLineEdit * leSettingFile[MaxNumberOfDigitizer];

  
  void SetStartSource();
  void SetGlobalTriggerSource();

  void SetupShortComboBox(RComboBox * &cbb, Reg para);

  void SetupComboBox(RComboBox * &cbb, const Reg para, int ch_index, bool isMaster, QString labelTxt, QGridLayout * layout, int row, int col, int srow = 1, int scol = 1);
  void SetupSpinBox(RSpinBox * &spb, const Reg para, int ch_index, bool isMaster,  QString labelTxt, QGridLayout * layout, int row, int col, int srow = 1, int scol = 1);

  void SyncComboBox(RComboBox *(&cbb)[][MaxNumberOfChannel+1], int ch);
  void SyncSpinBox(RSpinBox *(&spb)[][MaxNumberOfChannel+1], int ch);

  void SetupComboBoxTab(RComboBox *(&cbb)[][MaxNumberOfChannel+1], const Reg para, QString text, QTabWidget * tabWidget, int iDigi, int nChannel, int nCol = 4);
  void SetupSpinBoxTab(RSpinBox *(&spb)[][MaxNumberOfChannel+1], const Reg para, QString text, QTabWidget * tabWidget, int iDigi, int nChannel);

  void FillComboBoxValueFromMemory(RComboBox * &cbb, const Reg para, int ch_index = -1);
  void FillSpinBoxValueFromMemory(RSpinBox * &spb, const Reg para, int ch_index = -1 );

  void ReadBoardSetting(int cbIndex);
  void ReadChannelSetting(int cbIndex);

  void CheckRadioAndCheckedButtons();
  bool CopyChannelSettings(int digiFrom, int chFrom, int digiTo, int chTo);
  bool CopyBoardSettings();
  bool CopyWholeDigitizer();

};



#endif