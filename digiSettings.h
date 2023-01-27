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

class DigiSettings : public QWidget{
  Q_OBJECT

public:
  DigiSettings(Digitizer2Gen * digi, unsigned short nDigi, QWidget * parent = nullptr);
  ~DigiSettings();

private slots:
  void onTriggerClick(int haha){
    
    unsigned short ch = haha/100;
    unsigned short ch2 = haha - ch*100;

    if(bnClickStatus[ch][ch2]){
      bn[ch][ch2]->setStyleSheet(""); 
      bnClickStatus[ch][ch2] = false;
    }else{
      bn[ch][ch2]->setStyleSheet("background-color: red;"); 
      bnClickStatus[ch][ch2] = true;
    }
  }

signals:

private:
  
  Digitizer2Gen * digi;
  unsigned short nDigi;

  QPushButton *bn[64][64];
  bool bnClickStatus[64][64];

};



#endif