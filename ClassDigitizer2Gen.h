#ifndef  DIGITIZER_CLASS_H
#define  DIGITIZER_CLASS_H


#include <CAEN_FELib.h>
#include <cstdlib>
#include <string>

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

    std::string modelName;
    std::string cupVersion;
    std::string DPPVersion;
    std::string DPPType;
    unsigned short serialNumber;
    unsigned short adcBits;
    unsigned short nChannels;
    unsigned short adcRate;
    unsigned short ch2ns;

    std::string IPAddress;
    std::string netMask;
    std::string gateway;

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
    Reg VGASetting[4]; 

  public:
    Digitizer2Gen();
    ~Digitizer2Gen();

    unsigned short GetSerialNumber() const{return serialNumber;}

    void  SetDummy(unsigned short sn);
    bool  IsDummy() const {return isDummy;}

    int OpenDigitizer(const char * url);
    bool IsConnected() const {return isConnected;}
    int CloseDigitizer();

    int GetRet() const {return ret;};
  
    std::string  ReadValue(const char * parameter, bool verbose = false);
    std::string  ReadValue(Reg &para, int ch_index = -1, bool verbose = false);
    std::string  ReadValue(TYPE type, unsigned short index, int ch_index = -1, bool verbose = false);
    std::string  ReadDigValue(std::string shortPara, bool verbose = false);
    std::string  ReadChValue(std::string ch, std::string shortPara, bool verbose = false);
    bool         WriteValue(const char * parameter, std::string value);
    bool         WriteValue(Reg &para, std::string value, int ch_index = -1);
    bool         WriteDigValue(std::string shortPara, std::string value);
    bool         WriteChValue(std::string ch, std::string shortPara, std::string value);
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
    void ProgramPHA(bool testPulse = false);
    
    unsigned short GetNChannels() const {return nChannels;}
    unsigned short GetCh2ns()     const {return ch2ns;}
    uint64_t       GetHandle()    const {return handle;}
    
    Event *evt;  // should be evt[MaxNumber], when full or stopACQ, save into file
    void OpenOutFile(std::string fileName, const char * mode = "w");
    void CloseOutFile();
    void SaveDataToFile();
    unsigned int GetFileSize() const {return outFileSize;}
    uint64_t GetTotalFilesSize() const {return FinishedOutFilesSize + outFileSize;}

    std::string GetSettingFileName() const {return settingFileName;}
    void SetSettingFileName(std::string fileName) {settingFileName = fileName;}
    void ReadAllSettings(); // read settings from digitier and save to memory
    bool SaveSettingsToFile(const char * saveFileName = NULL); // ReadAllSettings + text file
    bool LoadSettingsFromFile(const char * loadFileName = NULL); // Load settings, write to digitizer and save to memory
    std::string GetSettingValue(TYPE type, unsigned short index, unsigned int ch_index = 0) const {
      switch(type){
        case TYPE::DIG: return boardSettings[index].GetValue();
        case TYPE::CH:  return chSettings[ch_index][index].GetValue();
        case TYPE::VGA: return VGASetting[ch_index].GetValue();
        case TYPE::LVDS: return "not defined";
      }
      return "invalid";
    }
    std::string GetSettingValue(TYPE type, const Reg para, unsigned int ch_index = 0) const{
      switch(type){
        case TYPE::DIG:{
          for( int i = 0; i < (int) boardSettings.size(); i++){
            if( para.GetPara() == boardSettings[i].GetPara()){
              return boardSettings[i].GetValue();
            }
          }
        };break;
        case TYPE::CH:{
          for( int i = 0; i < (int) chSettings[ch_index].size(); i++){
            if( para.GetPara() == chSettings[ch_index][i].GetPara()){
              return chSettings[ch_index][i].GetValue();
            }
          }
        };break;
        case TYPE::VGA:  return VGASetting[ch_index].GetValue();
        case TYPE::LVDS: return "not defined";
        default : return "invalid";
      }

      return "no such parameter";

    }
};

#endif
