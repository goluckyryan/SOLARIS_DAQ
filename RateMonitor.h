#ifndef RATE_MONITOR_H
#define RATE_MONITOR_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>

#include "qcustomplot.h"
#include "ClassDigitizer2Gen.h"

class SingleSpectra;
class RComboBox;
class QListWidget;
class QCheckBox;
class QCPGraph;

//^====================================================
//^ A companion "rate monitor": a QCustomPlot with X = time and Y = count rate [counts/s], one
//^ trace per SUBSCRIBED RATESTORE KEY. Keys come from two pickers: a (digitizer, channel) pair
//^ for a raw total, or any live key in SingleSpectra::rateStore -- which carries the per-channel
//^ totals AND the 1D gate rates, with the display names registered by SampleRates(). The monitor
//^ itself never counts: its timer pumps SampleRates() (so its tick is part of the sampling
//^ clock) and then reads rateStore.Rate(key, timeBase). Adding a trace keeps the owner channel
//^ drained via SetMonitorSelected (refcounted per channel) even when its pane is unmounted.
//^
//^ The "time average" is the trailing window of every rate: a trace point is Hz, counts in the
//^ window over the window's actual elapsed time (before one full window exists it degrades to
//^ elapsed-so-far, never a fake zero). The tick period equals the window, so consecutive points
//^ tile back-to-back like MCS bins; a longer average = more counts per point = less Poisson
//^ scatter (1/sqrt N). The window is linked app-wide (SingleSpectra::SetRateTimeBase).
//^ While hidden the timer is stopped -- closed means idle, never a silent plot loop.
//^ While the DAQ is not running the window is likewise idle (acqOn gate); each DAQ start
//^ resets the plot's absolute time to zero and erases the previous run's points.
//^====================================================
class RateMonitor : public QWidget{
  Q_OBJECT

public:
  // ss is NOT owned and must outlive this widget (SingleSpectra deletes it in its own dtor).
  RateMonitor(Digitizer2Gen ** digi, unsigned int nDigi, SingleSpectra * ss, QWidget * parent = nullptr);
  ~RateMonitor();

  //^---- the shared rate time base, pushed from SingleSpectra::SetRateTimeBase. This window's
  //^     time-base combo is a second face of that ONE control; fromUser=false when the change
  //^     originated from the main window, so it updates here without echoing back.
  void SetTimeBase(int ms, bool fromUser = true);

  /// Y auto-rescale currently paused (a user zoom owns the axis; Unzoom resumes). Public for
  /// the headless harness; there is deliberately no checkbox anymore.
  bool YFixed() const { return yFixed; }

  //^---- the acquisition gate. The monitor only samples while DAQ runs: DAQStopped freezes the
  //^     plot (last frame, no loop), DAQStarted resets ABSOLUTE TIME TO ZERO -- the x clock
  //^     restarts and the previous run's trace points are erased. (SingleSpectra drives both
  //^     from its startTimer/stopTimer ACQ hooks, and tells a freshly created monitor too.)
  void DAQStarted();
  void DAQStopped();

  //^---- SingleSpectra calls this when the gate picture changes WHILE the DAQ is idle (gate
  //^     drawn/loaded/renamed/deleted, a pane remounted): the picker lists what EXISTS, and
  //^     existence is announced by signals, not by samples. Safe to call any time.
  void RefreshStorePicker();          // relist live, untraced GATE keys (totals come via row 1)

protected:
  void showEvent(QShowEvent * e) override;  // resume the sampling+plot loop
  void hideEvent(QHideEvent * e) override;  // stop it -- closing the window must not keep plotting

private slots:
  void Sample();            // timer tick: pump the store, read windowed rates, append points
  void DigiChanged(int);    // repopulate the channel combo for the picked digitizer
  void AddChannel();        // subscribe the total key of the combo's (digi, ch)
  void AddStoreKey();       // subscribe the rateStore key picked in the histogram/gate combo
  void RemoveChannel();     // drop the list row the user picked
  void ClearAll();          // drop every subscription
  void SetLogY(bool on);    // toggle the Y scale type
  void UnzoomY();           // right-click: back to auto mode with the padded default range

private slots:
  /// Any USER-driven Y range change: hands the axis to Fixed-Y (an auto tick would undo it)
  /// and enforces the linear minimum of 0.
  void onYRangeChanged(const QCPRange & range);

private:
  void BuildUI();
  void FitY();                 // rescale + 10% headroom + minimum clamp (shared by all resets)
  bool HasKey(const QString & key) const;
  void AddTrace(const QString & key, int d, int ch, const QString & name);
  void RemoveTrace(const QString & key);
  void HoldChannel(int d, int ch);    // SetMonitorSelected(true), idempotent
  void ReleaseIfUnused(int d, int ch);// drop the drain only when no other trace needs it
  QString DisplayFor(const QString & key) const;
  QString TimeBaseText() const;       // "2 s" style label of the shared time base
  QString ChannelName(int d, int ch) const;

  Digitizer2Gen ** digi;
  unsigned int nDigi;
  SingleSpectra * ss;       // not owned

  QCustomPlot * plot;
  QTimer      * timer;
  QElapsedTimer clock;      // monitor time base; GUI thread only

  RComboBox   * cbDigi;
  RComboBox   * cbCh;
  RComboBox   * cbStore;    // live rateStore keys: totals and 1D gates
  RComboBox   * cmbTimeBase; // integration time (time base), e.g. 1 s / 5 s / 30 s
  QListWidget * lstMonitored;

  // One entry per subscribed store key. d/ch is the OWNING channel (drain refcount), not the key.
  struct Trace{
    QString key;
    int d = -1, ch = -1;
    QCPGraph        * graph = nullptr; // the trace on the plot
    QListWidgetItem * item  = nullptr; // the row in lstMonitored (its live rate readout)
    int miss = 0;                      // consecutive ticks the key has been gone from the store
  };
  QVector<Trace> traces;

  QVector<QColor> palette;
  int timeBaseMs = 1000;         // MIRROR of SingleSpectra::rateBaseMs; timer interval = window
  int windowSec = 300;           // keep the last five minutes
  bool acqOn = false;            // DAQ gate: no sampling, no timer, no pump until DAQStarted()
  bool yAutoBusy = false;        // we are setting the Y range ourselves; do not treat it as user action
  bool yFixed    = false;        // Y auto-rescale paused by a USER zoom (Unzoom resumes)
};

#endif
