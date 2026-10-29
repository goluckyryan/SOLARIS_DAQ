// One-shot live probe (no app changes): what does the VX2745 actually answer for
// the parameter the Scalar's Trig column reads (SelfTrgRate) vs the per-channel
// trigger counter the firmware tree DOES have (TriggerCnt)?
// run: QT_QPA_PLATFORM=offscreen ./tests/probeStat [ip]
#include <QCoreApplication>
#include <QThread>
#include <cstdio>
#include "ClassDigitizer2Gen.h"

int main(int argc, char ** argv){
  QCoreApplication a(argc, argv);
  const char * url = (argc > 1) ? argv[1] : "dig2://192.168.0.100/";
  Digitizer2Gen d;
  if( d.OpenDigitizer(url) != 0 || !d.IsConnected() ){ printf("cannot open %s\n", url); return 2; }
  printf("opened SN %u (%s)\n", d.GetSerialNumber(), d.GetModelName().c_str());
  // raw parameter names as the scalar composes them (channel 0)
  const char * names[] = {
    "/ch/0/par/SelfTrgRate",
    "/ch/0/par/TriggerCnt",
    "/ch/0/par/ChannelStatus",
  };
  for( const char * n : names ){
    std::string v = d.ReadValue(n, true);
    printf("%-28s -> [%s]\n", n, v.c_str());
  }
  d.ReadAllSettings();
  d.ReadStat();
  printf("IDLE     scalar-view SelfTrgRate ch0: [%s]\n", d.GetSettingValueFromMemory(PHA::CH::SelfTrgRate,0).c_str() );

  // what does the board itself think EnableStatisticEvents is?
  printf("EnableStatisticEvents default: [%s]\n", d.ReadValue(PHA::DIG::EnableStatisticEvents).c_str());

  // like the DAQ's NON-Raw path (Minimum): SetDataFormat + StartACQ, stat events NOT touched
  d.SetDataFormat(DataFormat::Minimum);
  d.StartACQ();
  for( int round = 0; round < 4; round++ ){
    QThread::msleep(2000);
    d.ReadStat();
    printf("RUNNING t=%2ds SelfTrgRate ch0: [%s]  triggerCount[0]=%u savedCount[0]=%u realTime[0]=%lu\n",
       2*(round+1), d.GetSettingValueFromMemory(PHA::CH::SelfTrgRate,0).c_str(),
       d.GetTriggerCount(0), d.GetSavedEventCount(0), d.GetRealTime(0));
  }
  d.StopACQ();
  d.WriteValue(PHA::CH::WaveSaving, "OnRequest");
  QThread::msleep(500);
  d.CloseDigitizer();
  return 0;
}
