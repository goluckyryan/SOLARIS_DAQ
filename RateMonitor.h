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
//^ A companion "rate monitor": a QCustomPlot with X = time and Y = the total-entry (in-range)
//^ count rate [counts/s], one scatter trace per selected (digitizer, channel). Opened from a
//^ button in SingleSpectra, it rides on the GUI thread -- its timer only does atomic loads of the
//^ counters the fill worker already maintains (SingleSpectra::c1), never a widget.
//^
//^ The rate is the delta, rate = d(total)/d(t), between consecutive samples, binned over the
//^ selected time base (default 1 s, the MCS "time base"). A longer time base integrates more
//^ counts, so the Poisson scatter shrinks as 1/sqrt(N) -- exactly the CAEN MCS time-base control.
//^====================================================
class RateMonitor : public QWidget{
  Q_OBJECT

public:
  // ss is NOT owned and must outlive this widget (SingleSpectra deletes it in its own dtor).
  RateMonitor(Digitizer2Gen ** digi, unsigned int nDigi, SingleSpectra * ss, QWidget * parent = nullptr);
  ~RateMonitor();

private slots:
  void Sample();            // timer tick: read totals, form the rate deltas, append a point
  void DigiChanged(int);    // repopulate the channel combo for the picked digitizer
  void AddChannel();        // add the combo's (digi, ch) to the monitored set
  void RemoveChannel();     // drop the list row the user picked
  void ClearAll();          // drop every monitored channel
  void SetLogY(bool on);    // toggle the Y scale type
  void SetTimeBase(int);    // time-base combo: change the timer's integration interval
  void SetFixedY(bool on);  // freeze/resume the Y auto-rescale

private:
  void BuildUI();
  bool IsMonitored(int d, int ch) const;
  void AddTrace(int d, int ch);
  void RemoveTrace(int d, int ch, bool replotNow = true);
  QString ChannelName(int d, int ch) const;

  Digitizer2Gen ** digi;
  unsigned int nDigi;
  SingleSpectra * ss;       // not owned

  QCustomPlot * plot;
  QTimer      * timer;
  QElapsedTimer clock;      // monitor time base; GUI thread only

  RComboBox   * cbDigi;
  RComboBox   * cbCh;
  RComboBox   * cmbTimeBase; // integration time (time base), e.g. 1 s / 5 s / 30 s
  QListWidget * lstMonitored;
  QCheckBox   * chkLogY;
  QCheckBox   * chkFixedY; // freeze the Y auto-rescale (user pans/zooms instead)

  // One entry per currently monitored channel.
  struct Trace{
    int d, ch;
    QCPGraph        * graph;     // the scatter trace on the plot
    QListWidgetItem * item;      // the row in lstMonitored (its live rate readout)
    unsigned long lastTotal = 0; // previous sample's in-range total
    double lastTime = 0;         // previous sample's time, seconds on the monitor clock
    bool hasPrev = false;        // false until the first sample seeds the baseline
  };
  QVector<Trace> traces;

  QVector<QColor> palette;
  int timeBaseMs = 1000;         // timer interval = integration time (time base)
  int windowSec = 300;           // keep the last five minutes
};

#endif
