//^====================================================
//^ READ-ONLY digitizer audit: is the ch0<->TRGIN hardware coincidence configured,
//^ and what do the board's own counters say?
//^
//^ CAEN_FELib_GetValue reads only, plus a GetHandle on the EXISTING
//^ /endpoint/dpppha/stats endpoint (same handle the DAQ app already uses; the
//^ SetReadDataFormat here matches the app's, so the endpoint is left exactly as
//^ the app configures it every run). No physics/DPP SetValue, no ACQ start/stop,
//^ no endpoint switching.
//^
//^ run: sudo? no -- just:  ./tests/coincCheck [dig2://192.168.0.100/]
//^====================================================
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <thread>
#include <chrono>
#include <CAEN_FELib.h>

static int g_fail = 0;

static std::string getv(uint64_t h, const char * par){
  char v[256] = {0};
  int ret = CAEN_FELib_GetValue(h, par, v);
  if( ret != CAEN_FELib_Success ){ snprintf(v, sizeof v, "<ERR %d>", ret); g_fail++; }
  return v;
}
static void show(uint64_t h, const char * par){
  printf("  %-38s = %s\n", par, getv(h, par).c_str());
}

int main( int argc, char ** argv ){
  const char * url = (argc > 1) ? argv[1] : "dig2://192.168.0.100/";
  uint64_t h = 0;
  int ret = CAEN_FELib_Open(url, &h);
  if( ret != CAEN_FELib_Success ){ printf("open %s failed (%d) -- DAQ app holding the session?\n", url, ret); return 2; }

  char model[64]={0}, sn[64]={0};
  CAEN_FELib_GetValue(h, "/par/ModelName", model);
  CAEN_FELib_GetValue(h, "/par/SerialNumber", sn);
  printf("board %s  model %s   (READ-ONLY audit)\n\n", sn, model);

  printf("[active endpoint]  %s   <- stats exist only under dpppha\n", getv(h,"/endpoint/par/activeendpoint").c_str());

  printf("\n[ch0 / ch52 trigger & coincidence configuration]\n");
  for( int ch : {0, 52, 1} ){
    char p[128];
    printf(" ch%d:\n", ch);
    snprintf(p,sizeof p,"/ch/%d/par/ChEnable",ch);                   show(h,p);
    snprintf(p,sizeof p,"/ch/%d/par/EventTriggerSource",ch);         show(h,p);
    snprintf(p,sizeof p,"/ch/%d/par/CoincidenceMask",ch);            show(h,p);
    snprintf(p,sizeof p,"/ch/%d/par/AntiCoincidenceMask",ch);        show(h,p);
    snprintf(p,sizeof p,"/ch/%d/par/CoincidenceLengthT",ch);         show(h,p);
    snprintf(p,sizeof p,"/ch/%d/par/TimeFilterRetriggerGuardT",ch);  show(h,p);
    snprintf(p,sizeof p,"/ch/%d/par/SelfTrgRate",ch);                show(h,p);
  }

  printf("\n[ITLA / TRGOUT routing (board level)]\n");
  show(h,"/par/ITLAMask"); show(h,"/par/ITLAMainLogic"); show(h,"/par/ITLAPairLogic");
  show(h,"/par/ITLAPolarity"); show(h,"/par/ITLAGateWidth");
  show(h,"/par/TrgOutMode"); show(h,"/par/GlobalTriggerSource");
  show(h,"/par/EnableStatisticEvents");

  /* the SAME endpoint + format the DAQ app opens; ReadData just consumes what
     the board already streams. Idle board => ReadData returns != Success. */
  printf("\n[counters from /endpoint/dpppha/stats, sampled ~4 s apart]\n");
  uint64_t st = 0;
  ret  = CAEN_FELib_GetHandle(h, "/endpoint/dpppha/stats", &st);
  ret |= CAEN_FELib_SetReadDataFormat(st,
    " [ \
        { \"name\": \"REAL_TIME_NS\",    \"type\": \"U64\", \"dim\": 1 }, \
        { \"name\": \"DEAD_TIME_NS\",    \"type\": \"U64\", \"dim\": 1 }, \
        { \"name\": \"LIVE_TIME_NS\",    \"type\": \"U64\", \"dim\": 1 }, \
        { \"name\": \"TRIGGER_CNT\",     \"type\": \"U32\", \"dim\": 1 }, \
        { \"name\": \"SAVED_EVENT_CNT\", \"type\": \"U32\", \"dim\": 1 } \
    ]" );
  if( ret != CAEN_FELib_Success ){
    printf("  stats endpoint unavailable (%d) -- activeendpoint not dpppha?\n", ret);
  }else{
    static uint64_t rt1[128], dt1[128], lt1[128];
    static uint32_t tc1[128], sc1[128];
    static uint64_t rt2[128], dt2[128], lt2[128];
    static uint32_t tc2[128], sc2[128];
    int r1 = CAEN_FELib_ReadData(st, 2000, rt1, dt1, lt1, tc1, sc1);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    int r2 = CAEN_FELib_ReadData(st, 2000, rt2, dt2, lt2, tc2, sc2);
    printf("  read rets: %d / %d\n", r1, r2);
    if( r1 == CAEN_FELib_Success && r2 == CAEN_FELib_Success ){
      printf("  ch | dRealTime[ms] | dTrigCnt | dSavedCnt |  TrigHz  |  SavedHz\n");
      for( int ch : {0, 52, 1} ){
        const double dt = (double)(rt2[ch] - rt1[ch]) * 1e-9;
        if( dt <= 0.001 ){ printf(" %2d |  stats idle (no acquisition time advancing)\n", ch); continue; }
        printf(" %2d | %13.1f | %8u | %9u | %8.2f | %8.2f\n", ch,
               dt*1e3, tc2[ch]-tc1[ch], sc2[ch]-sc1[ch],
               (tc2[ch]-tc1[ch])/dt, (sc2[ch]-sc1[ch])/dt);
      }
      printf("  NOTE: Saved < Trig  <=>  hardware coincidence is rejecting events.\n");
    }else{
      printf("  no stats data -- acquisition not running? (that is fine, nothing changed)\n");
    }
  }

  CAEN_FELib_Close(h);
  printf("\nfailed value-reads: %d (some parameter paths legitimately do not exist per model)\n", g_fail);
  return 0;
}
