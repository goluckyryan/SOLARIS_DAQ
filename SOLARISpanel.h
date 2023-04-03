#ifndef SOLARIS_PANEL_H
#define SOLARIS_PANEL_H

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
#include "macro.h"

#define MaxDetGroup 10
#define MaxDetID 60
#define MaxSettingItem  3

class SOLARISpanel : public QWidget{
  Q_OBJECT

public: 
  SOLARISpanel(Digitizer2Gen ** digi, 
               unsigned short nDigi, 
               QString analysisPath,
               std::vector<std::vector<int>> mapping, 
               QStringList detType, 
               QStringList detGroupName,
               std::vector<int> detGroupID,
               std::vector<int> detMaxID, 
               QWidget * parent = nullptr);
  ~SOLARISpanel();

private slots:

  void RefreshSettings();
  void SaveSettings();
  void LoadSettings();

public slots:
  void UpdateThreshold();
  void UpdatePanelFromMemory();

signals:

  void UpdateOtherPanels();
  void SendLogMsg(const QString str);

private:
  void CreateDetGroup(int SettingID, QList<int> detIDArray, QGridLayout * &layout, int row, int col);

  Digitizer2Gen ** digi;
  unsigned short nDigi;
  std::vector<std::vector<int>> mapping;
  QStringList detTypeNameList;
  std::vector<int> nDetinType;
  std::vector<int> detMaxID;
  QStringList detGroupNameList;
  std::vector<int> detGroupID;
  std::vector<int> nDetinGroup; 
  QList<QList<int>> detIDArrayList; // 1-D array of { detID,  (Digi << 8 ) + ch}

  QString digiSettingPath;

  int FindDetTypeID(int detID);
  int FindDetGroup(int detID);

  RSpinBox * sbCoinTime;

  QCheckBox * chkAll[MaxDetGroup][MaxSettingItem]; // checkBox for all setting on that tab;

  QGroupBox * groupBox[MaxDetGroup][MaxSettingItem][MaxDetID];

  QLineEdit * leDisplay[MaxSettingItem][MaxNumberOfDigitizer][MaxNumberOfChannel]; // [SettingID][DigiID][ChID]
  RSpinBox  * sbSetting[MaxSettingItem][MaxNumberOfDigitizer][MaxNumberOfChannel];
  QCheckBox * chkOnOff[MaxSettingItem][MaxNumberOfDigitizer][MaxNumberOfChannel];

  RComboBox * cbTrigger[MaxDetGroup][MaxDetID]; //[detTypeID][detID] for array only

  bool enableSignalSlot;

};

#endif