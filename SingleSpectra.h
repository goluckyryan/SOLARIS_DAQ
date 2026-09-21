#ifndef SINGLE_SPECTR_H
#define SINGLE_SPECTR_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QVector>
#include <QRandomGenerator>

#include <atomic>

#include "macro.h"
#include "ClassDigitizer2Gen.h"
#include "CustomThreads.h"
#include "CustomWidgets.h"
#include "Histogram1D.h"
#include "Histogram2D.h"

class HistWorker; // Forward declaration

#define MaxNumberOfPane 16 // the biggest split is 4x4

//^====================================================
//^ One cell of the histogram grid. The plot widget itself is NOT owned here;
//^ hist[][] / hist2D[] own them and a pane only mounts one at a time.
//^====================================================
struct HistPane{
  QWidget     * box;         // container added to histLayout
  QVBoxLayout * boxLayout;   // index 0 = header row, index 1 = plot (or placeholder)
  QLabel      * placeholder; // shown when the pane has no source
  RComboBox   * cbDigi;
  RComboBox   * cbCh;
  int digiID;                // -1 = empty pane
  int chID;                  // -1 = empty; == digi[digiID]->GetNChannels() => 2D overview
  int savedDigiID;           // remembered while the pane is hidden by a smaller split
  int savedChID;
};

//^====================================================
//^====================================================
class SingleSpectra : public QMainWindow{
  Q_OBJECT

public:
  SingleSpectra(Digitizer2Gen ** digi, unsigned int nDigi, QString rawDataPath, QMainWindow * parent = nullptr);
  ~SingleSpectra();

  void ClearInternalDataCount();
  // void SetFillHistograms(bool onOff) { fillHistograms = onOff;}
  // bool IsFillHistograms() const {return fillHistograms;}

  void LoadSetting();
  void SaveSetting();

  void SetMaxFillTime(unsigned short milliSec) { maxFillTimeinMilliSec = milliSec;}
  unsigned short GetMaxFillTime() const {return maxFillTimeinMilliSec;};

  QVector<int> generateNonRepeatedCombination(int size);

  void ReplotHistograms();

signals:
  // void startWorkerTimer(int interval);
  // void stopWorkerTimer();

public slots:
  void FillHistograms();
  void startTimer(){
    // printf("timer start\n");
    timer->start(maxFillTimeinMilliSec);
    // emit startWorkerTimer(maxFillTimeinMilliSec);
  }
  void stopTimer(){
    // printf("timer stop\n");
    timer->stop();
    // emit stopWorkerTimer();
    isFillingHistograms = false; // this will also break the FillHistogram do-loop
    ClearInternalDataCount();
  }

private:

  Digitizer2Gen ** digi;
  unsigned int nDigi;

  long lastFilledIndex[MaxNumberOfDigitizer][MaxNumberOfChannel]; // ring-buffer fill index per channel
  bool histVisibility[MaxNumberOfDigitizer][MaxNumberOfChannel];
  bool hist2DVisibility[MaxNumberOfDigitizer];

  std::atomic<bool> isFillingHistograms;
  std::atomic<bool> suspendFilling;  // set while the GUI thread re-parents plot widgets
  Histogram1D * hist[MaxNumberOfDigitizer][MaxNumberOfChannel];
  Histogram2D * hist2D[MaxNumberOfDigitizer];

  QCheckBox * chkIsFillHistogram;

  QGroupBox * histBox;
  QGridLayout * histLayout;

  //^---- the split grid
  HistPane pane[MaxNumberOfPane];
  RComboBox * cbSplit;
  int splitRows, splitCols;

  void BuildPanes();
  void RebuildChannelCombo(int p);
  void ApplySplit(int rows, int cols, bool autoAssign = true);
  void SetPaneSource(int p, int digiID, int chID); // caller must fence with suspendFilling
  void PaneSelectionChanged(int p);                // fenced wrapper used by the pane combos
  void RefreshComboEnables();
  void UpdateVisibilityFlags();
  void UpdateRefreshRate();
  void WaitForFillToDrain();
  int  NumberOfPanes() const { return splitRows * splitCols; }

  //^---- replot throttle
  QCheckBox * chkAutoRefresh;
  RSpinBox  * sbRefresh;
  QLabel    * lbEffRefresh;
  int replotDivider;
  int replotCounter;

  QString settingPath;

  unsigned short maxFillTimeinMilliSec;

  bool isSignalSlotActive;

  QThread * workerThread;
  HistWorker * histWorker;
  QTimer * timer;

};

// //^#======================================================== HistWorker
class HistWorker : public QObject{
  Q_OBJECT
public:
  HistWorker(SingleSpectra * parent): SS(parent){}

public slots:
  void FillHistograms(){
    SS->FillHistograms();
    emit workDone();
  }

signals:
  void workDone();

private:
  SingleSpectra * SS;
};

#endif
