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
#include <QHash>

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

  /// Panel slot of a detector within its own detector type, or -1 if it has none.
  ///
  /// groupBox[][][] and cbTrigger[][] are MaxDetID wide, but Mapping.h numbers detectors by type:
  /// the production file puts the recoils at 300-307, so groupBox[1][s][300] silently aliases
  /// another group's entry, and a type based higher up runs off the end outright. The slot is the
  /// detID minus the start of its type -- the same offset the consolidation loop uses to decide
  /// that e-3, xf-103 and xn-203 are one detector. Built once in the constructor.
  QHash<int,int> detSlotMap;
  int DetSlot(int detID) const { return detSlotMap.value(detID, -1); }

  RSpinBox * sbCoinTime;

  /// Explicitly zeroed: not every (group, slot, setting) combination gets a widget -- cbTrigger
  /// is only built for the threshold tab, and a detector past MaxDetID gets none at all -- so the
  /// unused entries have to read as null rather than as indeterminate.
  QCheckBox * chkAll[MaxDetGroup][MaxSettingItem] = {}; // checkBox for all setting on that tab;

  QGroupBox * groupBox[MaxDetGroup][MaxSettingItem][MaxDetID] = {};

  QLineEdit * leDisplay[MaxSettingItem][MaxNumberOfDigitizer][MaxNumberOfChannel] = {}; // [SettingID][DigiID][ChID]
  RSpinBox  * sbSetting[MaxSettingItem][MaxNumberOfDigitizer][MaxNumberOfChannel] = {};
  QCheckBox * chkOnOff[MaxSettingItem][MaxNumberOfDigitizer][MaxNumberOfChannel] = {};

  RComboBox * cbTrigger[MaxDetGroup][MaxDetID] = {}; //[detGroup][panel slot] for array only

  bool enableSignalSlot;

};

#endif