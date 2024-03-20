#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
#include <time.h> // time in nano-sec
#include <iostream>

#include <thread>
#include <mutex>

#include "../ClassDigitizer2Gen.h"
#include "../influxdb.h"

#define maxRead 400

std::mutex digiMTX;
Digitizer2Gen * digi = new Digitizer2Gen();
InfluxDB * influx = new InfluxDB("https://fsunuc.physics.fsu.edu/influx/", false);

unsigned int readCount = 0;

bool ThreadStop = false;
timespec ta, tb;

static void ReadDataLoop(){
  clock_gettime(CLOCK_REALTIME, &ta);
  //while(digi->IsAcqOn() && readCount < maxRead){
  while(!ThreadStop){
    digiMTX.lock();
    int ret = digi->ReadData();
    digiMTX.unlock();

    if( ret == CAEN_FELib_Success){
      digi->SaveDataToFile();
    }else if(ret == CAEN_FELib_Stop){
      digi->ErrorMsg("No more data");
      break;
    }else{
      //digi->ErrorMsg("ReadDataLoop()");
    }
    //if( readCount % 1000 == 0 ) {
    //  clock_gettime(CLOCK_REALTIME, &tb);
    //  double duration = tb.tv_nsec-ta.tv_nsec + tb.tv_sec*1e+9 - ta.tv_sec*1e+9;
    //  printf("%4d, duration : %10.0f, %6.1f\n", readCount, duration, 1e9/duration);
    //  ta = tb;
    //}
    //readCount++;
  }

}

char cmdStr[100];

static void StatLoop(){
  //while(digi->IsAcqOn() && readCount < maxRead){
  while(digi->IsAcqOn()){
    digiMTX.lock();

    digi->ReadStat();
    // for(int i = 0; i < 64; i++){
    //   digi->ReadValue( PHA::CH::SelfTrgRate, i, false);
    //   //influx->AddDataPoint("Rate,Bd=0,Ch=" + std::to_string(i) + " value=" + haha);
    // }
    // digi->ReadValue("/ch/4/par/ChRealtimeMonitor", true);
    // digi->ReadValue("/ch/4/par/ChDeadtimeMonitor", true);
    // digi->ReadValue("/ch/4/par/ChTriggerCnt", true);
    // digi->ReadValue("/ch/4/par/ChSavedEventCnt", true);
    // digi->ReadValue("/ch/4/par/ChWaveCnt", true);
    digiMTX.unlock();

    //influx->PrintDataPoints();
    //influx->WriteData("testing");
    //influx->ClearDataPointsBuffer();
    digi->PrintStat();
    usleep(1000*1000); // every 1000 msec    
  }

}

#include <CAENDig2.h>

int CAENDig2_GetLibVersion(char* version);

int main(int argc, char* argv[]){
  
  printf("##########################################\n");
  printf("\t   CAEN firmware DPP-PHA testing \n");
  printf("##########################################\n");

  remove("haha_000.sol");

  char version[16];
  CAENDig2_GetLibVersion(version);
  puts(version);

  char haha[100];
  CAEN_FELib_GetLibInfo(haha, 100);
  puts(haha);

  CAEN_FELib_GetLibVersion(version);
  puts(version);

/*
  const char * url = "dig2://192.168.0.100/";
  //const char * url = "dig2://192.168.0.254/";

  digi->OpenDigitizer(url);
  //digi->Reset();
  //digi->ReadAllSettings();

  //digi->ProgramBoard();

  //digi->WriteValue(PSD::CH::TriggerThreshold, "1000", -1);

  digi->PrintChannelSettings(0);

  digi->ReadValue(PHA::CH::ADCToVolts, 0, true);

  digi->SetDataFormat(DataFormat::ALL); 

  digi->OpenOutFile("haha");

  digi->StartACQ();

  timespec t0, t1;
  clock_gettime(CLOCK_REALTIME, &t0);

  std::thread th1 (ReadDataLoop);
  std::thread th2 (StatLoop);

  char c;
  printf("Press q for stop.");
  do{
    c = getchar();
  }while( c != 'q');

  ThreadStop = true;

  digiMTX.lock();
  digi->StopACQ();
  digiMTX.unlock();
  
  th1.join();
  th2.join();

  clock_gettime(CLOCK_REALTIME, &t1);
  printf("t1-t0 : %.0f ns = %.2f sec\n", 
            t1.tv_nsec-t0.tv_nsec + t1.tv_sec*1e+9 - t0.tv_sec*1e+9,
            (t1.tv_nsec-t0.tv_nsec + t1.tv_sec*1e+9 - t0.tv_sec*1e+9)*1.0/1e9);

  digi->CloseOutFile(); 



  digi->CloseDigitizer();
  
  delete digi;
*/

  delete influx;
  
}
