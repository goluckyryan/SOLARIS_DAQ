#ifndef Scope_H
#define Scope_H


#include <QMainWindow>
#include <QChart>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLineSeries>

#include "ClassDigitizer2Gen.h"
#include "manyThread.h"

class Scope : public QMainWindow{
  Q_OBJECT

public:
  Scope(Digitizer2Gen ** digi, unsigned int nDigi, ReadDataThread ** readDataThread, QMainWindow * parent = nullptr);
  ~Scope();


private slots:

  void ReadScopeSettings(int iDigi, int ch);
  void StartScope();
  void StopScope();
  void UpdateScope();
  void ScopeControlOnOff(bool on);
  void ScopeReadSpinBoxValue(int iDigi, int ch, QSpinBox *sb, std::string digPara);
  void ScopeReadComboBoxValue(int iDigi, int ch, QComboBox *cb, std::string digPara);
  void ScopeMakeSpinBox(QSpinBox * sb, QString str,  QGridLayout* layout, int row, int col, int min, int max, int step, std::string digPara);
  void ScopeMakeComoBox(QComboBox * cb, QString str, QGridLayout* layout, int row, int col, std::string digPara);
  void ProbeChange(QComboBox * cb[], const int size);

  void closeEvent(QCloseEvent * event){
    StopScope();  
    event->accept();
  }

signals:

private:

  Digitizer2Gen ** digi;
  unsigned short nDigi;

  ReadDataThread ** readDataThread;   
  UpdateTraceThread * updateTraceThread;

  QChart      * plot;
  QLineSeries * dataTrace[6];
  QComboBox   * cbScopeDigi;
  QComboBox   * cbScopeCh;
  QPushButton * bnScopeReset;
  QPushButton * bnScopeReadSettings;
  
  
  QPushButton * bnScopeStart;
  QPushButton * bnScopeStop;
  
  QComboBox   * cbAnaProbe[2];
  QComboBox   * cbDigProbe[4];
  QSpinBox * sbRL; // record length
  QSpinBox * sbPT; // pre trigger
  QSpinBox * sbDCOffset;
  QSpinBox * sbThreshold;
  QSpinBox * sbTimeRiseTime;
  QSpinBox * sbTimeGuard;
  QSpinBox * sbTrapRiseTime;
  QSpinBox * sbTrapFlatTop;
  QSpinBox * sbTrapPoleZero;
  QSpinBox * sbEnergyFineGain;
  QSpinBox * sbTrapPeaking;
  QComboBox * cbPolarity;
  QComboBox * cbWaveRes;
  QComboBox * cbTrapPeakAvg;

  QSpinBox * sbBaselineGuard;
  QSpinBox * sbPileUpGuard;
  QComboBox * cbBaselineAvg;
  QComboBox * cbLowFreqFilter;

  bool allowChange;

};

#endif