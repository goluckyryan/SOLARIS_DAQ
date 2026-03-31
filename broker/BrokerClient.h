#ifndef BROKER_CLIENT_H
#define BROKER_CLIENT_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

#include "BrokerProtocol.h"
#include "macro.h"
#include "ClassDigitizer2Gen.h"  // for HitSummary, TraceSnapshot, RingBuffer, MaxNumberOfChannel etc.

class BrokerClient {
public:
  BrokerClient();
  ~BrokerClient();

  int  Connect(const std::string& cmdEndpoint = DEFAULT_CMD_ENDPOINT_CLIENT,
               const std::string& pubEndpoint = DEFAULT_PUB_ENDPOINT_CLIENT);
  void Disconnect();
  bool IsConnected() const { return connected; }

  //=== Digitizer management
  int  OpenDigitizer(const std::string& url);   // returns digi index or -1
  void CloseDigitizer(int index);
  int  GetNumDigitizers();

  struct DigiInfo {
    bool     isConnected;
    uint16_t serialNumber;
    std::string modelName;
    std::string fpgaType;
    uint16_t nChannels;
    uint16_t tick2ns;
    uint32_t fpgaVersion;
    uint32_t cupVersion;
  };
  std::vector<DigiInfo> ListDigitizers();
  DigiInfo GetDigiInfo(int index);

  //=== Parameter access
  std::string ReadValue(int digiIndex, const std::string& parameter);
  bool WriteValue(int digiIndex, const std::string& parameter, const std::string& value);
  void SendCommand(int digiIndex, const std::string& command);

  //=== Acquisition control
  void StartACQ(int digiIndex, int dataFormat, bool saveData,
                const std::string& fileNameBase = "");
  void StopACQ(int digiIndex);
  struct ACQStatus {
    int nDigi;
    bool acqOn[MaxNumberOfDigitizer];
  };
  ACQStatus GetACQStatus();

  //=== File control
  void SetDataFormat(int digiIndex, int format);
  void OpenFile(int digiIndex, const std::string& fileName);
  void CloseFile(int digiIndex);
  void SetSaveData(int digiIndex, bool onOff);
  struct FileStatus {
    uint64_t totalFileSize;
    uint32_t currentFileSize;
  };
  FileStatus GetFileStatus(int digiIndex);

  //=== Settings
  void ReadAllSettings(int digiIndex);
  void SaveSettingsFile(int digiIndex, const std::string& fileName = "");
  void LoadSettingsFile(int digiIndex, const std::string& fileName = "");

  //=== Lifecycle
  bool Ping();
  void Shutdown();

  //=== Subscription data (populated by background SUB thread)
  struct ScalarData {
    uint16_t serialNumber;
    uint8_t  nChannels;
    uint32_t trgRate[MaxNumberOfChannel];
    uint64_t savedCount[MaxNumberOfChannel];
    float    acceptRate[MaxNumberOfChannel];
    uint64_t realTime[MaxNumberOfChannel];
    uint64_t totalFileSize;
    bool     acqOn;
    // Board status (updated by scalar broadcast)
    uint32_t ledStatus;
    uint32_t acqStatus;
    uint32_t tempADC[8];
  };

  // Heap-allocated buffers (ring buffers + traces are ~67 MB total)
  ScalarData scalarData[MaxNumberOfDigitizer];
  std::mutex scalarMutex;

  struct SubData {
    RingBuffer<HitSummary, RingBufferSize> ringBuffer[MaxNumberOfDigitizer][MaxNumberOfChannel];
    RingBuffer<TraceSnapshot, TraceRingBufferSize> traceRingBuffer[MaxNumberOfDigitizer];
  };
  std::unique_ptr<SubData> sub;  // allocated in constructor

  // Convenience accessors
  RingBuffer<HitSummary, RingBufferSize>& GetRingBuffer(int digi, int ch) { return sub->ringBuffer[digi][ch]; }
  RingBuffer<TraceSnapshot, TraceRingBufferSize>& GetTraceRingBuffer(int digi) { return sub->traceRingBuffer[digi]; }

  // Callbacks for subscription events (optional)
  std::function<void(int digiIndex)>                   onScalarUpdate;
  std::function<void(int digiIndex, int nHits)>        onHitSummary;
  std::function<void(int digiIndex)>                   onTraceSnapshot;
  std::function<void(StatusEvent event, int digiIndex)> onStatusChange;
  std::function<void(const std::string& msg)>          onLogMessage;

  std::string GetLastError() const { return lastError; }

private:
  void* zmqCtx;
  void* zmqReq;
  void* zmqSub;

  bool connected;
  std::string lastError;

  // Subscription thread
  std::thread subThread;
  std::atomic<bool> subThreadStop;
  void SubscriptionLoop();

  // REQ/REP helpers
  std::vector<uint8_t> SendRequest(const std::vector<uint8_t>& request);
  bool CheckOK(const std::vector<uint8_t>& response);
  std::string ExtractValue(const std::vector<uint8_t>& response);
};

#endif // BROKER_CLIENT_H
