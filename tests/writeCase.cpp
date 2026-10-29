//^====================================================
//^ Is CAEN_FELib_SetValue for enum-valued parameters case sensitive?
//^ Test that cannot change board state: we write CASE VARIANTS OF THE VALUE THE
//^ PARAMETER ALREADY HOLDS (accepted -> same setting; rejected -> nothing moved),
//^ plus one bogus token as a positive control for value validation.
//^ Board must be free (no DAQ app running). Run: ./tests/writeCase [dig2://ip/]
//^====================================================
#include <cstdio>
#include <cstring>
#include <string>
#include <CAEN_FELib.h>

static std::string getv(uint64_t h, const char * par){
  char v[256] = {0};
  if( CAEN_FELib_GetValue(h, par, v) != CAEN_FELib_Success ) return "<READ ERR>";
  return v;
}

int main(int argc, char ** argv){
  const char * url = (argc > 1) ? argv[1] : "dig2://192.168.0.100/";
  const char * par = "/ch/0/par/CoincidenceMask";
  uint64_t h = 0;
  int ret = CAEN_FELib_Open(url, &h);
  if( ret != CAEN_FELib_Success ){ printf("open %s failed (%d) -- DAQ app holding the session?\n", url, ret); return 2; }

  const std::string cur = getv(h, par);
  printf("board %s   %s = %s\n", getv(h,"/par/SerialNumber").c_str(), par, cur.c_str());

  std::string low = cur, up = cur, title = cur;
  for( char & c : low )   c = tolower((unsigned char)c);
  for( char & c : up )    c = toupper((unsigned char)c);
  for( size_t i = 0; i < title.size(); i++ )
    title[i] = i ? tolower((unsigned char)title[i]) : toupper((unsigned char)title[i]);

  const char * tries[][2] = {
    { cur.c_str(),   "exact (sanity, must succeed)" },
    { low.c_str(),   "all lower" },
    { up.c_str(),    "all upper" },
    { title.c_str(), "TitleCase" },
    { "TrgIn",       "app table token" },
    { "NoSuchValue", "bogus (positive control)" },
  };
  for( auto & t : tries ){
    const int r = CAEN_FELib_SetValue(h, par, t[0]);
    printf("  SetValue(\"%-12s\") -> %s (%d)   now: %s\n", t[0],
           r == CAEN_FELib_Success ? "OK" : "REJECTED", r, getv(h, par).c_str());
  }

  const int back = CAEN_FELib_SetValue(h, par, cur.c_str());   // leave no trace
  printf("restore exact(\"%s\") -> %d ; final value: %s\n", cur.c_str(), back, getv(h, par).c_str());
  CAEN_FELib_Close(h);
  return 0;
}
