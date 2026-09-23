#ifndef ANALYSIS_H
#define ANALYSIS_H

#include <cstdint>
#include <string>
#include <vector>

#include "BuiltHit.h"

/// The online analysis interface.
///
/// Deliberately free of Qt, CAEN_FELib and ROOT. An analysis declares what histograms and controls
/// it wants and fills the histograms; it never creates a widget, never owns a thread, and never
/// sees the GUI. That is what will let the same class be loaded from a .so in a separate process
/// later: every call crosses the boundary through a vtable, so the plugin needs no host symbols.
///
/// THREADING.
///   Declare()      - GUI thread, with the workers stopped. It is where the host creates widgets,
///                    which Qt only permits there. Called once when the analysis is selected.
///   ProcessEvent() - analysis worker thread.
///   OnRunStart()   - analysis worker thread, when analysis is switched on.
///   OnRunStop()    - analysis worker thread, on switch-off, after the final drain.
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

/// Histograms and controls are requested through this; the host owns everything it returns.
class HistRegistry {
public:
  virtual ~HistRegistry() = default;

  //^---- declared once, from Declare(). Both return an id used to fill that histogram.
  //^     1D and 2D ids are separate spaces, and Fill1/Fill2 are named apart, so that filling a
  //^     2D histogram with a 1D call is a compile-time or assert failure rather than silent data
  //^     landing in the wrong plot.
  virtual int Hist1D(const char * name, const char * xLabel,
                     int nBin, double xMin, double xMax, int row, int col) = 0;
  virtual int Hist2D(const char * name, const char * xLabel, const char * yLabel,
                     int nX, double xMin, double xMax,
                     int nY, double yMin, double yMax, int row, int col) = 0;

  //^---- called from ProcessEvent
  virtual void Fill1(int id, double x) = 0;
  virtual void Fill2(int id, double x, double y) = 0;

  //^---- runtime controls, also declared from Declare(). The analysis hands over a pointer to its
  //^     own member and the HOST builds the widget and writes the value back. ChannelParam is its
  //^     own entry because only the host knows how many channels each board has.
  //^     Int/Double params are not here: nothing needs them yet, and adding a virtual later costs
  //^     one line in the single implementor.
  virtual void ChannelParam(const char * label, int * digi, int * channel) = 0;
  virtual void BoolParam(const char * label, bool * value) = 0;
};

class Analysis {
public:
  virtual ~Analysis() = default;

  /// Declare histograms and controls. Called once when the analysis is selected.
  virtual void Declare(HistRegistry & reg, const AnalysisContext & ctx) = 0;

  /// One built event, hits in ascending timestamp order. Must not block.
  virtual void ProcessEvent(const BuiltHit * hits, int n, HistRegistry & reg) = 0;

  virtual void OnRunStart() {}
  virtual void OnRunStop()  {}
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
#define ANALYSIS_ABI_VERSION 1

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
