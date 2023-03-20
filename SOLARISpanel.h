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

  void SendLogMsg(const QString str);

private:
  void CreateSpinBoxGroup(int SettingID, QList<int> detID, QGridLayout * &layout, int row, int col);

  Digitizer2Gen ** digi;
  unsigned short nDigi;
  std::vector<std::vector<int>> mapping;
  QStringList detType;
  std::vector<int> detMaxID;

  int nDigiMapping;
  std::vector<int> nChMapping;

  QLineEdit **** leDisplay; // [SettingID][DigiID][ChID]
  RSpinBox **** sbSetting;

  bool enableSignalSlot;

};

#endif