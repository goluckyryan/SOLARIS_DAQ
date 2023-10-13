#ifndef HIT_H
#define HIT_H

#include <stdio.h> 
#include <cstdlib>
#include <stdint.h>
#include <string>

#define MaxTraceLenght 8100

enum DataFormat{

  ALL      = 0x00,
  OneTrace = 0x01,
  NoTrace  = 0x02,
  Minimum  = 0x03,
  Raw      = 0x0A,
  
};

namespace DPPType{

  const std::string PHA = "DPP_PHA";
  const std::string PSD = "DPP_PSD";

};

class Hit {
  public:

    unsigned short dataType; 
    std::string DPPType;

    ///============= for dpp-pha
    uint8_t  channel;        //  6 bit
    uint16_t energy;         // 16 bit
    uint16_t energy_short;   // 16 bit, only for PSD
    uint64_t timestamp;      // 48 bit
    uint16_t fine_timestamp; // 16 bit
    uint16_t flags_low_priority; // 12 bit
    uint16_t flags_high_priority; // 8 bit
    size_t   traceLenght;     // 64 bit
    uint8_t downSampling;     // 8 bit
    bool board_fail;
    bool flush; 
    uint8_t  analog_probes_type[2];  // 3 bit for PHA, 4 bit for PSD
    uint8_t  digital_probes_type[4]; // 4 bit for PHA, 5 bit for PSD
    int32_t * analog_probes[2];      // 18 bit
    uint8_t * digital_probes[4];     // 1 bit
    uint16_t trigger_threashold;     // 16 bit
    size_t   event_size;  // 64 bit
    uint32_t aggCounter; // 32 bit

    ///============= for raw
    uint8_t * data; 
    size_t  dataSize;  /// number of byte of the data, size/8 = word [64 bits]
    uint32_t n_events;

    bool isTraceAllZero;

    Hit(){
      Init();
    }

    ~Hit(){
      ClearMemory();
    }

    void Init(){
      DPPType =  DPPType::PHA;
      dataType = DataFormat::ALL;

      channel = 0;
      energy = 0;
      energy_short = 0;
      timestamp = 0;
      fine_timestamp = 0;
      downSampling = 0;
      board_fail = false;
      flush = false;
      flags_low_priority = 0;
      flags_high_priority = 0;
      trigger_threashold = 0;
      event_size = 0;
      aggCounter = 0;
      analog_probes[0] = NULL;
      analog_probes[1] = NULL;
      digital_probes[0] = NULL;
      digital_probes[1] = NULL;
      digital_probes[2] = NULL;
      digital_probes[3] = NULL;

      analog_probes_type[0] = 0xFF;
      analog_probes_type[1] = 0xFF;
      digital_probes_type[0] = 0xFF;
      digital_probes_type[1] = 0xFF;
      digital_probes_type[2] = 0xFF;
      digital_probes_type[3] = 0xFF;
      data = NULL;

      isTraceAllZero = true; // indicate trace are all zero
    }

    void ClearMemory(){
      if( data != NULL ) delete data;

      if( analog_probes[0] != NULL) delete analog_probes[0];
      if( analog_probes[1] != NULL) delete analog_probes[1];
      
      if( digital_probes[0] != NULL) delete digital_probes[0];
      if( digital_probes[1] != NULL) delete digital_probes[1];
      if( digital_probes[2] != NULL) delete digital_probes[2];
      if( digital_probes[3] != NULL) delete digital_probes[3];

      isTraceAllZero = true;
    }

    void SetDataType(unsigned int type, std::string dppType){
      dataType = type;
      DPPType = dppType;
      ClearMemory();

      if( dataType == DataFormat::Raw){
        data = new uint8_t[20*1024*1024];
      }else{
        analog_probes[0] = new int32_t[MaxTraceLenght];
        analog_probes[1] = new int32_t[MaxTraceLenght];

        digital_probes[0] = new uint8_t[MaxTraceLenght];
        digital_probes[1] = new uint8_t[MaxTraceLenght];
        digital_probes[2] = new uint8_t[MaxTraceLenght];
        digital_probes[3] = new uint8_t[MaxTraceLenght];

        isTraceAllZero = true;

      }
    }

    void ClearTrace(){
      if( isTraceAllZero ) return; // no need to clear again

      for( int i = 0; i < MaxTraceLenght; i++){
        analog_probes[0][i] = 0;
        analog_probes[1][i] = 0;

        digital_probes[0][i] = 0;
        digital_probes[1][i] = 0;
        digital_probes[2][i] = 0;
        digital_probes[3][i] = 0;
      }
      isTraceAllZero = true;
    }

    void PrintEnergyTimeStamp(){
      printf("ch: %2d, energy: %u, timestamp: %lu ch, traceLenght: %lu\n", channel, energy, timestamp, traceLenght);
    }

    std::string AnaProbeType(uint8_t probeType){

      if( DPPType == DPPType::PHA){
        switch(probeType){
          case 0: return "ADC";
          case 1: return "Time filter";
          case 2: return "Energy filter";
          default : return "none";
        }
      }else if (DPPType == DPPType::PSD){
        switch(probeType){
          case 0: return "ADC";
          case 9: return "Baseline";
          case 10: return "CFD";
          default : return "none";
        }
      }else{
        return "none";
      }
    }

    std::string DigiProbeType(uint8_t probeType){

      if( DPPType == DPPType::PHA){
        switch(probeType){
          case  0: return "Trigger";
          case  1: return "Time filter armed";
          case  2: return "Re-trigger guard";
          case  3: return "Energy filter baseline freeze";
          case  4: return "Energy filter peaking";
          case  5: return "Energy filter peaking ready";
          case  6: return "Energy filter pile-up guard";
          case  7: return "Event pile-up";
          case  8: return "ADC saturation";
          case  9: return "ADC saturation protection";
          case 10: return "Post-saturation event";
          case 11: return "Energy filter saturation";
          case 12: return "Signal inhibit";
          default : return "none";
        }
      }else if (DPPType == DPPType::PSD){
        switch(probeType){
          case  0: return "Trigger";
          case  1: return "CFD Filter Armed";
          case  2: return "Re-trigger guard";
          case  3: return "ADC Input Baseline freeze";
          case 20: return "ADC Input OverThreshold";
          case 21: return "Charge Ready";
          case 22: return "Long Gate";
          case  7: return "Pile-Up Trig.";
          case 24: return "Short Gate";
          case 25: return "Energy Saturation";
          case 26: return "Charge over-range";
          case 27: return "ADC Input Neg. OverThreshold";
          default : return "none";
        }

      }else{
        return "none";
      }
    }

    std::string HighPriority(uint16_t prio){
      std::string output;

      bool pileup = prio & 0x1;
      //bool pileupGuard = (prio >> 1) & 0x1; 
      //bool eventSaturated = (prio >> 2) & 0x1;
      //bool postSatEvent = (prio >> 3) & 0x1;
      //bool trapSatEvent = (prio >> 4) & 0x1;
      //bool SCA_Event = (prio >> 5) & 0x1;

      output = std::string("Pile-up: ") + (pileup ? "Yes" : "No");

      return output;
    }

    //TODO LowPriority

    void PrintAll(){
      
      switch(dataType){
        case DataFormat::ALL :      printf("============= Type : ALL\n"); break;
        case DataFormat::OneTrace : printf("============= Type : OneTrace\n"); break;
        case DataFormat::NoTrace :  printf("============= Type : NoTrace\n"); break;
        case DataFormat::Minimum :  printf("============= Type : Minimum\n"); break;
        case DataFormat::Raw :      printf("============= Type : Raw\n"); return; break;
        default : return;
      }

      printf("ch : %2d (0x%02X), fail: %d, flush: %d\n", channel, channel, board_fail, flush);
      if( DPPType == DPPType::PHA ) printf("energy: %u, timestamp: %lu, fine_timestamp: %u \n", energy, timestamp, fine_timestamp);
      if( DPPType == DPPType::PSD ) printf("energy: %u, energy_S : %u, timestamp: %lu, fine_timestamp: %u \n", energy, energy_short, timestamp, fine_timestamp);
      printf("flag (high): 0x%02X, (low): 0x%03X, traceLength: %lu\n", flags_high_priority, flags_low_priority, traceLenght);
      printf("Agg counter : %u, trigger Thr.: %u, downSampling: %u \n", aggCounter, trigger_threashold, downSampling);
      printf("AnaProbe Type: %s(%u), %s(%u)\n", AnaProbeType(analog_probes_type[0]).c_str(), analog_probes_type[0],
                                                AnaProbeType(analog_probes_type[1]).c_str(), analog_probes_type[1]);
      printf("DigProbe Type: %s(%u), %s(%u), %s(%u), %s(%u)\n", DigiProbeType(digital_probes_type[0]).c_str(), digital_probes_type[0],
                                                                DigiProbeType(digital_probes_type[1]).c_str(), digital_probes_type[1],
                                                                DigiProbeType(digital_probes_type[2]).c_str(), digital_probes_type[2],
                                                                DigiProbeType(digital_probes_type[3]).c_str(), digital_probes_type[3]);
    }

    void PrintTrace(unsigned short ID){
      for(unsigned short i = 0; i < (unsigned short)traceLenght; i++){
        if( ID == 0 ) printf("%4d| %6d\n", i, analog_probes[0][i]); 
        if( ID == 1 ) printf("%4d| %6d\n", i, analog_probes[1][i]); 
        if( ID == 2 ) printf("%4d| %u\n", i, digital_probes[0][i]); 
        if( ID == 3 ) printf("%4d| %u\n", i, digital_probes[1][i]); 
        if( ID == 4 ) printf("%4d| %u\n", i, digital_probes[2][i]); 
        if( ID == 5 ) printf("%4d| %u\n", i, digital_probes[3][i]); 
      }
    }

    void PrintAllTrace(){
      for(unsigned short i = 0; i < (unsigned short)traceLenght; i++){
        printf("%4d| %6d  %6d   %1d  %1d   %1d  %1d\n", i, analog_probes[0][i], 
                                                analog_probes[1][i], 
                                                digital_probes[0][i], 
                                                digital_probes[1][i], 
                                                digital_probes[2][i], 
                                                digital_probes[3][i]);
      }
    }

};

#endif