#ifndef BROKER_SERVER_H
#define BROKER_SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdint>

#include "BrokerProtocol.h"
#include "macro.h"
#include "ClassDigitizer2Gen.h"

class BrokerServer {
public:
  BrokerServer();
  ~BrokerServer();

  void SetCommandEndpoint(const std::string& endpoint);
  void SetPublishEndpoint(const std::string& endpoint);
  void SetScalarInterval(float seconds);

  int  Start();   // bind sockets
  void Run();     // blocking main loop
  void Stop();    // signal all threads to stop

  // Open/close digitizers before or during Run()
  int  OpenDigitizer(const std::string& url);
  void CloseDigitizer(int index);
  void CloseAllDigitizers();

  int  GetNumDigitizers() const { return nDigi; }

private:
  // ZMQ
  void* zmqCtx;
  void* zmqRep;  // REQ/REP command socket
  void* zmqPub;  // PUB broadcast socket

  std::string cmdEndpoint;
  std::string pubEndpoint;

  // Digitizers
  Digitizer2Gen* digi[MaxNumberOfDigitizer];
  int nDigi;
  std::mutex digiMutex[MaxNumberOfDigitizer];

  // Acquisition threads (one per digitizer)
  std::thread readThread[MaxNumberOfDigitizer];
  std::atomic<bool> readThreadStop[MaxNumberOfDigitizer];
  bool isSaveData[MaxNumberOfDigitizer];
  int  dataFormat[MaxNumberOfDigitizer]; // current data format per digitizer

  // Scalar broadcast thread
  std::thread scalarThread;
  std::atomic<bool> scalarThreadStop;
  std::atomic<int>  scalarCountdown;
  float scalarIntervalSec;

  // For accept rate calculation
  uint64_t oldTimeStamp[MaxNumberOfDigitizer][MaxNumberOfChannel];
  uint64_t oldSavedCount[MaxNumberOfDigitizer][MaxNumberOfChannel];

  // For hit summary publishing
  unsigned long lastPublishedIndex[MaxNumberOfDigitizer][MaxNumberOfChannel];
  unsigned long lastPublishedTraceIndex[MaxNumberOfDigitizer];

  std::atomic<bool> running;

  // Command dispatch
  void HandleMessage(const uint8_t* data, size_t len);

  // Command handlers
  void HandlePing();
  void HandleOpenDigi(const uint8_t* data, size_t len);
  void HandleCloseDigi(const uint8_t* data, size_t len);
  void HandleListDigi();
  void HandleGetDigiInfo(const uint8_t* data, size_t len);
  void HandleReadValue(const uint8_t* data, size_t len);
  void HandleWriteValue(const uint8_t* data, size_t len);
  void HandleSendCommand(const uint8_t* data, size_t len);
  void HandleStartACQ(const uint8_t* data, size_t len);
  void HandleStopACQ(const uint8_t* data, size_t len);
  void HandleSetDataFormat(const uint8_t* data, size_t len);
  void HandleGetACQStatus();
  void HandleOpenFile(const uint8_t* data, size_t len);
  void HandleCloseFile(const uint8_t* data, size_t len);
  void HandleSetSaveData(const uint8_t* data, size_t len);
  void HandleGetFileStatus(const uint8_t* data, size_t len);
  void HandleReadAllSettings(const uint8_t* data, size_t len);
  void HandleSaveSettingsFile(const uint8_t* data, size_t len);
  void HandleLoadSettingsFile(const uint8_t* data, size_t len);
  void HandleShutdown();

  // Background workers
  void ReadDataLoop(int digiIndex);
  void ScalarBroadcastLoop();

  // Response helpers
  void SendReply(const std::vector<uint8_t>& buf);
  void SendOK();
  void SendError(const std::string& msg);
  void SendValue(const std::string& value);

  // Publish helpers
  void PublishScalar(int digiIndex);
  void PublishHitSummaries(int digiIndex);
  void PublishTraceSnapshot(int digiIndex);
  void PublishStatusChange(StatusEvent event, uint8_t digiIndex);
  void PublishLog(const std::string& msg);
};

#endif // BROKER_SERVER_H
