#ifndef HIT_H
#define HIT_H

#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <string>

/// Software trace capacity. The true max is an FPGA property and cannot be queried over
/// the network, so it stays a compile-time constant: 64800 samples covers every record
/// length the parameter table allows on any known board (64800 ns @ 2 ns tick = 32400
/// samples; @ 8 ns = 8100). Cost: 12 B/sample -> trace ring 7.4 MiB + Hit 0.74 MiB per
/// board. Raise here and rebuild only.
#define MaxTraceLenght 8100 // in sample

enum DataFormat{

  ALL      = 0x00,
  OneTrace = 0x01,
  NoTrace  = 0x02,
  Minimum  = 0x03,
  MiniWithFineTime = 0x04,
  Raw      = 0x0A,
  
};

namespace DPPType{

  const std::string PHA = "DPP_PHA";
  const std::string PSD = "DPP_PSD";

};

/// A block starts with 0xAA00 | dataFormat, and Digitizer2Gen::SetDataFormat() ORs in 0x0010 for
/// PSD -- so the firmware flag is bit 4, not bit 1. Decoding it by hand in four places had already
/// produced one site that shifted by 1 and silently labelled every PHA block as PSD.
inline const std::string & DPPTypeOf(unsigned short blockStartIdentifier){
  return ((blockStartIdentifier >> 4) & 0xF) == 0 ? DPPType::PHA : DPPType::PSD;
}

class Hit {
  public:

    unsigned short dataType; 
    std::string DPPType;

    ///============= for dpp-pha
    uint8_t  channel;        //  6 bit
    uint16_t energy;         // 16 bit
    uint16_t energy_short;   // 16 bit, only for PSD
    uint64_t timestamp;      // 48 bit, in ns
    uint16_t fine_timestamp; // 10 bit, in ps
    uint16_t flags_low_priority; // 12 bit
    uint16_t flags_high_priority; // 8 bit
    size_t   traceLenght;     // 64 bit
    uint8_t downSampling;     // 8 bit
    bool board_fail;
    bool flush; 
    uint8_t  analog_probes_type[2];  // 3 bit for PHA, 4 bit for PSD
    uint8_t  digital_probes_type[4]; // 4 bit for PHA, 5 bit for PSD
    int32_t  analog_probes[2][MaxTraceLenght];   // 18 bit
    uint8_t  digital_probes[4][MaxTraceLenght];  // 1 bit
    uint16_t trigger_threashold;     // 16 bit
    size_t   event_size;  // 64 bit
    uint32_t aggCounter; // 32 bit

    ///============= for raw
    uint8_t * data; 
    size_t  dataSize;  /// number of byte of the data, size/8 = word [64 bits]
    uint32_t n_events;

    /// The format carries a trace (ALL, OneTrace); set by SetDataType(). ReadData() pushes to the
    /// scope's trace ring only when this is true -- never under Raw, where the waveform stays inside
    /// the undecoded blob (which is why StartScope() forces ALL).
    bool hasTrace = false;

    Hit(){
      DPPType =  DPPType::PHA;
      dataType = DataFormat::ALL;
      data = NULL;
      ResetEventFields();
      ZeroProbes();
    }

    ~Hit(){
      if( data != NULL ) { delete[] data; data = NULL; }
    }

    /// Owns `data`; a copy would free it twice (and would silently copy ~97 KB of probes).
    Hit(const Hit &) = delete;
    Hit & operator=(const Hit &) = delete;

    /// Per-event state, as a freshly constructed Hit has it. Not the buffers.
    void ResetEventFields(){
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

      analog_probes_type[0] = 0xFF;
      analog_probes_type[1] = 0xFF;
      digital_probes_type[0] = 0xFF;
      digital_probes_type[1] = 0xFF;
      digital_probes_type[2] = 0xFF;
      digital_probes_type[3] = 0xFF;
      traceLenght = 0;
      dataSize = 0;
      n_events = 0;
    }

    void ZeroProbes(){
      std::memset(analog_probes,  0, sizeof(analog_probes));
      std::memset(digital_probes, 0, sizeof(digital_probes));
    }

    /// Re-callable: SetDataFormat() calls this on the same Hit at every start-run, so it must not
    /// reallocate. It used to free and re-new all six probe arrays and the 20 MB raw buffer, which
    /// is why SetDataFormat deleted and re-created the whole Hit -- and a Hit that is re-created
    /// under another thread's feet is a use-after-free.
    void SetDataType(unsigned int type, std::string dppType){
      dataType = type;
      DPPType = dppType;
      hasTrace = ( type == DataFormat::ALL || type == DataFormat::OneTrace );
      ResetEventFields();

      /// Zero all six: OneTrace/NoTrace never write AP1/DP*, so a trace left over from an ALL run
      /// would otherwise show up in a OneTrace run.
      ZeroProbes();

      /// Allocated on first use and kept: Raw -> ALL -> Raw does not reallocate 20 MB. Freed only
      /// in ~Hit().
      if( dataType == DataFormat::Raw && data == NULL ){
        data = new uint8_t[20*1024*1024];
      }
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

    void PrintAll(){
      
      switch(dataType){
        case DataFormat::ALL :               printf("============= Type : ALL\n"); break;
        case DataFormat::OneTrace :          printf("============= Type : OneTrace\n"); break;
        case DataFormat::NoTrace :           printf("============= Type : NoTrace\n"); break;
        case DataFormat::MiniWithFineTime :  printf("============= Type : Min with FineTimestamp\n"); break;
        case DataFormat::Minimum :           printf("============= Type : Minimum\n"); break;
        case DataFormat::Raw :               printf("============= Type : Raw\n"); return; break;
        default : return;
      }

      printf("ch : %2d (0x%02X), fail: %d, flush: %d\n", channel, channel, board_fail, flush);
      if( DPPType == DPPType::PHA ) printf("energy: %u, timestamp: %lu ns, fine_timestamp: %u ps\n", energy, timestamp, fine_timestamp);
      if( DPPType == DPPType::PSD ) printf("energy: %u, energy_S : %u, timestamp: %lu ns, fine_timestamp: %u ps\n", energy, energy_short, timestamp, fine_timestamp);
      printf("flag (high): 0x%02X, (low): 0x%03X, traceLength: %lu\n", flags_high_priority, flags_low_priority, traceLenght);
      printf("Agg counter : %u, trigger Thr.: %u, downSampling: %u \n", aggCounter, trigger_threashold, downSampling);
      printf("AnaProbe Type: %s(%u), %s(%u)\n", AnaProbeType(analog_probes_type[0]).c_str(), analog_probes_type[0],
                                                AnaProbeType(analog_probes_type[1]).c_str(), analog_probes_type[1]);
      printf("DigProbe Type: %s(%u), %s(%u), %s(%u), %s(%u)\n", DigiProbeType(digital_probes_type[0]).c_str(), digital_probes_type[0],
                                                                DigiProbeType(digital_probes_type[1]).c_str(), digital_probes_type[1],
                                                                DigiProbeType(digital_probes_type[2]).c_str(), digital_probes_type[2],
                                                                DigiProbeType(digital_probes_type[3]).c_str(), digital_probes_type[3]);
    }

};

#endif