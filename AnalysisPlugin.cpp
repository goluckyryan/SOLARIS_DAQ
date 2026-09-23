#include "AnalysisPlugin.h"

#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <fstream>

std::vector<AnalysisPlugin::Info> AnalysisPlugin::Scan(const std::string & dir){

  std::vector<Info> out;

  DIR * d = opendir(dir.c_str());
  if( d == nullptr ) return out;              // no plugin directory yet is not an error

  struct dirent * e;
  while( (e = readdir(d)) != nullptr ){
    const std::string f = e->d_name;
    if( f.size() < 4 ) continue;
    if( f.compare(f.size()-3, 3, ".so") != 0 ) continue;
    Info i;
    i.name = f.substr(0, f.size()-3);
    i.path = dir + "/" + f;
    out.push_back(i);
  }
  closedir(d);

  std::sort(out.begin(), out.end(),
            [](const Info & a, const Info & b){ return a.name < b.name; });
  return out;
}

bool AnalysisPlugin::CopyFile(const std::string & from, const std::string & to, std::string & err){

  std::ifstream in(from, std::ios::binary);
  if( !in ){ err = "cannot read " + from; return false; }

  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  if( !out ){ err = "cannot write " + to; return false; }

  out << in.rdbuf();
  if( !out ){ err = "copy to " + to + " failed"; return false; }
  return true;
}

Analysis * AnalysisPlugin::Load(const std::string & path, std::string & err){

  err.clear();

  //^---- copy to a name we have never dlopen'd before
  char dirBuf[256];
  snprintf(dirBuf, sizeof(dirBuf), "/tmp/solaris-analyzers-%d", (int) getpid());
  mkdir(dirBuf, 0700);                        // fine if it already exists

  std::string base = path;
  const size_t slash = base.find_last_of('/');
  if( slash != std::string::npos ) base = base.substr(slash + 1);
  if( base.size() > 3 ) base = base.substr(0, base.size() - 3);

  char uniq[512];
  snprintf(uniq, sizeof(uniq), "%s/%s.%d.so", dirBuf, base.c_str(), loadCount);
  loadCount++;

  if( !CopyFile(path, uniq, err) ) return nullptr;

  //^---- RTLD_LOCAL so a plugin's symbols never leak into the global namespace and collide with
  //^     the next one; RTLD_NOW so a missing symbol is an error here rather than mid-run.
  void * h = dlopen(uniq, RTLD_NOW | RTLD_LOCAL);
  if( h == nullptr ){
    err = std::string("dlopen failed: ") + dlerror();
    return nullptr;
  }

  dlerror();  // clear any stale error before using the dlsym-returned-null test

  typedef int          (*VerFn)();
  typedef const char * (*NameFn)();
  typedef Analysis *   (*CreateFn)();
  typedef void         (*DestroyFn)(Analysis *);

  VerFn     verFn = (VerFn)     dlsym(h, "AnalysisABIVersion");
  NameFn    namFn = (NameFn)    dlsym(h, "AnalysisName");
  CreateFn  crtFn = (CreateFn)  dlsym(h, "CreateAnalysis");
  DestroyFn dstFn = (DestroyFn) dlsym(h, "DestroyAnalysis");

  if( verFn == nullptr || namFn == nullptr || crtFn == nullptr || dstFn == nullptr ){
    err = path + ": missing entry points. Did you end the file with ANALYSIS_ENTRY(...) and build "
                 "it with analyzers/Makefile?";
    return nullptr;                           // handle deliberately leaked, never dlclose
  }

  const int soVer = verFn();
  if( soVer != ANALYSIS_ABI_VERSION ){
    char msg[256];
    snprintf(msg, sizeof(msg),
             "%s: built against analysis ABI %d, this DAQ speaks %d. Rebuild it: make -C analyzers",
             path.c_str(), soVer, ANALYSIS_ABI_VERSION);
    err = msg;
    return nullptr;
  }

  Analysis * a = crtFn();
  if( a == nullptr ){ err = path + ": CreateAnalysis returned null"; return nullptr; }

  /// Only now commit: an earlier failure must leave the previously loaded plugin usable.
  handle       = h;
  createFn     = crtFn;
  destroyFn    = dstFn;
  declaredName = namFn() ? namFn() : "";
  loadedFrom   = path;

  return a;
}

void AnalysisPlugin::Destroy(Analysis * a){
  if( a == nullptr ) return;
  if( destroyFn ) destroyFn(a);
  else            delete a;     // only reachable for a compiled-in analysis
}
