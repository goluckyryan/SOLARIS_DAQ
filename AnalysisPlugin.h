#ifndef ANALYSIS_PLUGIN_H
#define ANALYSIS_PLUGIN_H

#include <string>
#include <vector>

#include "Analysis.h"

/// Loads an analysis from a shared object at runtime, so analyses can be compiled separately and
/// swapped without restarting the DAQ.
///
/// Deliberately Qt-free, so the loader can be exercised from the console test harness before any
/// GUI is involved.
///
/// Two behaviours here are not obvious and both are deliberate:
///
///  - IT NEVER CALLS dlclose. Unloading is where hot-reload crashes come from: static destructors,
///    thread-locals and atexit handlers registered by the library all run against code that is
///    about to be unmapped. Leaking the handle costs ~17 KB per reload, which is nothing over a
///    shift, and removes the entire failure class.
///
///  - IT LOADS A UNIQUELY-NAMED COPY EVERY TIME. dlopen keys on the path, so re-opening the same
///    path after a rebuild silently hands back the OLD handle and you debug phantom behaviour.
///    Each load copies the .so to a per-process temp directory under a fresh name.
class AnalysisPlugin {
public:

  struct Info {
    std::string name;   ///< basename without .so, what the GUI shows
    std::string path;   ///< full path to the shared object
  };

  /// List the .so files in `dir`. Does not load anything — the name shown is the filename, and the
  /// library's own AnalysisName() is only read once it is actually loaded.
  static std::vector<Info> Scan(const std::string & dir);

  AnalysisPlugin() {}
  ~AnalysisPlugin() {}

  /// Copy, dlopen, check the ABI version, and create an analysis.
  /// Returns null on any failure, with the reason in `err`. Never throws, never aborts: a bad or
  /// stale .so must leave the DAQ running.
  Analysis * Load(const std::string & path, std::string & err);

  /// Destroy through the library's own DestroyAnalysis — the object came from the plugin's
  /// operator new, so the plugin must free it.
  void Destroy(Analysis * a);

  bool                IsLoaded()    const { return handle != nullptr; }
  const std::string & DeclaredName() const { return declaredName; }
  const std::string & LoadedFrom()   const { return loadedFrom; }
  int                 LoadCount()    const { return loadCount; }

private:
  static bool CopyFile(const std::string & from, const std::string & to, std::string & err);

  void      * handle       = nullptr;   ///< intentionally leaked on reload; see the class comment
  Analysis * (*createFn)()             = nullptr;
  void       (*destroyFn)(Analysis *)  = nullptr;
  std::string declaredName;
  std::string loadedFrom;
  int         loadCount = 0;
};

#endif
