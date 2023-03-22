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

class SOLARISpanel : public QWidget{
  Q_OBJECT

public: 
  SOLARISpanel(Digitizer2Gen ** digi, 
               unsigned short nDigi, 
               std::vector<std::vector<int>> mapping, 
               QStringList detType, 
               std::vector<int> detMaxID, 
               QWidget * parent = nullptr);
  ~SOLARISpanel();

private slots:


public slots:
  void UpdatePanel();
  void UpdateThreshold();

signals:
  
  //TODO
  void UpdateSettingsPanel();
  void UpdateScopePanel();
  void SendLogMsg(const QString str);

private:
  void CreateDetGroup(int SettingID, QList<int> detID, QGridLayout * &layout, int row, int col);

  Digitizer2Gen ** digi;
  unsigned short nDigi;
  std::vector<std::vector<int>> mapping;
  QStringList detType;
  std::vector<int> nDet; // number of disting detector
  std::vector<int> detMaxID;
  QList<QList<int>> detIDList; // 1-D array of { detID,  (Digi << 8 ) + ch}

  int FindDetTypID(QList<int> detIDListElement);

  int nDigiMapping; /// mapping.size()
  std::vector<int> nChMapping; /// mapping[i].size(), NON -1 

  QCheckBox * chkAll; // checkBox for all setting on that tab;
  QCheckBox * chkAlle;
  QCheckBox * chkAllxf;
  QCheckBox * chkAllxn;

  QLineEdit **** leDisplay; // [SettingID][DigiID][ChID]
  RSpinBox  **** sbSetting;
  QCheckBox **** chkOnOff;

  RComboBox *** cbTrigger; //[detTypeID][detID] for array only

  bool enableSignalSlot;

};

#endif