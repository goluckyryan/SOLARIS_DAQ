#ifndef EVENT_H
#define EVENT_H

#include <stdio.h> 
#include <cstdlib>
#include <stdint.h>

#define MaxTraceLenght 2048

class Event {
  public:

    unsigned short dataType; 

    ///============= for dpp-pha
    uint8_t  channel;    //  6 bit
    uint16_t energy;   // 16 bit
    uint64_t timestamp;  // 48 bit
    uint16_t fine_timestamp; // 16 bit
    uint16_t flags_low_priority; // 12 bit
    uint16_t flags_high_priority; // 8 bit
    size_t   traceLenght; // 64 bit
    uint8_t downSampling; // 8 bit
    bool board_fail;
    bool flush; 
    uint8_t  analog_probes_type[2];  // 3 bit
    uint8_t  digital_probes_type[4]; // 4 bit
    int32_t * analog_probes[2];  // 18 bit
    uint8_t * digital_probes[4]; // 1 bit
    uint16_t trigger_threashold; // 16 bit
    size_t   event_size;  // 64 bit
    uint32_t aggCounter; // 32 bit

    ///============= for raw
    uint8_t * data; 
    size_t  dataSize;  /// number of byte of the data, size/8 = word [64 bits]
    uint32_t n_events;

    Event(){
      Init();
    }

    ~Event(){
      ClearMemory();
    }

    void Init(){
      channel = 0;
      energy = 0;
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
    }

    void ClearMemory(){
      if( data != NULL ) delete data;

      if( analog_probes[0] != NULL) delete analog_probes[0];
      if( analog_probes[1] != NULL) delete analog_probes[1];
      
      if( digital_probes[0] != NULL) delete digital_probes[0];
      if( digital_probes[1] != NULL) delete digital_probes[1];
      if( digital_probes[2] != NULL) delete digital_probes[2];
      if( digital_probes[3] != NULL) delete digital_probes[3];
    }

    void SetDataType(unsigned int type){
      dataType = type;
      ClearMemory();

      if( dataType == 0xF){
        data = new uint8_t[20*1024*1024];
      }else{
        analog_probes[0] = new int32_t[MaxTraceLenght];
        analog_probes[1] = new int32_t[MaxTraceLenght];

        digital_probes[0] = new uint8_t[MaxTraceLenght];
        digital_probes[1] = new uint8_t[MaxTraceLenght];
        digital_probes[2] = new uint8_t[MaxTraceLenght];
        digital_probes[3] = new uint8_t[MaxTraceLenght];

      }

    }

    void PrintEnergyTimeStamp(){
      printf("ch: %2d, energy: %u, timestamp: %lu ch, traceLenght: %lu\n", channel, energy, timestamp, traceLenght);
    }

    std::string AnaProbeType(uint8_t probeType){
      switch(probeType){
        case 0: return "ADC";
        case 1: return "Time filter";
        case 2: return "Energy filter";
        default : return "none";
      }
    }

    std::string DigiProbeType(uint8_t probeType){
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
      printf("============= Type : %u\n", dataType);
      printf("ch : %2d (0x%02X), fail: %d, flush: %d\n", channel, channel, board_fail, flush);
      printf("energy: %u, timestamp: %lu, fine_timestamp: %u \n", energy, timestamp, fine_timestamp);
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