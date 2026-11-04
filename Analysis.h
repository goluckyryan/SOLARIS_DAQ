#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <cstdint>
#include <string>
#include <vector>

#include "BuiltHit.h"

/// The online analysis interface. ABI version 4.
///
/// An analysis declares histograms and controls and fills them; the HOST creates every widget.
/// That is not shyness about Qt -- an analysis may link Qt and include widget headers, it only
/// may never reference a SOLARIS symbol, because those live in the executable and the .so must
/// not depend on how the DAQ itself was built. Keeping the boundary at vtables is what lets the
/// same class load from a .so, and later from a separate process.
///
/// LAYOUT IS THE ANALYSIS' (this is the v2 change). WorkArea() hands over the one content widget
/// the host leaves entirely to the analysis: setLayout() anything -- splitters, group boxes,
/// nested grids, even a custom QLayout subclass (QLayout is not a QObject, so that needs no
/// moc). Histograms and channel pickers come back as opaque void * handles to place into that
/// layout; they remain host-built because they own thread fences and board knowledge.
///
/// THREADING.
///   Declare()      - GUI thread, with the workers stopped. Called once when the analysis is
///                    selected, before LoadState() and before picker wiring.
///   LoadState()    - GUI thread, immediately after Declare(). Restore members here: pickers are
///                    seeded from them afterwards, so a value written before Declare would be
///                    overwritten by the analysis' own initialiser.
///   SaveState()    - GUI thread, when the analysis is deselected or the window closes.
///   ProcessEvent() - analysis worker thread.
///   OnRunStart()   - analysis worker thread, when analysis is switched on.
///   OnRunStop()    - analysis worker thread, on switch-off, after the final drain.
///
/// Hist*() and *Param() run in whichever thread calls them:
///   - from the GUI thread (Declare, or any connect()ed callback -- the context object should be
///     a widget inside WorkArea, which the host guarantees to destroy BEFORE the analysis object)
///     the widget exists the moment the call returns and View*() is valid;
///   - from the analysis thread the id is reserved and fills may start immediately, but the
///     widget is built on the GUI thread at the next publish tick, and View*() returns null until
///     then. `placement` is still honoured either way, so an analysis-thread add needs no second
///     step; holding the handle until a GUI callback is the alternative.
///
/// Parameter values declared through HistRegistry are written by the host on the analysis thread,
/// between ProcessEvent() calls, so a plain int member needs no atomics. Nothing an analysis
/// implements is ever entered concurrently with anything else it implements.

/// Context the host knows and the analysis does not. Passed to Declare() as a struct so that
/// adding a field later does not change every analysis's signature.
struct AnalysisContext {
  int             nBoards;     ///< number of boards feeding the builder; dummies excluded
  const uint8_t * nChannels;   ///< nBoards entries; boards can differ
  uint64_t        timeWindow;  ///< builder's coincidence window in ns, so dt binning can match
};

/// SaveState()/LoadState() talk through this rather than a Qt type, so the file format stays the
/// host's business. All keys are flat; namespace anything collision-prone behind the analysis
/// name. SetStr/Set values from the current session are what a later LoadState() will see.
class SettingsKV {
public:
  virtual ~SettingsKV() = default;

  virtual void Set(const char * key, double value) = 0;
  virtual bool Get(const char * key, double * value) = 0;   ///< false = not stored

  virtual void SetStr(const char * key, const char * value) = 0;
  virtual bool GetStr(const char * key, std::string & out) = 0;
};

/// Histograms and controls are requested through this; the host owns everything it returns.
class HistRegistry {
public:
  virtual ~HistRegistry() = default;

  //^---- the content area the analysis owns outright. Returns a QWidget *. The host pins it below
  //      its own control strip and destroys it (every child widget with it) BEFORE DestroyAnalysis.
  virtual void * WorkArea() = 0;

  //^---- declared from Declare() or at runtime (see the threading note). Both return an id used to
  //^     fill that histogram. 1D and 2D ids are separate spaces, and Fill1/Fill2 are named apart,
  //^     so that filling a 2D histogram with a 1D call is a compile-time or assert failure rather
  //^     than silent data landing in the wrong plot.
  //^     `placement` is an opaque QLayout * the host addWidget()s the new plot into (on a splitter
  //^     pass its own addWidget, not a layout cast); null = place it yourself via View*().
  virtual int Hist1D(const char * name, const char * xLabel,
                     int nBin, double xMin, double xMax, void * placement) = 0;
  virtual int Hist2D(const char * name, const char * xLabel, const char * yLabel,
                     int nX, double xMin, double xMax,
                     int nY, double yMin, double yMax, void * placement) = 0;

  //^---- the created widget as an opaque QWidget *; null while the histogram is still queued
  //^     behind a publish tick. The host keeps ownership: never delete what you get back here,
  //^     just position it.
  virtual void * View1(int id) = 0;
  virtual void * View2(int id) = 0;

  //^---- called from ProcessEvent
  virtual void Fill1(int id, double x) = 0;
  virtual void Fill2(int id, double x, double y) = 0;

  //^---- runtime controls, declared from Declare() or at runtime. The analysis hands over a pointer
  //^     to its own member and the HOST builds the widget and writes the value back -- through the
  //^     analysis-thread fence, followed by a counts clear, so a plot never mixes the old and the
  //^     new selection. ChannelParam is its own entry because only the host knows how many
  //^     channels each board has. Both return the container QWidget * to place into the layout.
  //^     Int/Double params are not here: nothing needs them yet, and adding a virtual later costs
  //^     one line in the single implementor.
  virtual void * ChannelParam(const char * label, int * digi, int * channel, void * placement) = 0;
  virtual void * BoolParam(const char * label, bool * value, void * placement) = 0;

  //^---- HOST PLUMBING (v4). The Analyzer bar -- selector, enable, window, guard -- is FIXED
  //^     host chrome, always on top, never claimable: an analysis that could remove the button
  //^     that stops it (or the selector that rescues it from a broken .so) is not an analysis
  //^     anyone can trust mid-run. What an analysis CAN do is read and drive the VALUES below,
  //^     and claim the one widget that has no bar cell: the status label, which does not exist
  //^     on screen until it is asked for. GUI thread only, like Declare().
  virtual void * StatusControl() = 0;         ///< label with events/dropped/late counters

  //^---- programmatic access to the bar settings (GUI thread) -- the values, since the widgets
  //^     themselves stay on the bar. Setters are idle-only: moving the window mid-run would
  //^     rebuild half an event; a rejected call logs and no-ops. Guard time is not an absolute
  //^     knob any more: it is always factor x time window, so the two numbers cannot disagree.
  //^     The bar spinboxes update themselves when a setter takes effect.
  virtual uint64_t TimeWindow() = 0;
  virtual void SetTimeWindow(uint64_t ns) = 0;
  virtual double GuardFactor() = 0;
  virtual void SetGuardFactor(double factor) = 0;

  /// Window chrome, so an analysis can name and size its own canvas. Deliberately NOT the
  /// QMainWindow * itself: the host keeps the window, this is only dressing.
  virtual void SetWindowTitle(const char * title) = 0;
  virtual void ResizeWindow(int width, int height) = 0;

  //^---- InfluxDB output (v5). The checkbox lives on the Analyzer bar, NOT in the analysis:
  //^     InfluxOut() is its live state and InfluxConfig() the host's connection settings (empty
  //^     url/database = no database configured). The write itself belongs to the analysis: it
  //^     carries its own InfluxDB client (ClassInfluxDB.cpp is compiled into the .so), because
  //^     handing over the host's InfluxDB * would be a SOLARIS symbol crossing the plugin
  //^     boundary. Callable from any thread; both are cheap copies.
  virtual bool InfluxOut() = 0;
  virtual void InfluxConfig(std::string & url, std::string & token, std::string & database) = 0;
};

class Analysis {
public:
  virtual ~Analysis() = default;

  /// Declare histograms, controls and layout. Called once when the analysis is selected.
  virtual void Declare(HistRegistry & reg, const AnalysisContext & ctx) = 0;

  /// One built event, hits in ascending timestamp order. Must not block.
  virtual void ProcessEvent(const BuiltHit * hits, int n, HistRegistry & reg) = 0;

  virtual void OnRunStart() {}
  virtual void OnRunStop()  {}

  /// Persist the analysis' own state (channel picks, what extra plots were added, ...).
  /// The host stores one object per analysis name; a fresh analysis that never implements these
  /// simply always starts at its compiled-in defaults.
  virtual void SaveState(SettingsKV & kv) { (void)kv; }
  virtual void LoadState(SettingsKV & kv) { (void)kv; }
};

//^==================================================================== registry

/// Analyses register themselves, so adding one never means editing mainwindow.cpp or the .pro.
/// Drop a .cpp in analyzers/, end it with REGISTER_ANALYSIS, then re-run qmake (see README).
class AnalysisRegistry {
public:
  typedef Analysis * (*Factory)();

  /// Function-local static on purpose. A namespace-scope registry object would be a genuine
  /// static-initialisation-order fiasco: a registrar in analyzers/Foo.cpp can run before the
  /// registry's own std::vector is constructed, and the registry's construction would then wipe
  /// whatever had already registered.
  static AnalysisRegistry & Instance(){
    static AnalysisRegistry reg;
    return reg;
  }

  void Add(const char * name, Factory f){
    entry.push_back({std::string(name), f});
  }

  std::vector<std::string> Names() const {
    std::vector<std::string> out;
    for( size_t i = 0; i < entry.size(); i++ ) out.push_back(entry[i].name);
    return out;
  }

  /// Caller owns the returned object. Null if the name is unknown.
  Analysis * Create(const std::string & name) const {
    for( size_t i = 0; i < entry.size(); i++ ){
      if( entry[i].name == name ) return entry[i].factory();
    }
    return nullptr;
  }

private:
  struct Entry { std::string name; Factory factory; };
  std::vector<Entry> entry;
};

//^==================================================================== plugin ABI

/// Bump whenever anything an analysis can see changes shape: the Analysis or HistRegistry vtables,
/// AnalysisContext, or BuiltHit. The host refuses a .so built against a different value rather than
/// crashing in a vtable call.
///   1 - Qt-free declare/fill interface, host-owned layout (single plot grid, stacked controls)
///   2 - WorkArea()/View*()/placement handles: the analysis lays out its own window;
///       SaveState()/LoadState() per analysis
///   3 - claimable host plumbing, programmatic builder settings, window dressing; guard time
///       becomes factor x window, always
///   4 - the Analyzer bar is fixed chrome: Enable/TimeWindow/GuardFactor controls are no longer
///       claimable (virtuals removed). StatusControl, the setters and window dressing survive.
///   5 - InfluxOut()/InfluxConfig(): the host's "Influx out" checkbox and connection settings,
///       for analyses that carry their own InfluxDB client and write from ProcessEvent.
#define ANALYSIS_ABI_VERSION 5

/// The four C entry points a loadable analysis exports. Only emitted when compiled as a plugin.
///
/// The guard is not cosmetic: extern "C" names are fixed, so one analysis per .so, and without the
/// guard the test harness — which links every analyzers/*.cpp into ONE binary — would get duplicate
/// CreateAnalysis the moment a second analysis existed.
///
/// DestroyAnalysis exists rather than a host-side delete because the object came from the plugin's
/// operator new, so the plugin must be the one to free it.
#ifdef ANALYSIS_PLUGIN_BUILD
  #define ANALYSIS_PLUGIN_EXPORTS(cls, nameStr)                             \
    extern "C" int          AnalysisABIVersion(){ return ANALYSIS_ABI_VERSION; } \
    extern "C" const char * AnalysisName()      { return nameStr; }         \
    extern "C" Analysis *   CreateAnalysis()    { return new cls(); }       \
    extern "C" void         DestroyAnalysis(Analysis * a){ delete a; }
#else
  #define ANALYSIS_PLUGIN_EXPORTS(cls, nameStr)
#endif

/// Put this at the bottom of an analysis. It works in both link modes:
///   - built as a .so  (analyzers/Makefile, -DANALYSIS_PLUGIN_BUILD) -> the C entry points
///   - linked directly (Aux/testOnlineBuilder)                       -> the static registrar
/// The registrar is in an anonymous namespace so two analyses can never clash on its symbol.
#define ANALYSIS_ENTRY(cls, name)                                           \
  namespace {                                                               \
    struct cls##_Registrar {                                                \
      cls##_Registrar(){                                                    \
        AnalysisRegistry::Instance().Add(                                   \
          name, [](){ return (Analysis *) new cls(); });                    \
      }                                                                     \
    } g_##cls##_Registrar;                                                  \
  }                                                                         \
  ANALYSIS_PLUGIN_EXPORTS(cls, name)

/// Older spelling, static linking only.
#define REGISTER_ANALYSIS(cls, name)                                        \
  namespace {                                                               \
    struct cls##_Registrar {                                                \
      cls##_Registrar(){                                                    \
        AnalysisRegistry::Instance().Add(                                   \
          name, [](){ return (Analysis *) new cls(); });                    \
      }                                                                     \
    } g_##cls##_Registrar;                                                  \
  }

#endif
