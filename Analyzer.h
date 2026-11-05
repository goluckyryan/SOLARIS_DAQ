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

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Analysis.h"
#include "AnalysisPlugin.h"
#include "ClassDigitizer2Gen.h"
#include "CustomWidgets.h"
#include "EventRing.h"
#include "Histogram1D.h"
#include "Histogram2D.h"
#include "OnlineEventBuilder.h"
#include "RateStore.h"

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
///
/// Since ABI v2 the analysis lays out its own window: the host keeps the "Control" box (enable,
/// selector, rebuild, event-builder settings, log) and hands the analysis everything below and
/// beside it as one work area. What used to be plotBox + a stacked settings column is now the
/// analysis' canvas, see Analysis.h.

//^============================================================ histogram registry

/// Implements HistRegistry over the Qt histogram widgets.
///
/// Counts live in plain atomic arrays written by the analysis thread; the widgets are only ever
/// touched from the GUI thread. This is not tidiness: Histogram1D::Fill() assigns a QString into a
/// QCPItemText, whose d-pointer store races the GUI thread's draw() and can leave it reading freed
/// memory. Histogram2D::Fill() additionally walks cutList while the operator may be editing it.
///
/// Declarations arriving on the analysis thread reserve their id and counters but not a widget:
/// the widget (a Qt object, GUI thread only) materialises at the next Publish(). placement is
/// honoured either way, which is what makes an analysis-thread "add a histogram now" complete
/// without any GUI-side follow-up from the analysis.

struct AnalyzerHooks {
  /// Run fn on the analysis thread and wait for it. This is the fence every parameter write rides.
  std::function<void(const std::function<void()> &)> fence;
  /// Analyzer::isSignalSlotActive, read when a widget signal fires.
  std::function<bool()> signalsActive;
  /// v4 plumbing: only the status label can be claimed; the builder setters are idle-gated
  /// inside the window and keep the bar spinboxes in sync as a side effect.
  std::function<QWidget *()> claimStatus;
  std::function<uint64_t()>   timeWindow;
  std::function<void(uint64_t)> setTimeWindow;
  std::function<double()>     guardFactor;
  std::function<void(double)> setGuardFactor;
  std::function<void(const QString &)> setWindowTitle;
  std::function<void(int,int)> resizeWindow;
  std::function<bool()> influxOut;
  std::function<void(std::string &, std::string &, std::string &)> influxConfig;
  std::function<double()> rateBase;   ///< the bar's rate-average window in seconds
};

class QtHistRegistry : public HistRegistry {
public:
  QtHistRegistry(QWidget * root, Digitizer2Gen ** digi, unsigned int nDigi,
                 const std::vector<int> & boardIndex, const AnalyzerHooks & hooks);

  //---- HistRegistry. Declare-side calls work on any thread (see class comment).
  void * WorkArea() override;
  int  Hist1D(const char * name, const char * xLabel,
              int nBin, double xMin, double xMax, void * placement) override;
  int  Hist2D(const char * name, const char * xLabel, const char * yLabel,
              int nX, double xMin, double xMax,
              int nY, double yMin, double yMax, void * placement) override;
  void * View1(int id) override;
  void * View2(int id) override;
  void * ChannelParam(const char * label, int * digi, int * channel, void * placement) override;
  void * BoolParam(const char * label, bool * value, void * placement) override;

  //---- HistRegistry, v3 claimable plumbing. GUI thread; they forward to AnalyzerHooks.
  void * StatusControl() override;   ///< the only claimable plumbing widget; enable/window/guard
                                     ///< are fixed Analyzer-bar chrome (v4)
  uint64_t TimeWindow() override;
  void SetTimeWindow(uint64_t ns) override;
  double GuardFactor() override;
  void SetGuardFactor(double factor) override;
  void SetWindowTitle(const char * title) override;
  void ResizeWindow(int width, int height) override;

  //---- HistRegistry, v3+ InfluxDB plumbing (v5). Any thread; forwarded to AnalyzerHooks.
  bool InfluxOut() override;
  void InfluxConfig(std::string & url, std::string & token, std::string & database) override;

  //---- HistRegistry v6: host-side rate ledger. Any thread; reads the published snapshot.
  double GateRates(int id, std::vector<std::pair<std::string,double>> & rates) override;

  //---- HistRegistry, called from ProcessEvent() on the analysis thread
  void Fill1(int id, double x) override;
  void Fill2(int id, double x, double y) override;

  //---- GUI thread only
  /// Wire every picker/flag created during Declare(): seeding must read the members AFTER
  /// LoadState() has restored them, so wiring cannot happen at creation time.
  void FinishDeclare();
  void Publish();      ///< materialise queued widgets, then counts -> widgets
  void ClearCounts();  ///< zero everything; used when a parameter changes
  /// Sample every 2D's cumulative total and live-cut cell-sums into the rate stores, then
  /// publish the {tag, Hz} snapshot. Driven by the replot clock WHILE ENABLED, independently of
  /// visibility (see the v6 comment on HistRegistry::GateRates).
  void SampleRates();
  void ResetRates();   ///< run boundary: wipe sample history, keep keys
  /// Forget every widget. It does NOT delete them: they are children of the work area, and the
  /// window destroys the work area itself, before this registry or the analysis go away.
  void Clear();

private:

  struct H1 {
    Histogram1D * w = nullptr;                 ///< null until materialised (GUI thread)
    std::string   name, xLabel;
    void        * placement = nullptr;
    bool          created = false;
    int nBin = 0; double lo = 0, hi = 0, dx = 1;   ///< declared binning; Resize1 re-reads the widget's
    std::unique_ptr<std::atomic<uint32_t>[]> c;
    /// total counts IN-RANGE entries only (matches the widget's "Total Entry"); the rejects
    /// live in under/over, so lifetime-filled == total+under+over. H2.total differs (it is
    /// everything, with the rejects in out) -- publish paths for the two differ accordingly.
    std::atomic<uint64_t> total{0}, under{0}, over{0};
  };
  struct H2 {
    Histogram2D * w = nullptr;
    std::string   name, xLabel, yLabel;
    void        * placement = nullptr;
    bool          created = false;
    /// Before the widget exists, declared geometry decides in/out; guard cells only exist once
    /// Histogram2D is constructed, so queued fills keep total/out but not cell counts (see Fill2).
    int nXReq = 0, nYReq = 0;
    double xLo = 0, xHi = 0, yLo = 0, yHi = 0;
    /// The widget's real cell grid, not the bin count the analysis asked for. See
    /// Histogram2D::GetCellGeom(): Rebin() adds two guard cells and coordToCell spreads the range
    /// over nx-1 intervals, so binning over the requested count draws everything on the wrong
    /// scale. Cached here so Fill2() never touches the widget from the analysis thread.
    Histogram2D::CellGeom g;
    std::unique_ptr<std::atomic<uint32_t>[]> c;
    std::atomic<uint64_t> total{0}, out{0};

    //^---- v6 rate ledger (see HistRegistry::GateRates). store/rateBaseS are GUI-thread only;
    //^     rateSnap is the published view the analysis thread reads under rateMtx. Sampled from
    //^     SampleRates(), NOT Publish(): publishing is visibility-gated, the numbers must not be
    //^     (a run whose window is hidden still tiles its rate points).
    RateStore    store;                        ///< keys: "all" + one per live cut name
    std::mutex   rateMtx;
    std::vector<std::pair<std::string,double>> rateSnap;
    double       rateBaseS = 0;
  };

  struct ChanReq {
    std::string label;
    int * digi = nullptr; int * ch = nullptr;
    QWidget    * line = nullptr;
    RComboBox  * cbDigi = nullptr;
    RComboBox  * cbCh   = nullptr;
    void * placement = nullptr;
    bool wired = false;
  };
  struct FlagReq {
    std::string label;
    bool * value = nullptr;
    QCheckBox * cb = nullptr;
    void * placement = nullptr;
    bool wired = false;
  };

  //---- GUI-thread materialisation
  void CreateH1(size_t i);
  void CreateH2(size_t i);
  void CreateChan(size_t i);
  void CreateFlag(size_t i);
  void WireChan(size_t i);
  void WireFlag(size_t i);
  void DrainPending();
  bool OnGuiThread() const;

  //---- analysis thread (routed through hooks.fence)
  void Resize1(int id);
  void Resize2(int id);

  std::vector<std::unique_ptr<H1>> h1;
  std::vector<std::unique_ptr<H2>> h2;
  std::vector<ChanReq>             chans;
  std::vector<FlagReq>             flags;
  std::vector<uint32_t>            scratch;   ///< GUI thread only

  /// Indices of entries declared on the analysis thread, waiting for their widgets.
  std::mutex gate;
  std::vector<size_t> pendH1, pendH2, pendChan, pendFlag;
  bool declareFinished = false;

  QWidget * root;
  Digitizer2Gen ** digi;
  unsigned int   nDigi;
  std::vector<int> boardIndex;   ///< view position -> digi[] index
  AnalyzerHooks hooks;
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

  /// MainWindow's InfluxDB connection settings, like SingleSpectra::SetInflux: pass real values
  /// when the singleton is healthy, empty strings when it is not, and the bar's "Influx out"
  /// checkbox enables itself accordingly. Re-callable mid-session (Program Settings -> Database).
  void SetInflux(const QString & url, const QString & token, const QString & database);

private:
  void BuildViewList();
  /// Close == stop, full stop: event building, rate sampling and Influx pushes die with the
  /// window, so nothing streams to a database nobody is watching. Re-showing and re-enabling
  /// starts a fresh run.
  void closeEvent( QCloseEvent * ev ) override;
  std::vector<DigiHitView> MakeViews() const;
  void RescanPlugins();
  void SelectAnalysis(const QString & name);
  void Rebuild();
  void Reload();
  void Log(const QString & text, bool isError = false);
  void SetEnabled(bool on);
  void UpdateStatus();

  /// The analysis' canvas: destroyed and rebuilt on every SelectAnalysis, in that order — the
  /// widgets inside belong to the registry's histogram entries, the layout tree to the analysis.
  QWidget * ResetWorkArea();

  /// One JSON object per analysis, next to the .so it came from: <analyzers>/state/<name>.json.
  void SaveAnalysisState();
  void LoadAnalysisState(const QString & name);

  Digitizer2Gen ** digi;
  unsigned int     nDigi;  std::vector<int> boardIndex;      ///< view position -> digi[] index, dummies excluded
  std::vector<uint8_t> nChannels;

  std::unique_ptr<OnlineEventBuilder> eb;
  std::unique_ptr<EventRing>          ring;
  Analysis       * analysis = nullptr;
  QtHistRegistry * registry = nullptr;
  AnalysisPlugin   plugin;                 ///< owns the dlopen handle of the loaded analysis
  std::vector<AnalysisPlugin::Info> found; ///< what the last scan saw
  QString          pluginDir;
  QString          stateDir;
  QProcess       * builder = nullptr;      ///< runs make for the Rebuild button

  QThread     * buildThread = nullptr;
  QThread     * anaThread   = nullptr;
  BuildWorker * buildWorker = nullptr;
  AnaWorker   * anaWorker   = nullptr;
  QTimer      * replotTimer = nullptr;

  QCheckBox      * chkEnable;
  QCheckBox      * chkInflux = nullptr;   ///< "Influx out" bar checkbox, host chrome like Enable
  RComboBox      * cbAnalysis;
  QWidget        * ctrlVault;       ///< resting place for the unclaimed status label
  QString          currentName;
  QPushButton    * bnRebuild;
  QPushButton    * bnReload;
  QPushButton    * bnRescan;
  QPlainTextEdit * logPane;
  RSpinBox    * sbTimeWindow;
  RSpinBox    * sbGuardFactor;      ///< guard time is ALWAYS this times the time window
  RSpinBox    * sbRateBase = nullptr;   ///< v6 rate-average window [s] for GateRates
  QLabel      * lbStatus;
  QGridLayout * topLay;
  QWidget     * workArea = nullptr;

  bool isSignalSlotActive = true;

  //^---- InfluxDB output config (v5 ABI). Written on the GUI thread by SetInflux, read on the
  //^     analysis thread through the registry hooks, so the mutex IS the fence -- there is no
  //^     worker-parked window for a settings change to ride. influxOut mirrors the checkbox for
  //^     the same cross-thread read.
  std::mutex        influxMtx;
  std::string       influxURL, influxToken, influxDatabase;
  std::atomic<bool> influxOut{false};
};

#endif
