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

  void onChannelonOff(int haha){
    qDebug() << haha;

    if( haha == 64){
      if( cbCh[64]->isChecked() ){
        for( int i = 0 ; i < digi->GetNChannels() ; i++){
          cbCh[i]->setChecked(true);
        }
      }else{
        for( int i = 0 ; i < digi->GetNChannels() ; i++){
          cbCh[i]->setChecked(false);
        }
      }
    }
  }

signals:

private:
  
  Digitizer2Gen * digi;
  unsigned short nDigi;

  QPushButton *bn[MaxNumberOfChannel][MaxNumberOfChannel];
  bool bnClickStatus[MaxNumberOfChannel][MaxNumberOfChannel];

  QCheckBox * cbCh[MaxNumberOfChannel + 1]; // index = 64 is for all channels

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

};



#endif