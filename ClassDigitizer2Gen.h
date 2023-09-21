#ifndef  DIGITIZER_CLASS_H
#define  DIGITIZER_CLASS_H


#include <CAEN_FELib.h>
#include <cstdlib>
#include <string>
#include <map>

#include "Event.h"

#define MaxOutFileSize 2*1024*1024*1024  //2GB
//#define MaxOutFileSize 20*1024*1024  //20MB
#define MaxNumberOfChannel 64

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
    int ret;

    char retValue[256];

    unsigned short serialNumber;
    std::string  FPGAType;
    unsigned int FPGAVer;
    unsigned short nChannels;
    unsigned short ch2ns;
    std::string ModelName;

    void Initialization();

    uint64_t realTime[MaxNumberOfChannel];
    uint64_t deadTime[MaxNumberOfChannel];
    uint64_t liveTime[MaxNumberOfChannel];
    uint32_t triggerCount[MaxNumberOfChannel];
    uint32_t savedEventCount[MaxNumberOfChannel];
    
    unsigned short outFileIndex;
    unsigned short dataStartIndetifier;
    std::string outFileNameBase;
    char outFileName[100];
    FILE * outFile;
    unsigned int outFileSize;
    uint64_t FinishedOutFilesSize;

    bool acqON;

    //all read and read/write settings
    std::string settingFileName;
    std::vector<Reg> boardSettings; 
    std::vector<Reg> chSettings[MaxNumberOfChannel];
    std::vector<Reg> LVDSSettings[4];
    Reg VGASetting[4]; 

    std::map<std::string, int> boardMap;
    std::map<std::string, int> LVDSMap;
    std::map<std::string, int> chMap;

  public:
    Digitizer2Gen();
    ~Digitizer2Gen();

    unsigned short GetSerialNumber() const{return serialNumber;}
    std::string GetFPGAType() const {return FPGAType;}
    std::string GetModelName() const {return ModelName;}
    unsigned int GetFPGAVersion() const {return FPGAVer;}

    void  SetDummy(unsigned short sn);
    bool  IsDummy() const {return isDummy;}

    int OpenDigitizer(const char * url);
    bool IsConnected() const {return isConnected;}
    int CloseDigitizer();

    int GetRet() const {return ret;};

    int FindIndex(const Reg para); // get index from DIGIPARA

    std::string  ReadValue(const char * parameter, bool verbose = false);
    std::string  ReadValue(const Reg para, int ch_index = -1, bool verbose = false); // read digitizer and save to memory
    bool         WriteValue(const char * parameter, std::string value, bool verbose = true);
    bool         WriteValue(const Reg para, std::string value, int ch_index = -1); // write digituzer and save to memory
    void         SendCommand(const char * parameter);
    void         SendCommand(std::string shortPara);

    uint64_t    GetHandle(const char * parameter);
    uint64_t    GetParentHandle(uint64_t handle);
    std::string GetPath(uint64_t handle);
    
    std::string ErrorMsg(const char * funcName);

    void StartACQ();
    void StopACQ();
    bool IsAcqOn() const {return acqON;}
    
    void SetPHADataFormat(unsigned short dataFormat); // 0 = all data, 
                                                      // 1 = analog trace-0 only + flags
                                                      // 2 = no trace, only ch, energy, timestamp, fine_timestamp + flags
                                                      // 3 = only ch, energy, timestamp, minimum
                                                      // 15 = raw data
    int  ReadData();
    int  ReadStat(); // digitizer update it every 500 msec
    void PrintStat();
    uint32_t GetTriggerCount(int ch) const {return triggerCount[ch];}
    uint64_t GetRealTime(int ch) const {return realTime[ch];}

    void Reset();
    void ProgramDPPBoard();
    void ProgramPHAChannels(bool testPulse = false);
    void ProgramPSDChannels(bool testPulse = false);
    
    unsigned short GetNChannels() const {return nChannels;}
    unsigned short GetCh2ns()     const {return ch2ns;}
    uint64_t       GetHandle()    const {return handle;}
    
    Event *evt;  // should be evt[MaxNumber], when full or stopACQ, save into file
    void OpenOutFile(std::string fileName, const char * mode = "wb"); //overwrite binary
    void CloseOutFile();
    void SaveDataToFile();
    unsigned int GetFileSize() const {return outFileSize;}
    uint64_t GetTotalFilesSize() const {return FinishedOutFilesSize + outFileSize;}

    std::string GetSettingFileName() const {return settingFileName;}
    void SetSettingFileName(std::string fileName) {settingFileName = fileName;}
    void ReadAllSettings(); // read settings from digitier and save to memory
    int  SaveSettingsToFile(const char * saveFileName = NULL, bool setReadOnly = false); //Save settings from memory to text file
    int  ReadAndSaveSettingsToFile(const char * saveFileName = NULL); // ReadAllSettings + text file
    bool LoadSettingsFromFile(const char * loadFileName = NULL); // Load settings, write to digitizer and save to memory

    std::string GetSettingValue(const Reg para, unsigned int ch_index = 0); // read from memory
};

#endif
