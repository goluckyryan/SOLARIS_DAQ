#ifndef  DIGITIZER_CLASS_H
#define  DIGITIZER_CLASS_H


#include <CAEN_FELib.h>
#include <cstdlib>
#include <string>

#include "Event.h"
//#include "Parameter.h"

#define MaxOutFileSize 2*1024*1024*1024
#define MaxNumberOfChannel 64

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

    bool acqON;
  
  public:
    Digitizer2Gen();
    ~Digitizer2Gen();

    unsigned short GetSerialNumber() {return serialNumber;}

    void  SetDummy();
    bool  IsDummy() {return isDummy;}

    int OpenDigitizer(const char * url);
    bool IsConnected() const {return isConnected;}
    int CloseDigitizer();

    int GetRet() const {return ret;};
  
    std::string  ReadValue(const char * parameter, bool verbose = false);
    std::string  ReadChValue(std::string ch, std::string shortPara, bool verbose = false);
    void         WriteValue(const char * parameter, std::string value);
    void         WriteChValue(std::string ch, std::string shortPara, std::string value);
    void         SendCommand(const char * parameter);

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
    int  ReadStat();
    void PrintStat();

    void Reset();
    void ProgramPHA(bool testPulse = false);
    void ReadDigitizerSettings();
    
    unsigned short GetNChannels() const {return nChannels;}
    unsigned short GetCh2ns()     const {return ch2ns;}
    uint64_t       GetHandle()    const {return handle;}
    
    Event *evt;  // should be evt[MaxNumber], when full or stopACQ, save into file
    void OpenOutFile(std::string fileName);
    void CloseOutFile();
    void SaveDataToFile();
    unsigned int GetFileSize() {return outFileSize;}

    static unsigned short TraceStep;

};

#endif
