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
  void onChannelonOff(int haha);

  void onReset();
  void onDefault();
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

  QLineEdit * leInfo[MaxNumberOfChannel][12];
  QPushButton * LEDStatus[MaxNumberOfDigitizer][19];
  QPushButton * ACQStatus[MaxNumberOfDigitizer][19];



  QPushButton *bn[MaxNumberOfChannel][MaxNumberOfChannel];
  bool bnClickStatus[MaxNumberOfChannel][MaxNumberOfChannel];

  QCheckBox * cbCh[MaxNumberOfDigitizer][MaxNumberOfChannel + 1]; // index = 64 is for all channels

  QSpinBox  * sbRecordLength[MaxNumberOfChannel + 1];
  QSpinBox  * sbPreTrigger[MaxNumberOfChannel + 1];
  
  QComboBox * cmbWaveRes[MaxNumberOfChannel + 1];

  QComboBox * cmbAnaProbe0[MaxNumberOfChannel + 1];
  QComboBox * cmbAnaProbe1[MaxNumberOfChannel + 1];
  QComboBox * cmbDigProbe0[MaxNumberOfChannel + 1];
  QComboBox * cmbDigProbe1[MaxNumberOfChannel + 1];
  QComboBox * cmbDigProbe2[MaxNumberOfChannel + 1];
  QComboBox * cmbDigProbe3[MaxNumberOfChannel + 1];
  
  QComboBox * cmbEvtTrigger[MaxNumberOfChannel + 1];
  QComboBox * cmbWaveTrigger[MaxNumberOfChannel + 1];
  QComboBox * cmbWaveSave[MaxNumberOfChannel + 1];
  QComboBox * cmbWaveSource[MaxNumberOfChannel + 1];
  
  QComboBox * cmbChVetoSrc[MaxNumberOfChannel + 1];
  QSpinBox  * sbChADCVetoWidth[MaxNumberOfChannel + 1];

  QLineEdit * leSettingFile[MaxNumberOfDigitizer];
  




};



#endif