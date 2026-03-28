#include "BrokerServer.h"

#include <zmq.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <algorithm>

BrokerServer::BrokerServer() {
  zmqCtx = zmq_ctx_new();
  zmqRep = nullptr;
  zmqPub = nullptr;

  cmdEndpoint = DEFAULT_CMD_ENDPOINT_SERVER;
  pubEndpoint = DEFAULT_PUB_ENDPOINT_SERVER;
  scalarIntervalSec = DEFAULT_SCALAR_INTERVAL_SEC;

  nDigi = 0;
  running = false;
  scalarThreadStop = false;

  for (int i = 0; i < MaxNumberOfDigitizer; i++) {
    digi[i] = nullptr;
    readThreadStop[i] = false;
    isSaveData[i] = false;
    lastPublishedTraceIndex[i] = 0;
    for (int ch = 0; ch < MaxNumberOfChannel; ch++) {
      oldTimeStamp[i][ch] = 0;
      oldSavedCount[i][ch] = 0;
      lastPublishedIndex[i][ch] = 0;
    }
  }
}

BrokerServer::~BrokerServer() {
  Stop();
  CloseAllDigitizers();
  if (zmqRep) zmq_close(zmqRep);
  if (zmqPub) zmq_close(zmqPub);
  if (zmqCtx) zmq_ctx_destroy(zmqCtx);
}

void BrokerServer::SetCommandEndpoint(const std::string& ep) { cmdEndpoint = ep; }
void BrokerServer::SetPublishEndpoint(const std::string& ep) { pubEndpoint = ep; }
void BrokerServer::SetScalarInterval(float sec) { scalarIntervalSec = sec; }

int BrokerServer::Start() {
  zmqRep = zmq_socket(zmqCtx, ZMQ_REP);
  if (!zmqRep) { printf("ERROR: cannot create REP socket\n"); return -1; }

  zmqPub = zmq_socket(zmqCtx, ZMQ_PUB);
  if (!zmqPub) { printf("ERROR: cannot create PUB socket\n"); return -1; }

  if (zmq_bind(zmqRep, cmdEndpoint.c_str()) != 0) {
    printf("ERROR: cannot bind REP to %s: %s\n", cmdEndpoint.c_str(), zmq_strerror(errno));
    return -1;
  }
  printf("Command socket bound to %s\n", cmdEndpoint.c_str());

  if (zmq_bind(zmqPub, pubEndpoint.c_str()) != 0) {
    printf("ERROR: cannot bind PUB to %s: %s\n", pubEndpoint.c_str(), zmq_strerror(errno));
    return -1;
  }
  printf("Publish socket bound to %s\n", pubEndpoint.c_str());

  return 0;
}

void BrokerServer::Run() {
  running = true;

  // Start scalar broadcast thread
  scalarThreadStop = false;
  scalarThread = std::thread(&BrokerServer::ScalarBroadcastLoop, this);

  printf("Broker running. Waiting for commands...\n");

  zmq_pollitem_t items[1];
  items[0] = { zmqRep, 0, ZMQ_POLLIN, 0 };

  while (running) {
    int rc = zmq_poll(items, 1, 500); // 500ms timeout
    if (rc < 0) {
      if (errno == EINTR) continue;
      printf("ERROR: zmq_poll failed: %s\n", zmq_strerror(errno));
      break;
    }

    if (items[0].revents & ZMQ_POLLIN) {
      zmq_msg_t msg;
      zmq_msg_init(&msg);
      int size = zmq_msg_recv(&msg, zmqRep, 0);
      if (size >= 2) {
        HandleMessage(static_cast<const uint8_t*>(zmq_msg_data(&msg)),
                      static_cast<size_t>(size));
      } else {
        SendError("message too short");
      }
      zmq_msg_close(&msg);
    }
  }

  // Stop scalar thread
  scalarThreadStop = true;
  if (scalarThread.joinable()) scalarThread.join();

  // Stop all read threads
  for (int i = 0; i < nDigi; i++) {
    readThreadStop[i] = true;
    if (readThread[i].joinable()) readThread[i].join();
  }

  printf("Broker stopped.\n");
}

void BrokerServer::Stop() {
  running = false;
}

//^============================================ Digitizer management

int BrokerServer::OpenDigitizer(const std::string& url) {
  // Find a free slot (reuse closed slots first)
  int idx = -1;
  for (int i = 0; i < nDigi; i++) {
    if (digi[i] == nullptr) { idx = i; break; }
  }
  if (idx < 0) {
    if (nDigi >= MaxNumberOfDigitizer) {
      printf("ERROR: max digitizers (%d) reached\n", MaxNumberOfDigitizer);
      return -1;
    }
    idx = nDigi;
  }
  digi[idx] = new Digitizer2Gen();
  int ret = digi[idx]->OpenDigitizer(url.c_str());

  if (ret != CAEN_FELib_Success) {
    printf("ERROR: failed to open digitizer at %s\n", url.c_str());
    delete digi[idx];
    digi[idx] = nullptr;
    return -1;
  }

  nDigi++;
  printf("Opened digitizer %d: SN=%d, Model=%s, FPGA=%s, %d channels\n",
         idx, digi[idx]->GetSerialNumber(),
         digi[idx]->GetModelName().c_str(),
         digi[idx]->GetFPGAType().c_str(),
         digi[idx]->GetNChannels());

  PublishStatusChange(EVT_DIGI_OPENED, idx);
  return idx;
}

void BrokerServer::CloseDigitizer(int index) {
  if (index < 0 || index >= nDigi || !digi[index]) return;

  // Stop read thread if running
  readThreadStop[index] = true;
  if (readThread[index].joinable()) readThread[index].join();

  {
    std::lock_guard<std::mutex> lock(digiMutex[index]);
    digi[index]->CloseDigitizer();
    delete digi[index];
    digi[index] = nullptr;
  }

  PublishStatusChange(EVT_DIGI_CLOSED, index);

  // Compact: shrink nDigi if trailing slots are empty
  while (nDigi > 0 && digi[nDigi - 1] == nullptr) nDigi--;
}

void BrokerServer::CloseAllDigitizers() {
  for (int i = 0; i < nDigi; i++) {
    CloseDigitizer(i);
  }
}

//^============================================ Command dispatch

void BrokerServer::HandleMessage(const uint8_t* data, size_t len) {
  size_t off = 0;
  uint8_t msgType = UnpackHeader(data, off);

  switch (msgType) {
    case REQ_PING:              HandlePing(); break;
    case REQ_OPEN_DIGI:         HandleOpenDigi(data, len); break;
    case REQ_CLOSE_DIGI:        HandleCloseDigi(data, len); break;
    case REQ_LIST_DIGI:         HandleListDigi(); break;
    case REQ_GET_DIGI_INFO:     HandleGetDigiInfo(data, len); break;
    case REQ_READ_VALUE:        HandleReadValue(data, len); break;
    case REQ_WRITE_VALUE:       HandleWriteValue(data, len); break;
    case REQ_SEND_COMMAND:      HandleSendCommand(data, len); break;
    case REQ_START_ACQ:         HandleStartACQ(data, len); break;
    case REQ_STOP_ACQ:          HandleStopACQ(data, len); break;
    case REQ_SET_DATA_FORMAT:   HandleSetDataFormat(data, len); break;
    case REQ_GET_ACQ_STATUS:    HandleGetACQStatus(); break;
    case REQ_OPEN_FILE:         HandleOpenFile(data, len); break;
    case REQ_CLOSE_FILE:        HandleCloseFile(data, len); break;
    case REQ_SET_SAVE_DATA:     HandleSetSaveData(data, len); break;
    case REQ_GET_FILE_STATUS:   HandleGetFileStatus(data, len); break;
    case REQ_READ_ALL_SETTINGS: HandleReadAllSettings(data, len); break;
    case REQ_SAVE_SETTINGS_FILE:HandleSaveSettingsFile(data, len); break;
    case REQ_LOAD_SETTINGS_FILE:HandleLoadSettingsFile(data, len); break;
    case REQ_SHUTDOWN:          HandleShutdown(); break;
    default:
      SendError("unknown message type: " + std::to_string(msgType));
      break;
  }
}

//^============================================ Response helpers

void BrokerServer::SendReply(const std::vector<uint8_t>& buf) {
  zmq_send(zmqRep, buf.data(), buf.size(), 0);
}

void BrokerServer::SendOK() {
  std::vector<uint8_t> buf;
  PackHeader(buf, RSP_OK);
  SendReply(buf);
}

void BrokerServer::SendError(const std::string& msg) {
  std::vector<uint8_t> buf;
  PackHeader(buf, RSP_ERROR);
  PackString(buf, msg);
  SendReply(buf);
  printf("ERROR: %s\n", msg.c_str());
}

void BrokerServer::SendValue(const std::string& value) {
  std::vector<uint8_t> buf;
  PackHeader(buf, RSP_VALUE);
  PackString(buf, value);
  SendReply(buf);
}

//^============================================ Command handlers

void BrokerServer::HandlePing() {
  std::vector<uint8_t> buf;
  PackHeader(buf, RSP_PONG);
  SendReply(buf);
}

void BrokerServer::HandleOpenDigi(const uint8_t* data, size_t len) {
  size_t off = 2; // skip header
  if (off >= len) { SendError("REQ_OPEN_DIGI: missing url"); return; }
  std::string url = UnpackString(data, off);

  int idx = OpenDigitizer(url);
  if (idx < 0) {
    SendError("failed to open digitizer at " + url);
  } else {
    std::vector<uint8_t> buf;
    PackHeader(buf, RSP_OK);
    PackU8(buf, static_cast<uint8_t>(idx));
    SendReply(buf);
  }
}

void BrokerServer::HandleCloseDigi(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_CLOSE_DIGI: missing index"); return; }
  uint8_t idx = UnpackU8(data, off);

  if (idx >= nDigi || !digi[idx]) {
    SendError("invalid digitizer index: " + std::to_string(idx));
    return;
  }
  CloseDigitizer(idx);
  SendOK();
}

void BrokerServer::HandleListDigi() {
  std::vector<uint8_t> buf;
  PackHeader(buf, RSP_DIGI_LIST);
  PackU8(buf, static_cast<uint8_t>(nDigi));
  for (int i = 0; i < nDigi; i++) {
    if (digi[i] && digi[i]->IsConnected()) {
      PackU8(buf, 1); // connected
      PackU16(buf, digi[i]->GetSerialNumber());
      PackString(buf, digi[i]->GetModelName());
      PackString(buf, digi[i]->GetFPGAType());
      PackU16(buf, digi[i]->GetNChannels());
    } else {
      PackU8(buf, 0); // not connected
      PackU16(buf, 0);
      PackString(buf, "");
      PackString(buf, "");
      PackU16(buf, 0);
    }
  }
  SendReply(buf);
}

void BrokerServer::HandleGetDigiInfo(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_GET_DIGI_INFO: missing index"); return; }
  uint8_t idx = UnpackU8(data, off);
  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  std::vector<uint8_t> buf;
  PackHeader(buf, RSP_DIGI_INFO);
  PackU16(buf, digi[idx]->GetSerialNumber());
  PackString(buf, digi[idx]->GetModelName());
  PackString(buf, digi[idx]->GetFPGAType());
  PackU16(buf, digi[idx]->GetNChannels());
  PackU16(buf, digi[idx]->GetTick2ns());
  PackU32(buf, digi[idx]->GetFPGAVersion());
  PackU32(buf, digi[idx]->GetCupVer());
  SendReply(buf);
}

void BrokerServer::HandleReadValue(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_READ_VALUE: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);
  std::string param = UnpackString(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  std::string val = digi[idx]->ReadValue(param.c_str());
  if (digi[idx]->GetRet() != CAEN_FELib_Success) {
    SendError("ReadValue failed for " + param);
  } else {
    SendValue(val);
  }
}

void BrokerServer::HandleWriteValue(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_WRITE_VALUE: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);
  std::string param = UnpackString(data, off);
  std::string value = UnpackString(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  bool ok = digi[idx]->WriteValue(param.c_str(), value);
  if (ok) {
    SendOK();
  } else {
    SendError("WriteValue failed for " + param + " = " + value);
  }
}

void BrokerServer::HandleSendCommand(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_SEND_COMMAND: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);
  std::string cmd = UnpackString(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  digi[idx]->SendCommand(cmd.c_str());
  SendOK();
}

void BrokerServer::HandleStartACQ(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_START_ACQ: missing data"); return; }

  uint8_t idx = UnpackU8(data, off);
  uint8_t dataFormat = UnpackU8(data, off);
  uint8_t saveData = UnpackU8(data, off);
  std::string fileNameBase;
  if (off < len) fileNameBase = UnpackString(data, off);

  int startIdx = 0, endIdx = nDigi;
  if (idx != ALL_DIGITIZERS) {
    if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }
    startIdx = idx;
    endIdx = idx + 1;
  }

  for (int i = startIdx; i < endIdx; i++) {
    if (!digi[i] || !digi[i]->IsConnected()) continue;

    std::lock_guard<std::mutex> lock(digiMutex[i]);

    digi[i]->SetDataFormat(dataFormat);

    if (saveData && !fileNameBase.empty()) {
      std::string fn = fileNameBase + "_" + std::to_string(i) +
                       "_" + std::to_string(digi[i]->GetSerialNumber());
      digi[i]->OpenOutFile(fn);
    }

    isSaveData[i] = (saveData != 0);
    digi[i]->StartACQ();

    // Reset ring buffer tracking
    for (int ch = 0; ch < MaxNumberOfChannel; ch++) {
      lastPublishedIndex[i][ch] = digi[i]->ringBuffer[ch].index();
      oldTimeStamp[i][ch] = 0;
      oldSavedCount[i][ch] = 0;
    }
    lastPublishedTraceIndex[i] = digi[i]->traceRingBuffer.index();

    // Start read thread
    readThreadStop[i] = false;
    readThread[i] = std::thread(&BrokerServer::ReadDataLoop, this, i);
  }

  PublishStatusChange(EVT_ACQ_STARTED, idx);
  PublishLog("ACQ started");
  SendOK();
}

void BrokerServer::HandleStopACQ(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_STOP_ACQ: missing data"); return; }

  uint8_t idx = UnpackU8(data, off);

  int startIdx = 0, endIdx = nDigi;
  if (idx != ALL_DIGITIZERS) {
    if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }
    startIdx = idx;
    endIdx = idx + 1;
  }

  for (int i = startIdx; i < endIdx; i++) {
    if (!digi[i] || !digi[i]->IsConnected()) continue;

    {
      std::lock_guard<std::mutex> lock(digiMutex[i]);
      digi[i]->StopACQ();
    }

    // Wait for read thread to finish
    readThreadStop[i] = true;
    if (readThread[i].joinable()) readThread[i].join();

    // Close file if saving
    if (isSaveData[i]) {
      std::lock_guard<std::mutex> lock(digiMutex[i]);
      digi[i]->CloseOutFile();
      isSaveData[i] = false;
    }
  }

  PublishStatusChange(EVT_ACQ_STOPPED, idx);
  PublishLog("ACQ stopped");
  SendOK();
}

void BrokerServer::HandleSetDataFormat(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_SET_DATA_FORMAT: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);
  uint8_t fmt = UnpackU8(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  digi[idx]->SetDataFormat(fmt);
  SendOK();
}

void BrokerServer::HandleGetACQStatus() {
  std::vector<uint8_t> buf;
  PackHeader(buf, RSP_ACQ_STATUS);
  PackU8(buf, static_cast<uint8_t>(nDigi));
  for (int i = 0; i < nDigi; i++) {
    if (digi[i] && digi[i]->IsConnected()) {
      PackU8(buf, digi[i]->IsAcqOn() ? 1 : 0);
    } else {
      PackU8(buf, 0);
    }
  }
  SendReply(buf);
}

void BrokerServer::HandleOpenFile(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_OPEN_FILE: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);
  std::string fileName = UnpackString(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  digi[idx]->OpenOutFile(fileName);
  PublishStatusChange(EVT_FILE_OPENED, idx);
  SendOK();
}

void BrokerServer::HandleCloseFile(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_CLOSE_FILE: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  digi[idx]->CloseOutFile();
  isSaveData[idx] = false;
  PublishStatusChange(EVT_FILE_CLOSED, idx);
  SendOK();
}

void BrokerServer::HandleSetSaveData(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_SET_SAVE_DATA: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);
  uint8_t onOff = UnpackU8(data, off);

  if (idx >= nDigi) { SendError("invalid digi index"); return; }
  isSaveData[idx] = (onOff != 0);
  SendOK();
}

void BrokerServer::HandleGetFileStatus(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_GET_FILE_STATUS: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  std::vector<uint8_t> buf;
  PackHeader(buf, RSP_FILE_STATUS);
  PackU64(buf, digi[idx]->GetTotalFilesSize());
  PackU32(buf, digi[idx]->GetFileSize());
  SendReply(buf);
}

void BrokerServer::HandleReadAllSettings(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_READ_ALL_SETTINGS: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  digi[idx]->ReadAllSettings();
  SendOK();
}

void BrokerServer::HandleSaveSettingsFile(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_SAVE_SETTINGS_FILE: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);
  std::string fileName = UnpackString(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  int ret = digi[idx]->SaveSettingsToFile(fileName.empty() ? nullptr : fileName.c_str());
  if (ret == 0) {
    SendOK();
  } else {
    SendError("SaveSettingsToFile failed");
  }
}

void BrokerServer::HandleLoadSettingsFile(const uint8_t* data, size_t len) {
  size_t off = 2;
  if (off >= len) { SendError("REQ_LOAD_SETTINGS_FILE: missing data"); return; }
  uint8_t idx = UnpackU8(data, off);
  std::string fileName = UnpackString(data, off);

  if (idx >= nDigi || !digi[idx]) { SendError("invalid digi index"); return; }

  std::lock_guard<std::mutex> lock(digiMutex[idx]);
  bool ok = digi[idx]->LoadSettingsFromFile(fileName.empty() ? nullptr : fileName.c_str());
  if (ok) {
    SendOK();
  } else {
    SendError("LoadSettingsFromFile failed");
  }
}

void BrokerServer::HandleShutdown() {
  printf("Shutdown requested by client.\n");
  SendOK();
  Stop();
}

//^============================================ Background workers

void BrokerServer::ReadDataLoop(int digiIndex) {
  printf("ReadDataLoop started for digi %d (SN=%d)\n",
         digiIndex, digi[digiIndex]->GetSerialNumber());

  while (!readThreadStop[digiIndex]) {
    int ret;
    {
      std::lock_guard<std::mutex> lock(digiMutex[digiIndex]);
      ret = digi[digiIndex]->ReadData();

      if (ret == CAEN_FELib_Success && isSaveData[digiIndex]) {
        digi[digiIndex]->SaveDataToFile();
      }
    }

    if (ret == CAEN_FELib_Stop) {
      digi[digiIndex]->hit->ClearTrace();
      break;
    }

    // Yield briefly so ScalarBroadcastLoop and command handlers can acquire the mutex
    if (ret != CAEN_FELib_Success) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  printf("ReadDataLoop stopped for digi %d\n", digiIndex);
}

void BrokerServer::ScalarBroadcastLoop() {
  printf("ScalarBroadcastLoop started (interval=%.1f s)\n", scalarIntervalSec);

  while (!scalarThreadStop) {
    // Sleep in small increments so we can stop quickly
    int sleepMs = static_cast<int>(scalarIntervalSec * 1000);
    for (int t = 0; t < sleepMs && !scalarThreadStop; t += 100) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (scalarThreadStop) break;

    for (int i = 0; i < nDigi; i++) {
      if (!digi[i] || !digi[i]->IsConnected()) continue;

      PublishScalar(i);

      if (digi[i]->IsAcqOn()) {
        PublishHitSummaries(i);
        PublishTraceSnapshot(i);
      }
    }
  }

  printf("ScalarBroadcastLoop stopped.\n");
}

//^============================================ Publish helpers

void BrokerServer::PublishScalar(int digiIndex) {
  int nCh = std::min((int)digi[digiIndex]->GetNChannels(), (int)MaxNumberOfChannel);

  std::vector<uint8_t> buf;
  PackHeader(buf, PUB_SCALAR);
  PackU8(buf, static_cast<uint8_t>(digiIndex));
  PackU16(buf, digi[digiIndex]->GetSerialNumber());
  PackU8(buf, static_cast<uint8_t>(nCh));

  std::lock_guard<std::mutex> lock(digiMutex[digiIndex]);

  for (int ch = 0; ch < nCh; ch++) {
    // Read scalar values from digitizer (same as MainWindow::UpdateScalar)
    std::string timeStr = digi[digiIndex]->ReadValue(PHA::CH::ChannelRealtime, ch);
    std::string rateStr = digi[digiIndex]->ReadValue(PHA::CH::SelfTrgRate, ch);
    std::string countStr = digi[digiIndex]->ReadValue(PHA::CH::ChannelSavedCount, ch);

    uint32_t trgRate = 0;
    uint64_t realTime = 0;
    uint64_t savedCount = 0;
    float acceptRate = 0.0f;

    try { trgRate = std::stoul(rateStr); } catch (...) {}
    try { realTime = std::stoull(timeStr); } catch (...) {}
    try { savedCount = std::stoull(countStr); } catch (...) {}

    if (digi[digiIndex]->GetModelName() == "VX2730") { realTime /= 4; }

    // Calculate accept rate
    if (oldTimeStamp[digiIndex][ch] > 0 &&
        realTime > oldTimeStamp[digiIndex][ch] &&
        realTime - oldTimeStamp[digiIndex][ch] > 1000000000ULL &&
        savedCount > oldSavedCount[digiIndex][ch]) {
      acceptRate = static_cast<float>(
        (savedCount - oldSavedCount[digiIndex][ch]) * 1e9 /
        (realTime - oldTimeStamp[digiIndex][ch]));
    }

    oldTimeStamp[digiIndex][ch] = realTime;
    oldSavedCount[digiIndex][ch] = savedCount;

    PackU32(buf, trgRate);
    PackU64(buf, savedCount);
    PackFloat(buf, acceptRate);
    PackU64(buf, realTime);
  }

  // Footer
  PackU64(buf, digi[digiIndex]->GetTotalFilesSize());
  PackU8(buf, digi[digiIndex]->IsAcqOn() ? 1 : 0);

  zmq_send(zmqPub, buf.data(), buf.size(), 0);
}

void BrokerServer::PublishHitSummaries(int digiIndex) {
  int nCh = std::min((int)digi[digiIndex]->GetNChannels(), (int)MaxNumberOfChannel);

  // Collect new hits from ring buffers
  std::vector<uint8_t> buf;
  PackHeader(buf, PUB_HIT_SUMMARY);
  PackU8(buf, static_cast<uint8_t>(digiIndex));

  // Reserve space for nHits count
  size_t nHitsPos = buf.size();
  PackU16(buf, 0); // placeholder

  uint16_t nHits = 0;

  for (int ch = 0; ch < nCh; ch++) {
    unsigned long currentIdx = digi[digiIndex]->ringBuffer[ch].index();
    unsigned long lastIdx = lastPublishedIndex[digiIndex][ch];

    if (currentIdx <= lastIdx) continue;

    // Limit to last RingBufferSize entries if we fell behind
    if (currentIdx - lastIdx > RingBufferSize) {
      lastIdx = currentIdx - RingBufferSize;
    }

    // Batch at most 1000 hits per channel per publish
    unsigned long count = std::min(currentIdx - lastIdx, (unsigned long)1000);
    unsigned long startIdx = currentIdx - count;

    for (unsigned long j = startIdx; j < currentIdx; j++) {
      HitSummary hs = digi[digiIndex]->ringBuffer[ch].at(j);
      PackU8(buf, static_cast<uint8_t>(ch));
      PackU16(buf, hs.energy);
      PackU16(buf, hs.energy_short);
      nHits++;
    }

    lastPublishedIndex[digiIndex][ch] = currentIdx;
  }

  if (nHits == 0) return; // nothing to publish

  // Patch nHits count
  buf[nHitsPos]     = static_cast<uint8_t>(nHits & 0xFF);
  buf[nHitsPos + 1] = static_cast<uint8_t>((nHits >> 8) & 0xFF);

  zmq_send(zmqPub, buf.data(), buf.size(), 0);
}

void BrokerServer::PublishTraceSnapshot(int digiIndex) {
  unsigned long currentIdx = digi[digiIndex]->traceRingBuffer.index();
  if (currentIdx <= lastPublishedTraceIndex[digiIndex]) return;

  // Get the latest trace
  const TraceSnapshot& ts = digi[digiIndex]->traceRingBuffer.at(currentIdx - 1);
  if (ts.traceLenght == 0) return;

  std::vector<uint8_t> buf;
  PackHeader(buf, PUB_TRACE);
  PackU8(buf, static_cast<uint8_t>(digiIndex));
  // channel info from the hit
  PackU8(buf, digi[digiIndex]->hit ? digi[digiIndex]->hit->channel : 0);
  PackU32(buf, static_cast<uint32_t>(ts.traceLenght));

  // Analog probes
  for (int p = 0; p < 2; p++) {
    for (size_t s = 0; s < ts.traceLenght; s++) {
      PackU32(buf, static_cast<uint32_t>(ts.analog_probes[p][s]));
    }
  }
  // Digital probes
  for (int p = 0; p < 4; p++) {
    buf.insert(buf.end(), ts.digital_probes[p], ts.digital_probes[p] + ts.traceLenght);
  }

  lastPublishedTraceIndex[digiIndex] = currentIdx;
  zmq_send(zmqPub, buf.data(), buf.size(), 0);
}

void BrokerServer::PublishStatusChange(StatusEvent event, uint8_t digiIndex) {
  if (!zmqPub) return;
  std::vector<uint8_t> buf;
  PackHeader(buf, PUB_STATUS_CHANGE);
  PackU8(buf, static_cast<uint8_t>(event));
  PackU8(buf, digiIndex);
  zmq_send(zmqPub, buf.data(), buf.size(), 0);
}

void BrokerServer::PublishLog(const std::string& msg) {
  if (!zmqPub) return;
  std::vector<uint8_t> buf;
  PackHeader(buf, PUB_LOG_MESSAGE);
  PackString(buf, msg);
  zmq_send(zmqPub, buf.data(), buf.size(), 0);
}
