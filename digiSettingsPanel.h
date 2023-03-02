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
#include <QCheckBox>
#include <QComboBox>
#include <QTableWidget>
#include <QDebug>
#include <QPushButton>
#include <QFrame>
#include <QSignalMapper>

#include "ClassDigitizer2Gen.h"

#define MaxNumberOfDigitizer 20

class DigiSettingsPanel : public QWidget{
  Q_OBJECT

public:
  DigiSettingsPanel(Digitizer2Gen ** digi, unsigned short nDigi, QWidget * parent = nullptr);
  ~DigiSettingsPanel();

private slots:

  
  void onTriggerClick(int haha);

  void RefreshSettings();
  void SaveSettings();
  void LoadSettings();

signals:

  void sendLogMsg(const QString &msg);

private:
  
  Digitizer2Gen ** digi;
  unsigned short nDigi;
  unsigned short ID; // index for digitizer;

  void ShowSettingsToPanel();

  bool enableSignalSlot;

  //------------ status
  QLineEdit * leInfo[MaxNumberOfChannel][12];
  QPushButton * LEDStatus[MaxNumberOfDigitizer][19];
  QPushButton * ACQStatus[MaxNumberOfDigitizer][19];
  QLineEdit * leTemp[MaxNumberOfDigitizer][8];

  //-------------- board settings
  QComboBox * cbbClockSource[MaxNumberOfDigitizer];
  QCheckBox * ckbStartSource[MaxNumberOfDigitizer][5];
  QCheckBox * ckbGlbTrgSource[MaxNumberOfDigitizer][5];
  QComboBox * cbbTrgOut[MaxNumberOfDigitizer];
  QComboBox * cbbGPIO[MaxNumberOfDigitizer];
  QComboBox * cbbAutoDisarmAcq[MaxNumberOfDigitizer];
  QComboBox * cbbBusyIn[MaxNumberOfDigitizer];
  QComboBox * cbbStatEvents[MaxNumberOfDigitizer];
  QComboBox * cbbSyncOut[MaxNumberOfDigitizer];
  QComboBox * cbbBoardVetoSource[MaxNumberOfDigitizer];
  QSpinBox  * spbBdVetoWidth[MaxNumberOfDigitizer];
  QComboBox * cbbBdVetoPolarity[MaxNumberOfDigitizer];
  QComboBox * cbbIOLevel[MaxNumberOfDigitizer];
  QSpinBox  * spbRunDelay[MaxNumberOfDigitizer];
  QDoubleSpinBox * dsbVolatileClockOutDelay[MaxNumberOfDigitizer];
  QDoubleSpinBox * dsbClockOutDelay[MaxNumberOfDigitizer];

  //-------------- Test pulse
  QGroupBox * testPulseBox;
  QDoubleSpinBox * dsbTestPuslePeriod[MaxNumberOfDigitizer];
  QDoubleSpinBox * dsbTestPusleWidth[MaxNumberOfDigitizer];
  QSpinBox * spbTestPusleLowLevel[MaxNumberOfDigitizer];
  QSpinBox * spbTestPusleHighLevel[MaxNumberOfDigitizer];

  //---------------

  QPushButton *bn[MaxNumberOfChannel][MaxNumberOfChannel];
  bool bnClickStatus[MaxNumberOfChannel][MaxNumberOfChannel];

  //--------------- Channel settings
  QComboBox * cbbOnOff[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QSpinBox  * spbRecordLength[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbPreTrigger[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbDCOffset[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbThreshold[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QComboBox * cbbParity[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbWaveSource[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbWaveRes[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbWaveSave[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QSpinBox  * spbInputRiseTime[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbTriggerGuard[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QSpinBox  * spbTrapRiseTime[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbTrapFlatTop[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbTrapPoleZero[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QSpinBox  * spbPeaking[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbBaselineGuard[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbPileupGuard[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QComboBox * cbbPeakingAvg[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbBaselineAvg[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbFineGain[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbLowFilter[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QComboBox * cbbAnaProbe0[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbAnaProbe1[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QComboBox * cbbDigProbe0[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbDigProbe1[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbDigProbe2[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbDigProbe3[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  
  QComboBox * cbbEvtTrigger[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbWaveTrigger[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbChVetoSrc[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QLineEdit * leTriggerMask[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QComboBox * cbbEventSelector[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbWaveSelector[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QComboBox * cbbCoinMask[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QComboBox * cbbAntiCoinMask[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QSpinBox  * spbCoinLength[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbADCVetoWidth[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];

  QSpinBox  * spbEnergySkimLow[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];
  QSpinBox  * spbEnergySkimHigh[MaxNumberOfDigitizer][MaxNumberOfChannel + 1];


  //-------------------------
  QLineEdit * leSettingFile[MaxNumberOfDigitizer];
  
  void SetStartSource();
  void SetGlobalTriggerSource();

  void SetupShortComboBox(QComboBox * cbb, Reg para);

  void SetupComboBox(QComboBox * &cbb, const Reg para, int ch_index, bool isMaster, QString labelTxt, QGridLayout * layout, int row, int col, int srow = 1, int scol = 1);
  void SetupSpinBox(QSpinBox * &spb, const Reg para, int ch_index, QString labelTxt, QGridLayout * layout, int row, int col, int srow = 1, int scol = 1);

  void SyncComboBox(QComboBox *(&cbb)[][MaxNumberOfChannel+1], int ch);
  void SyncSpinBox(QSpinBox *(&spb)[][MaxNumberOfChannel+1], int ch);

  void SetupComboBoxTab(QComboBox *(&cbb)[][MaxNumberOfChannel+1], const Reg para, QString text, QTabWidget * tabWidget, int iDigi, int nChannel, int nCol = 4);
  void SetupSpinBoxTab(QSpinBox *(&spb)[][MaxNumberOfChannel+1], const Reg para, QString text, QTabWidget * tabWidget, int iDigi, int nChannel);

  void FillComboBoxValueFromMemory(QComboBox * &cbb, const Reg para, int ch_index = -1);
  template<typename T> void FillSpinBoxValueFromMemory(T * &spb, const Reg para, int ch_index = -1 );




};



#endif