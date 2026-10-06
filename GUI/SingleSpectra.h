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
#include <memory>
#include <vector>

#include "macro.h"
#include "ClassDigitizer2Gen.h"
#include "DigiManager.h"
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
  int chID;                  // -1 = empty; == GetNChannels(digiID) => 2D overview
  int savedDigiID;           // remembered while the pane is hidden by a smaller split
  int savedChID;
};

//^====================================================
//^====================================================
class SingleSpectra : public QMainWindow{
  Q_OBJECT

public:
  SingleSpectra(DigiManager * digiManager, unsigned int nDigi, QString rawDataPath, QMainWindow * parent = nullptr);
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
    /// Sync the fill cursor before the timer runs, same reasoning as the chkIsFillHistogram
    /// connection: everything the ring collected since the last sync (previous-run tail, or the
    /// whole backlog of a run during which filling was off) would otherwise be replayed into
    /// the plots as one sudden burst on the first pass.
    suspendFilling = true;
    WaitForFillToDrain();
    ClearInternalDataCount();
    suspendFilling = false;
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

  DigiManager * digiManager;
  unsigned int nDigi;

  long lastFilledIndex[MaxNumberOfDigitizer][MaxNumberOfChannel]; // ring-buffer fill index per channel

  /// Which plots are mounted in a pane. Written by UpdateVisibilityFlags() on the GUI thread and
  /// read by the fill worker, hence atomic. They gate the fill: only what is on screen is drained.
  std::atomic<bool> histVisibility[MaxNumberOfDigitizer][MaxNumberOfChannel];
  std::atomic<bool> hist2DVisibility[MaxNumberOfDigitizer];

  std::atomic<bool> isFillingHistograms;
  std::atomic<bool> suspendFilling;  // set while the GUI thread re-parents plot widgets

  /// Mirrors of GUI state the worker used to read straight off the widgets. QWidget accessors are
  /// not safe to call from another thread, so the GUI writes these and the worker reads them.
  std::atomic<bool> fillEnabled;     // mirrors chkIsFillHistogram
  std::atomic<bool> windowVisible;   // mirrors isVisible(), via show/hideEvent

  Histogram1D * hist[MaxNumberOfDigitizer][MaxNumberOfChannel];
  Histogram2D * hist2D[MaxNumberOfDigitizer];

  //^==================================================================== counters
  //^ The worker only ever increments these; the GUI alone touches the widgets, publishing through
  //^ Histogram1D::SetBinContents / Histogram2D::SetCellContents. Fill() mutates QCPItemText and
  //^ the colormap array, which the GUI thread is concurrently drawing -- see the notes on those
  //^ two methods. Same split QtHistRegistry (Analyzer.h) already uses for the online analyzer.
  struct H1Counts {
    int    nBin = 0;
    double lo = 0, hi = 0, dx = 1;
    int    nSeries = 1;                                   ///< 2 under PSD: long gate + short gate
    std::unique_ptr<std::atomic<uint32_t>[]> c[2];
    std::atomic<uint64_t> total{0}, under{0}, over{0};
  };
  struct H2Counts {
    Histogram2D::CellGeom g;   ///< the widget's real cell grid; never the requested bin count
    std::unique_ptr<std::atomic<uint32_t>[]> c;
    std::atomic<uint64_t> total{0}, out{0};
  };
  H1Counts c1[MaxNumberOfDigitizer][MaxNumberOfChannel];
  H2Counts c2[MaxNumberOfDigitizer];
  std::vector<uint32_t> scratch;   ///< GUI thread only

  void ResizeCounts1D(int d, int ch);   ///< (re)size from the widget's binning and zero
  void ResizeCounts2D(int d);
  void ClearAllCounts();
  void PublishHist1D(int d, int ch);    ///< counts -> widget. GUI thread.
  void PublishHist2D(int d);
  void AddCount1D(int d, int ch, int series, double v);   ///< worker thread
  void AddCount2D(int d, int ch, double e);

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

  /// Connected to Histogram1D/Histogram2D AboutToRebin/ReBinned. The right-click Rebin frees and
  /// reallocates the very buffers the fill worker writes into, so the worker has to be drained
  /// across the whole operation -- the same suspendFilling fence SetPaneSource() uses.
  void BeginHistogramEdit();
  void EndHistogramEdit();

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

protected:
  /// Keep windowVisible in step without the worker having to call isVisible() itself.
  void showEvent(QShowEvent * event) override;
  void hideEvent(QHideEvent * event) override;

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
