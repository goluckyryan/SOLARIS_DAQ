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
#include <QWheelEvent>

#include "ClassDigitizer2Gen.h"

#define MaxNumberOfDigitizer 20


class RComboBox : public QComboBox{
  public : 
    RComboBox(QWidget * parent = nullptr): QComboBox(parent){
      setFocusPolicy(Qt::StrongFocus);
    }
  protected:
    void wheelEvent(QWheelEvent * event) override{ event->ignore(); }
};

class RSpinBox : public QDoubleSpinBox{
  Q_OBJECT
  public : 
    RSpinBox(QWidget * parent = nullptr, int decimal = 0): QDoubleSpinBox(parent){
      setFocusPolicy(Qt::StrongFocus);
      setDecimals(decimal);
    }
  signals:
    void returnPressed();
  protected:
    void wheelEvent(QWheelEvent * event) override{ event->ignore(); }

    void keyPressEvent(QKeyEvent * event) override{
      if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit returnPressed();
      } else {
        QDoubleSpinBox::keyPressEvent(event);
      }
    }
};

//^#######################################################
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
  QGroupBox * testPulseBox;
  RSpinBox * dsbTestPuslePeriod[MaxNumberOfDigitizer];
  RSpinBox * dsbTestPusleWidth[MaxNumberOfDigitizer];
  RSpinBox * spbTestPusleLowLevel[MaxNumberOfDigitizer];
  RSpinBox * spbTestPusleHighLevel[MaxNumberOfDigitizer];

  //-------------- VGA
  QGroupBox * VGABox;
  RSpinBox * VGA[MaxNumberOfDigitizer][4];

  //---------------
  QPushButton *bn[MaxNumberOfChannel][MaxNumberOfChannel];
  bool bnClickStatus[MaxNumberOfChannel][MaxNumberOfChannel];

  //--------------- Channel settings
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

  void SetupShortComboBox(RComboBox * cbb, Reg para);

  void SetupComboBox(RComboBox * &cbb, const Reg para, int ch_index, bool isMaster, QString labelTxt, QGridLayout * layout, int row, int col, int srow = 1, int scol = 1);
  void SetupSpinBox(RSpinBox * &spb, const Reg para, int ch_index, QString labelTxt, QGridLayout * layout, int row, int col, int srow = 1, int scol = 1);

  void SyncComboBox(RComboBox *(&cbb)[][MaxNumberOfChannel+1], int ch);
  void SyncSpinBox(RSpinBox *(&spb)[][MaxNumberOfChannel+1], int ch);

  void SetupComboBoxTab(RComboBox *(&cbb)[][MaxNumberOfChannel+1], const Reg para, QString text, QTabWidget * tabWidget, int iDigi, int nChannel, int nCol = 4);
  void SetupSpinBoxTab(RSpinBox *(&spb)[][MaxNumberOfChannel+1], const Reg para, QString text, QTabWidget * tabWidget, int iDigi, int nChannel);

  void FillComboBoxValueFromMemory(RComboBox * &cbb, const Reg para, int ch_index = -1);
  void FillSpinBoxValueFromMemory(RSpinBox * &spb, const Reg para, int ch_index = -1 );




};



#endif