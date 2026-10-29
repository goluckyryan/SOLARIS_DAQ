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
#include "CustomThreads.h"
#include "CustomWidgets.h"
#include "Histogram1D.h"
#include "Histogram2D.h"
#include "RateStore.h"

#include <QElapsedTimer>

class HistWorker;  // Forward declaration
class RateMonitor; // Forward declaration

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

  //^---- RateMonitor support. SetMonitorSelected is GUI-thread only (it snaps lastFilledIndex,
  //^     fenced like every other GUI-thread write of that array); GetTotal1D is a plain atomic
  //^     load of the in-range total the fill worker maintains, so the monitor can call it anywhere.
  void SetMonitorSelected(int d, int ch, bool on);
  unsigned long GetTotal1D(int d, int ch) const;

  /// Stable RateStore key of a channel's total ("d<i>_sn<SN>_ch<j>"), or empty if the channel
  /// has no 1D histogram object.
  QString TotalRateKey(int d, int ch) const;

  /// True while a key's owning channel is actually being drained and sampled (mounted in a pane
  /// or held by a monitor trace) -- i.e. its window is fresh, not a stale leftover.
  bool RateChannelLive(const QString & key) const;

  /// Pump the RateStore from the worker atomics: totals and live 1D gate sums, for every channel
  /// that is mounted or monitored. GUI thread; called by the replot clock and by the monitor's
  /// own tick, so the sampling horizon never lags the slowest consumer.
  void SampleRates();

  //^---- THE rate time base: one shared integration window for every instantaneous number --
  //^     all histogram total/gate labels and every RateMonitor trace. The control sits next to
  //^     the "Rate Monitor" button; the monitor's own combo is a second face of the same value.
  void SetRateTimeBase(int ms);          ///< push to all plots, the store horizon, and the monitor
  int  RateTimeBase() const { return rateBaseMs; }

  /// Sliding-window dN/dt ledger for totals and 1D gates, sampled by the publish path on the
  /// GUI thread. GUI thread only. Future sinks (RateMonitor windows, InfluxDB) read from here.
  RateStore rateStore;

signals:
  // void startWorkerTimer(int interval);
  // void stopWorkerTimer();

public slots:
  void FillHistograms();
  void startTimer(){
    // printf("timer start\n");
    timer->start(maxFillTimeinMilliSec);
    replotTimer->start(replotIntervalMs);
    // emit startWorkerTimer(maxFillTimeinMilliSec);
  }
  void stopTimer(){
    // printf("timer stop\n");
    timer->stop();
    replotTimer->stop();
    // emit stopWorkerTimer();
    isFillingHistograms = false; // this will also break the FillHistogram do-loop
    ClearInternalDataCount();
  }

private:

  Digitizer2Gen ** digi;
  unsigned int nDigi;

  long lastFilledIndex[MaxNumberOfDigitizer][MaxNumberOfChannel]; // ring-buffer fill index per channel

  /// Which plots are mounted in a pane. Written by UpdateVisibilityFlags() on the GUI thread and
  /// read by the fill worker, hence atomic. They gate the fill: only what is on screen is drained.
  std::atomic<bool> histVisibility[MaxNumberOfDigitizer][MaxNumberOfChannel];
  std::atomic<bool> hist2DVisibility[MaxNumberOfDigitizer];

  /// A channel the RateMonitor window is tracking. ORed into the fill gate (see FillHistograms)
  /// so a monitored channel drains even when its pane -- and its board's 2D overview -- are not on
  /// screen. Written by SetMonitorSelected() on the GUI thread, read by the fill worker.
  std::atomic<bool> monitorSelected[MaxNumberOfDigitizer][MaxNumberOfChannel];

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
  RComboBox * cbRateBase;   ///< the shared rate time base, next to the "Rate Monitor" button
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

  /// The frozen line-parser for the pre-JSON HistogramSettings.txt; used once, to migrate.
  void ApplyLegacyTxtSettings(QFile & file);

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

  /// The GUI-thread replot clock. Counting (worker) and drawing are decoupled: the worker no
  /// longer drives repaints through workDone(); PublishHist1D/2D only relaxed-loads the atomic
  /// count arrays, so plotting at a steady GUI cadence is safe at any fill phase. Rebin/pane
  /// changes run on this same thread and fence the worker, so they cannot interleave with a
  /// repaint. Interval from UpdateRefreshRate(); started/stopped with the fill timer.
  QTimer    * replotTimer = nullptr;
  int replotIntervalMs;

  /// monotonic ms clock for RateStore sampling (GUI thread, started in the ctor)
  QElapsedTimer sampleClock;

  /// the ONE rate integration window [ms] shared by every histogram label and every monitor trace
  int rateBaseMs = 1000;

  QString settingPath;       // HistogramSettings.json
  QString legacySettingPath; // HistogramSettings.txt, migrated once on first json-less load

  unsigned short maxFillTimeinMilliSec;

  bool isSignalSlotActive;

  QThread * workerThread;
  HistWorker * histWorker;
  QTimer * timer;

  /// The RateMonitor window, opened from a control-box button. It is NOT a QObject child: this
  /// object owns the pointer and deletes it in ~SingleSpectra (after the worker is stopped), so it
  /// cannot be auto-freed a second time by the QObject destructor.
  RateMonitor * rateMonitor = nullptr;

protected:
  /// Keep windowVisible in step without the worker having to call isVisible() itself.
  void showEvent(QShowEvent * event) override;
  void hideEvent(QHideEvent * event) override;
  void closeEvent(QCloseEvent * event) override;   // takes the Rate Monitor companion down too

};

// //^#======================================================== HistWorker
class HistWorker : public QObject{
  Q_OBJECT
public:
  HistWorker(SingleSpectra * parent): SS(parent){}

public slots:
  void FillHistograms(){
    SS->FillHistograms();
  }

private:
  SingleSpectra * SS;
};

#endif
