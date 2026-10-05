#ifndef  DIGITIZER_CLASS_H
#define  DIGITIZER_CLASS_H


#include <CAEN_FELib.h>
#include <atomic>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include "Hit.h"
#include "LeanHit.h"
#include "RingBuffer.h"
#include "RawDecoder.h"

/// UL, and parenthesised. As bare `2*1024*1024*1024` this is signed-int overflow, i.e. undefined;
/// it only produced the intended value because its single use casts the leading 2 to unsigned,
/// which quietly made the whole expression unsigned arithmetic. Any other use would have been UB.
#define MaxOutFileSize (2UL*1024*1024*1024)  //2GB
//#define MaxOutFileSize 20*1024*1024  //20MB
#define MaxNumberOfChannel 64
#define MaxNumberOfGroup 16
#define RingBufferSize 10000

struct HitSummary {
  uint16_t energy;
  uint16_t energy_short;
};

#define TraceRingBufferSize 10

struct TraceSnapshot {
  int32_t analog_probes[2][MaxTraceLenght];
  uint8_t digital_probes[4][MaxTraceLenght];
  size_t traceLenght;
};

#include "DigiParameters.h"

//^=================== Digitizer Class
class Digitizer2Gen {
  private:
    uint64_t handle;    
    uint64_t ep_handle; ///end point handle
    uint64_t ep_folder_handle; ///end point folder handle

    uint64_t stat_handle;
    //uint64_t stat_folder_handle;

    bool isDummy;
    bool isConnected;

    unsigned int serialNumber;
    unsigned int CupVer;
    std::string  FPGAType; // look the DigitiParameter.h::PHA::DIG::FirwareType, DPP_PHA, DPP_ZLE, DPP_PSD, DPP_DAW, DPP_OPEN, and Scope
    unsigned int FPGAVer; // for checking copy setting
    unsigned short nChannels;
    unsigned short tick2ns;
    std::string ModelName;

    /// How many TempSensADCn this model answers, resolved from ModelName once the board has been
    /// opened. It replaces three hand-written exclusion lists that had drifted apart -- and all
    /// three were wrong in the same way: they said a VX2745 has TempSensADC0..6 and forgot
    /// TempSensADC7, so ADC7 was read, printed and saved on every model, including the ones that
    /// do not have it. Probed on the test station, a VX2745 answers all eight (32.6 .. 42.9 C).
    int nTempSensADC;
    void ResolveModelSensors();

    void Initialization();

    uint64_t realTime[MaxNumberOfChannel];
    uint64_t deadTime[MaxNumberOfChannel];
    uint64_t liveTime[MaxNumberOfChannel];
    uint32_t triggerCount[MaxNumberOfChannel];
    uint32_t savedEventCount[MaxNumberOfChannel];
    
    unsigned short outFileIndex;
    unsigned short dataStartIdentifier;
    std::string outFileNameBase;
    char outFileName[100];
    FILE * outFile;
    unsigned int outFileSize;
    uint64_t FinishedOutFilesSize;

    bool acqON;

    RawDecoder rawDecoder;

    //all read and read/write settings
    std::string settingFileName;
    std::vector<Reg> boardSettings;
    /// ONE channel table, not 64. Everything a Reg carries -- name, type, RW flag, unit, the whole
    /// allowed-answer list -- is identical for every channel; only the value differs. The code has
    /// always assumed that (clampToRange reads channel 0's table as representative for all of
    /// them), it just paid for 64 private copies anyway: 3328 Reg copies per digitizer in the
    /// constructor, dummies included, then again on open. It also had a bug by construction -- the
    /// PSD open path refilled only slots [0, nChannels), leaving the constructor's PHA Regs in
    /// [nChannels, 64) on a PSD board.
    std::vector<Reg> chSettingTable;
    /// The per-channel half, sized to chSettingTable by SetChSettingTable().
    std::vector<std::string> chValue[MaxNumberOfChannel];
    void SetChSettingTable(const std::vector<Reg> & table);

    std::vector<Reg> LVDSSettings[4];
    Reg VGASetting[4]; 
    Reg InputDelay[16];

    std::unordered_map<std::string, int> boardMap;
    std::unordered_map<std::string, int> LVDSMap;
    std::unordered_map<std::string, int> chMap;

  public:
    Digitizer2Gen();
    ~Digitizer2Gen();

    unsigned int GetSerialNumber() const {return serialNumber;}
    std::string    GetFPGAType()     const {return FPGAType;}
    std::string    GetModelName()    const {return ModelName;}
    unsigned int   GetFPGAVersion()  const {return FPGAVer;}
    unsigned int   GetCupVer()       const {return CupVer;}

    void  SetDummy(unsigned int sn);
    bool  IsDummy()     const {return isDummy;}
    bool  IsConnected() const {return isConnected;}

    int  OpenDigitizer(const char * url);
    int  CloseDigitizer();

    uint64_t    GetHandle(const char * parameter);
    uint64_t    GetParentHandle(uint64_t handle);
    std::string GetPath(uint64_t handle);

    std::string  ReadValue(const char * parameter, bool verbose = false);
    std::string  ReadValue(const Reg &para, int ch_index = -1, bool verbose = false); // read digitizer and save to memory
    bool         WriteValue(const char * parameter, std::string value, bool verbose = true);
    bool         WriteValue(const Reg &para, std::string value, int ch_index = -1); // write digituzer and save to memory
    void         SendCommand(const char * parameter);
    void         SendCommand(std::string shortPara);

    int FindIndex(const Reg &para); // get index from DIGIPARA
    std::string GetSettingValueFromMemory(const Reg &para, unsigned int ch_index = 0); // read from memory

    
    /// ret is passed in explicitly: a per-call status must not live in per-object storage,
    /// the DAQ and GUI threads call into this object concurrently.
    std::string ErrorMsg(const char * funcName, int ret);

    void StartACQ();
    void StopACQ();
    bool IsAcqOn() const {return acqON;}
    
    void SetDataFormat(unsigned short dataFormat); // dataFormat = namespace DataFormat

    int  ReadData();
    int  ReadStat(); // digitizer update it every 500 msec
    void PrintStat();
    /// Bounded: these are read from the GUI timer with whatever channel count the board reported.
    uint32_t GetTriggerCount(int ch) const {return (ch >= 0 && ch < MaxNumberOfChannel) ? triggerCount[ch] : 0;}
    uint64_t GetRealTime(int ch) const {return (ch >= 0 && ch < MaxNumberOfChannel) ? realTime[ch] : 0;}
    uint32_t GetSavedEventCount(int ch) const {return (ch >= 0 && ch < MaxNumberOfChannel) ? savedEventCount[ch] : 0;}

    void Reset();
    void ProgramBoard();
    void ProgramChannels(bool testPulse = false);

    /// Whether this model answers a given environment-sensor board parameter. The one place that
    /// knows which sensors a model has; the loops that read, print and save board settings all
    /// ask it rather than carrying their own list. Non-sensor parameters are always true, so it
    /// is safe to apply to a whole table.
    bool IsSensorReadable(const Reg & para) const;
    /// For the settings panel, which lays out one box per ADC temperature sensor.
    int  GetNTempSensADC() const {return nTempSensADC;}

    void PrintBoardSettings();
    void PrintChannelSettings(unsigned short ch);
    void AdjustParameterRanges(); // adjust min/max/step based on ModelName and FPGAType
    
    unsigned short GetNChannels() const {return nChannels;}
    unsigned short GetTick2ns()     const {return tick2ns;}
    uint64_t       GetHandle()    const {return handle;}

    /// The shared channel table. Used to be GetChSettings(ch); there is no longer a per-channel
    /// table to hand out, and both callers only ever read the immutable half of it.
    const std::vector<Reg>& GetChSettingTable() const {return chSettingTable;}
    const std::vector<Reg>& GetBoardSettings() const {return boardSettings;}
    
    RingBuffer<HitSummary, RingBufferSize> ringBuffer[MaxNumberOfChannel];
    RingBuffer<TraceSnapshot, TraceRingBufferSize> traceRingBuffer;

    /// Timestamped hits for online event building. ONE ring for the whole board, not one per
    /// channel: the board already emits all channels interleaved in timestamp order
    /// (format_RAW.md:91), so splitting per channel would only force a 64-way merge.
    /// Written by ReadDataThread only — single producer, like the rings above.
    RingBuffer<LeanHit, LeanHitRingSize> hitRing;

    /// Filling hitRing is opt-in, so the DAQ pays nothing for online event building when nobody is
    /// analysing. Set by the Analyzer window's enable toggle; read on the DAQ hot path, where a
    /// relaxed load is a plain mov. Off by default: hitRing stays empty until something asks.
    std::atomic<bool> fillHitRing{false};

    Hit *hit;  // should be hit[MaxNumber], when full or stopACQ, save into file
    void OpenOutFile(std::string fileName, const char * mode = "wb"); //overwrite binary
    void CloseOutFile();
    void SaveDataToFile();
    unsigned int GetFileSize() const {return outFileSize;}
    uint64_t GetTotalFilesSize() const {return FinishedOutFilesSize + outFileSize;}
    unsigned short GetOutFileIndex() const {return outFileIndex;} // number of files = index + 1
    std::string GetOutFileName() const {return outFileName;}

    /// Look up a setting by its CAEN name in the in-memory cache, for the elog template.
    /// It never touches the hardware, so the GUI thread can call it while the DAQ runs.
    /// found is set to false when the name is not a parameter of this firmware.
    std::string GetBoardSettingByName(const std::string & name, bool * found = nullptr) const;
    std::string GetChSettingByName(const std::string & name, int ch, bool * found = nullptr) const;

    std::string GetSettingFileName() const {return settingFileName;}
    void SetSettingFileName(std::string fileName) {settingFileName = fileName;}
    void ReadAllSettings(); // read settings from digitier and save to memory
    int  SaveSettingsToFile(const char * saveFileName = NULL, bool setReadOnly = false); //Save settings from memory to text file
    int  ReadAndSaveSettingsToFile(const char * saveFileName = NULL); // ReadAllSettings + text file
    bool LoadSettingsFromFile(const char * loadFileName = NULL); // Load settings, write to digitizer and save to memory

};

#endif
