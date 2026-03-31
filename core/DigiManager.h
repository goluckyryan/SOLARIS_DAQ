#ifndef DIGI_MANAGER_H
#define DIGI_MANAGER_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

#include "macro.h"
#include "ClassDigitizer2Gen.h"
#include "BrokerClient.h"

class DigiManager {
public:

  enum class Mode { Standalone, Broker };

  DigiManager(Mode mode);
  ~DigiManager();

  Mode GetMode() const { return mode; }

  //=== Connection (broker mode only) ===
  int  Connect(const std::string& cmdEndpoint, const std::string& pubEndpoint);
  void Disconnect();

  //=== Digitizer lifecycle ===
  int  OpenDigitizer(const std::string& url);   // returns index or -1
  void CloseDigitizer(int index);
  void CloseAll();                               // standalone: close all; broker: disconnect
  int  GetNumDigitizers() const { return nDigi; }

  //=== Info queries ===
  int          GetNChannels(int digi) const;
  uint16_t     GetSerialNumber(int digi) const;
  std::string  GetModelName(int digi) const;
  std::string  GetFPGAType(int digi) const;
  unsigned short GetTick2ns(int digi) const;
  bool         IsDummy(int digi) const;
  bool         IsDigiConnected(int digi) const;

  //=== Parameter access ===
  std::string ReadValue(int digi, const Reg& reg, int ch = -1);       // reads from hardware, updates cache
  std::string ReadValueFromCache(int digi, const Reg& reg, int ch = 0); // reads from memory cache only
  bool        WriteValue(int digi, const Reg& reg, const std::string& value, int ch = -1); // writes to hardware, updates cache
  void        SendCommand(int digi, const Reg& reg);
  void        SendCommand(int digi, const std::string& cmd);

  //=== ACQ control ===
  void StartACQ(int digi, int dataFormat, bool saveData, const std::string& fileNameBase = "");
  void StopACQ(int digi);
  bool IsACQOn(int digi) const;

  //=== File I/O ===
  void     SetSaveData(int digi, bool onOff);
  void     OpenFile(int digi, const std::string& fileName);
  void     CloseFile(int digi);
  uint64_t GetTotalFileSize(int digi) const;

  //=== Settings ===
  void        SaveSettings(int digi, const std::string& fileName);
  void        LoadSettings(int digi, const std::string& fileName);
  void        SetSettingFileName(int digi, const std::string& fileName);
  std::string GetSettingFileName(int digi) const;

  void ReadAllSettings(int digi);

  //=== Data access (same ring buffer types in both modes) ===
  RingBuffer<HitSummary, RingBufferSize>&           GetRingBuffer(int digi, int ch);
  RingBuffer<TraceSnapshot, TraceRingBufferSize>&   GetTraceRingBuffer(int digi);

  //=== Scalar data ===
  struct ScalarSnapshot {
    uint32_t trgRate[MaxNumberOfChannel];
    uint64_t savedCount[MaxNumberOfChannel];
    float    acceptRate[MaxNumberOfChannel];
    uint64_t realTime[MaxNumberOfChannel];
    uint64_t totalFileSize;
    bool     acqOn;
    uint32_t ledStatus;
    uint32_t acqStatus;
    uint32_t tempADC[8];
  };
  ScalarSnapshot GetScalarSnapshot(int digi);

  //=== Callbacks ===
  std::function<void(int digiIndex)>            onScalarUpdate;
  std::function<void(int digiIndex, int nHits)> onHitSummary;
  std::function<void(int digiIndex)>            onTraceSnapshot;
  std::function<void(const std::string& msg)>   onLogMessage;

  //=== Direct access (standalone only, for compatibility during migration) ===
  Digitizer2Gen* GetDigitizer(int digi);

private:
  Mode mode;
  int  nDigi;

  //--- Standalone mode ---
  Digitizer2Gen* digi[MaxNumberOfDigitizer];
  std::thread    readThread[MaxNumberOfDigitizer];
  std::atomic<bool> readThreadStop[MaxNumberOfDigitizer];
  bool           isSaveData[MaxNumberOfDigitizer];
  std::mutex     digiMutex[MaxNumberOfDigitizer];

  void ReadDataLoop(int digiIndex);

  //--- Broker mode ---
  BrokerClient*  client;

  struct DigiInfoCache {
    uint16_t serialNumber = 0;
    std::string modelName;
    std::string fpgaType;
    uint16_t nChannels = 0;
    uint16_t tick2ns = 0;
    bool isConnected = false;
    bool isDummy = false;
    std::string settingFileName;
  };
  DigiInfoCache infoCache[MaxNumberOfDigitizer];

  void RefreshDigiInfo(int index);
};

#endif // DIGI_MANAGER_H
