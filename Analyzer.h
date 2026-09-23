#ifndef ANALYZER_H
#define ANALYZER_H

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <atomic>
#include <memory>
#include <vector>

#include "Analysis.h"
#include "AnalysisPlugin.h"
#include "ClassDigitizer2Gen.h"
#include "CustomWidgets.h"
#include "EventRing.h"
#include "Histogram1D.h"
#include "Histogram2D.h"
#include "OnlineEventBuilder.h"

/// Online analyzer window.
///
/// Owns the event builder, the event ring, and two worker threads:
///
///   builder thread   timer -> BuildEvents()           -> EventRing
///   analysis thread  timer -> Reader drains EventRing -> ProcessEvent -> atomic counts
///   GUI thread              counts -> Histogram widgets, ~2 Hz
///
/// Two threads rather than one is what makes the ring earn its keep: if the analysis stalls,
/// events queue in the ring instead of the builder stalling and letting the per-board hit rings
/// lap. Losing events is recoverable; losing raw hits is not.
///
/// Nothing here is tied to ACQ start/stop. The enable checkbox is the only control, and while it
/// is off the digitizers are not even filling their hit rings (Digitizer2Gen::fillHitRing).
///
/// Every GUI-initiated change is a BlockingQueuedConnection into the worker that owns the state,
/// so the marshalling IS the fence — there is no hand-rolled suspend/drain flag anywhere.

//^============================================================ histogram registry

/// Implements HistRegistry over the Qt histogram widgets.
///
/// Counts live in plain atomic arrays written by the analysis thread; the widgets are only ever
/// touched from the GUI thread. This is not tidiness: Histogram1D::Fill() assigns a QString into a
/// QCPItemText, whose d-pointer store races the GUI thread's draw() and can leave it reading freed
/// memory. Histogram2D::Fill() additionally walks cutList while the operator may be editing it.
class QtHistRegistry : public HistRegistry {
public:
  QtHistRegistry(QWidget * plotParent, QGridLayout * plotLayout,
                 QWidget * ctrlParent, QVBoxLayout * ctrlLayout,
                 Digitizer2Gen ** digi, const std::vector<int> & boardIndex);

  //---- HistRegistry, called from Declare() on the GUI thread
  int  Hist1D(const char * name, const char * xLabel,
              int nBin, double xMin, double xMax, int row, int col) override;
  int  Hist2D(const char * name, const char * xLabel, const char * yLabel,
              int nX, double xMin, double xMax,
              int nY, double yMin, double yMax, int row, int col) override;
  void ChannelParam(const char * label, int * digi, int * channel) override;
  void BoolParam(const char * label, bool * value) override;

  //---- HistRegistry, called from ProcessEvent() on the analysis thread
  void Fill1(int id, double x) override;
  void Fill2(int id, double x, double y) override;

  //---- GUI thread only
  void Publish();      ///< counts -> widgets
  void ClearCounts();  ///< zero everything; used when a parameter changes
  void Destroy();      ///< delete every widget this registry created

  /// Widgets the host built for declared parameters, so the window can wire them up.
  struct ChanWidget { RComboBox * cbDigi; RComboBox * cbCh; int * digi; int * ch; };
  std::vector<ChanWidget> chanWidget;
  std::vector<QCheckBox *> flagWidget;
  std::vector<bool *>      flagValue;

private:
  struct H1 {
    Histogram1D * w = nullptr;
    int nBin = 0; double lo = 0, hi = 0, dx = 1;
    std::unique_ptr<std::atomic<uint32_t>[]> c;
    std::atomic<uint64_t> total{0}, under{0}, over{0};
  };
  struct H2 {
    Histogram2D * w = nullptr;
    int nX = 0, nY = 0; double xLo = 0, xHi = 0, dx = 1, yLo = 0, yHi = 0, dy = 1;
    std::unique_ptr<std::atomic<uint32_t>[]> c;
    std::atomic<uint64_t> total{0}, out{0};
  };

  std::vector<std::unique_ptr<H1>> h1;
  std::vector<std::unique_ptr<H2>> h2;
  std::vector<uint32_t>            scratch;   ///< GUI thread only

  QWidget      * plotParent;
  QGridLayout  * plotLayout;
  QWidget      * ctrlParent;
  QVBoxLayout  * ctrlLayout;
  Digitizer2Gen ** digi;
  std::vector<int> boardIndex;   ///< view position -> digi[] index
};

//^============================================================ workers

/// Drains the per-board hit rings into the event ring. Owns all OnlineEventBuilder state.
class BuildWorker : public QObject {
  Q_OBJECT
public:
  explicit BuildWorker(OnlineEventBuilder * builder) : eb(builder) {}
public slots:
  void Init();                 ///< create the timer on THIS thread
  void Start(int periodMs);
  void Stop();                 ///< stop, then one final BuildEvents(true) to flush the tail
  void Tick();
private:
  OnlineEventBuilder * eb;
  QTimer * timer = nullptr;
};

/// Drains the event ring through the selected analysis.
class AnaWorker : public QObject {
  Q_OBJECT
public:
  explicit AnaWorker(EventRing * r) : ring(r) {}
  EventRing::Reader reader;
public slots:
  void Init();
  void Start(int periodMs);
  void Stop();                 ///< drain whatever is left, then OnRunStop
  void Tick();
  void SetAnalysis(Analysis * a, QtHistRegistry * r);
  /// Apply a parameter change on this thread, which is what makes the plain int members safe.
  void ApplyChannel(int * digi, int * channel, int d, int c);
  void ApplyFlag(bool * value, bool v);
private:
  void Drain();
  EventRing      * ring;
  Analysis       * ana = nullptr;
  QtHistRegistry * reg = nullptr;
  QTimer         * timer = nullptr;
  std::vector<BuiltHit> scratch;
};

//^============================================================ the window

class Analyzer : public QMainWindow {
  Q_OBJECT
public:
  Analyzer(Digitizer2Gen ** digi, unsigned int nDigi, QMainWindow * parent = nullptr);
  ~Analyzer();

private:
  void BuildViewList();
  std::vector<DigiHitView> MakeViews() const;
  void RescanPlugins();
  void SelectAnalysis(const QString & name);
  void Rebuild();
  void Reload();
  void Log(const QString & text, bool isError = false);
  void SetEnabled(bool on);
  void UpdateStatus();

  Digitizer2Gen ** digi;
  unsigned int     nDigi;
  std::vector<int> boardIndex;      ///< view position -> digi[] index, dummies excluded
  std::vector<uint8_t> nChannels;

  std::unique_ptr<OnlineEventBuilder> eb;
  std::unique_ptr<EventRing>          ring;
  Analysis       * analysis = nullptr;
  QtHistRegistry * registry = nullptr;
  AnalysisPlugin   plugin;                 ///< owns the dlopen handle of the loaded analysis
  std::vector<AnalysisPlugin::Info> found; ///< what the last scan saw
  QString          pluginDir;
  QProcess       * builder = nullptr;      ///< runs make for the Rebuild button

  QThread     * buildThread = nullptr;
  QThread     * anaThread   = nullptr;
  BuildWorker * buildWorker = nullptr;
  AnaWorker   * anaWorker   = nullptr;
  QTimer      * replotTimer = nullptr;

  QCheckBox      * chkEnable;
  RComboBox      * cbAnalysis;
  QPushButton    * bnRebuild;
  QPushButton    * bnReload;
  QPushButton    * bnRescan;
  QPlainTextEdit * logPane;
  RSpinBox    * sbTimeWindow;
  RSpinBox    * sbGuardTime;
  QLabel      * lbStatus;
  QGroupBox   * plotBox;
  QGridLayout * plotLayout;
  QGroupBox   * ctrlBox;
  QVBoxLayout * ctrlLayout;

  bool isSignalSlotActive = true;
};

#endif
