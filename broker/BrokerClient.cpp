#include "BrokerClient.h"

#include <zmq.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

BrokerClient::BrokerClient() {
  zmqCtx = zmq_ctx_new();
  zmqReq = nullptr;
  zmqSub = nullptr;
  connected = false;
  subThreadStop = false;

  memset(scalarData, 0, sizeof(scalarData));
  sub = std::make_unique<SubData>();
}

BrokerClient::~BrokerClient() {
  Disconnect();
  if (zmqCtx) zmq_ctx_destroy(zmqCtx);
}

int BrokerClient::Connect(const std::string& cmdEndpoint,
                           const std::string& pubEndpoint) {
  if (connected) Disconnect();

  zmqReq = zmq_socket(zmqCtx, ZMQ_REQ);
  if (!zmqReq) { lastError = "cannot create REQ socket"; return -1; }

  // Set send/recv timeouts (shorter for probe/ping, normal for full connection)
  int timeout = pubEndpoint.empty() ? 1000 : 5000;
  zmq_setsockopt(zmqReq, ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
  zmq_setsockopt(zmqReq, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

  if (zmq_connect(zmqReq, cmdEndpoint.c_str()) != 0) {
    lastError = "cannot connect REQ to " + cmdEndpoint + ": " + zmq_strerror(errno);
    return -1;
  }

  connected = true;

  // SUB socket is optional (empty pubEndpoint = command-only connection for ping/probe)
  if (!pubEndpoint.empty()) {
    zmqSub = zmq_socket(zmqCtx, ZMQ_SUB);
    if (!zmqSub) { lastError = "cannot create SUB socket"; return -1; }

    // Subscribe to all messages
    zmq_setsockopt(zmqSub, ZMQ_SUBSCRIBE, "", 0);

    if (zmq_connect(zmqSub, pubEndpoint.c_str()) != 0) {
      lastError = "cannot connect SUB to " + pubEndpoint + ": " + zmq_strerror(errno);
      connected = false;
      return -1;
    }

    // Start subscription thread
    subThreadStop = false;
    subThread = std::thread(&BrokerClient::SubscriptionLoop, this);
  }

  return 0;
}

void BrokerClient::Disconnect() {
  connected = false;

  subThreadStop = true;
  if (subThread.joinable()) subThread.join();

  if (zmqReq) { zmq_close(zmqReq); zmqReq = nullptr; }
  if (zmqSub) { zmq_close(zmqSub); zmqSub = nullptr; }
}

//^============================================ REQ/REP helpers

std::vector<uint8_t> BrokerClient::SendRequest(const std::vector<uint8_t>& request) {
  std::vector<uint8_t> empty;
  if (!connected || !zmqReq) {
    lastError = "not connected";
    return empty;
  }

  int sent = zmq_send(zmqReq, request.data(), request.size(), 0);
  if (sent < 0) {
    lastError = std::string("send failed: ") + zmq_strerror(errno);
    return empty;
  }

  zmq_msg_t reply;
  zmq_msg_init(&reply);
  int size = zmq_msg_recv(&reply, zmqReq, 0);
  if (size < 0) {
    lastError = std::string("recv timeout: ") + zmq_strerror(errno);
    zmq_msg_close(&reply);
    return empty;
  }

  std::vector<uint8_t> result(static_cast<uint8_t*>(zmq_msg_data(&reply)),
                              static_cast<uint8_t*>(zmq_msg_data(&reply)) + size);
  zmq_msg_close(&reply);
  return result;
}

bool BrokerClient::CheckOK(const std::vector<uint8_t>& response) {
  if (response.size() < 2) {
    lastError = "empty response";
    return false;
  }
  size_t off = 0;
  uint8_t type = UnpackHeader(response.data(), off);
  if (type == RSP_OK) return true;
  if (type == RSP_ERROR && response.size() > 2) {
    lastError = UnpackString(response.data(), off);
  } else {
    lastError = "unexpected response type: " + std::to_string(type);
  }
  return false;
}

std::string BrokerClient::ExtractValue(const std::vector<uint8_t>& response) {
  if (response.size() < 2) { lastError = "empty response"; return ""; }
  size_t off = 0;
  uint8_t type = UnpackHeader(response.data(), off);
  if (type == RSP_VALUE) {
    return UnpackString(response.data(), off);
  }
  if (type == RSP_ERROR && response.size() > 2) {
    lastError = UnpackString(response.data(), off);
  }
  return "";
}

//^============================================ Digitizer management

int BrokerClient::OpenDigitizer(const std::string& url) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_OPEN_DIGI);
  PackString(req, url);
  auto rsp = SendRequest(req);
  if (rsp.size() < 3) { CheckOK(rsp); return -1; }
  size_t off = 0;
  uint8_t type = UnpackHeader(rsp.data(), off);
  if (type == RSP_OK) {
    return UnpackU8(rsp.data(), off);
  }
  CheckOK(rsp);
  return -1;
}

void BrokerClient::CloseDigitizer(int index) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_CLOSE_DIGI);
  PackU8(req, static_cast<uint8_t>(index));
  CheckOK(SendRequest(req));
}

int BrokerClient::GetNumDigitizers() {
  auto list = ListDigitizers();
  return static_cast<int>(list.size());
}

std::vector<BrokerClient::DigiInfo> BrokerClient::ListDigitizers() {
  std::vector<DigiInfo> result;
  std::vector<uint8_t> req;
  PackHeader(req, REQ_LIST_DIGI);
  auto rsp = SendRequest(req);
  if (rsp.size() < 3) return result;

  size_t off = 0;
  uint8_t type = UnpackHeader(rsp.data(), off);
  if (type != RSP_DIGI_LIST) return result;

  uint8_t nDigi = UnpackU8(rsp.data(), off);
  for (int i = 0; i < nDigi && off < rsp.size(); i++) {
    DigiInfo info;
    info.isConnected = (UnpackU8(rsp.data(), off) != 0);
    info.serialNumber = UnpackU16(rsp.data(), off);
    info.modelName = UnpackString(rsp.data(), off);
    info.fpgaType = UnpackString(rsp.data(), off);
    info.nChannels = UnpackU16(rsp.data(), off);
    info.tick2ns = 0;
    info.fpgaVersion = 0;
    info.cupVersion = 0;
    result.push_back(info);
  }
  return result;
}

BrokerClient::DigiInfo BrokerClient::GetDigiInfo(int index) {
  DigiInfo info = {};
  std::vector<uint8_t> req;
  PackHeader(req, REQ_GET_DIGI_INFO);
  PackU8(req, static_cast<uint8_t>(index));
  auto rsp = SendRequest(req);
  if (rsp.size() < 2) return info;

  size_t off = 0;
  uint8_t type = UnpackHeader(rsp.data(), off);
  if (type != RSP_DIGI_INFO) return info;

  info.isConnected = true;
  info.serialNumber = UnpackU16(rsp.data(), off);
  info.modelName = UnpackString(rsp.data(), off);
  info.fpgaType = UnpackString(rsp.data(), off);
  info.nChannels = UnpackU16(rsp.data(), off);
  info.tick2ns = UnpackU16(rsp.data(), off);
  info.fpgaVersion = UnpackU32(rsp.data(), off);
  info.cupVersion = UnpackU32(rsp.data(), off);
  return info;
}

//^============================================ Parameter access

std::string BrokerClient::ReadValue(int digiIndex, const std::string& parameter) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_READ_VALUE);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackString(req, parameter);
  return ExtractValue(SendRequest(req));
}

bool BrokerClient::WriteValue(int digiIndex, const std::string& parameter, const std::string& value) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_WRITE_VALUE);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackString(req, parameter);
  PackString(req, value);
  return CheckOK(SendRequest(req));
}

void BrokerClient::SendCommand(int digiIndex, const std::string& command) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_SEND_COMMAND);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackString(req, command);
  CheckOK(SendRequest(req));
}

//^============================================ Acquisition control

void BrokerClient::StartACQ(int digiIndex, int dataFormat, bool saveData,
                             const std::string& fileNameBase) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_START_ACQ);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackU8(req, static_cast<uint8_t>(dataFormat));
  PackU8(req, saveData ? 1 : 0);
  PackString(req, fileNameBase);
  CheckOK(SendRequest(req));
}

void BrokerClient::StopACQ(int digiIndex) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_STOP_ACQ);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  CheckOK(SendRequest(req));
}

BrokerClient::ACQStatus BrokerClient::GetACQStatus() {
  ACQStatus status = {};
  std::vector<uint8_t> req;
  PackHeader(req, REQ_GET_ACQ_STATUS);
  auto rsp = SendRequest(req);
  if (rsp.size() < 3) return status;

  size_t off = 0;
  uint8_t type = UnpackHeader(rsp.data(), off);
  if (type != RSP_ACQ_STATUS) return status;

  status.nDigi = UnpackU8(rsp.data(), off);
  for (int i = 0; i < status.nDigi && off < rsp.size(); i++) {
    status.acqOn[i] = (UnpackU8(rsp.data(), off) != 0);
  }
  return status;
}

//^============================================ File control

void BrokerClient::SetDataFormat(int digiIndex, int format) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_SET_DATA_FORMAT);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackU8(req, static_cast<uint8_t>(format));
  CheckOK(SendRequest(req));
}

void BrokerClient::OpenFile(int digiIndex, const std::string& fileName) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_OPEN_FILE);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackString(req, fileName);
  CheckOK(SendRequest(req));
}

void BrokerClient::CloseFile(int digiIndex) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_CLOSE_FILE);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  CheckOK(SendRequest(req));
}

void BrokerClient::SetSaveData(int digiIndex, bool onOff) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_SET_SAVE_DATA);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackU8(req, onOff ? 1 : 0);
  CheckOK(SendRequest(req));
}

BrokerClient::FileStatus BrokerClient::GetFileStatus(int digiIndex) {
  FileStatus fs = {};
  std::vector<uint8_t> req;
  PackHeader(req, REQ_GET_FILE_STATUS);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  auto rsp = SendRequest(req);
  if (rsp.size() < 2) return fs;

  size_t off = 0;
  uint8_t type = UnpackHeader(rsp.data(), off);
  if (type != RSP_FILE_STATUS) return fs;

  fs.totalFileSize = UnpackU64(rsp.data(), off);
  fs.currentFileSize = UnpackU32(rsp.data(), off);
  return fs;
}

//^============================================ Settings

void BrokerClient::ReadAllSettings(int digiIndex) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_READ_ALL_SETTINGS);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  CheckOK(SendRequest(req));
}

void BrokerClient::SaveSettingsFile(int digiIndex, const std::string& fileName) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_SAVE_SETTINGS_FILE);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackString(req, fileName);
  CheckOK(SendRequest(req));
}

void BrokerClient::LoadSettingsFile(int digiIndex, const std::string& fileName) {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_LOAD_SETTINGS_FILE);
  PackU8(req, static_cast<uint8_t>(digiIndex));
  PackString(req, fileName);
  CheckOK(SendRequest(req));
}

//^============================================ Lifecycle

bool BrokerClient::Ping() {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_PING);
  auto rsp = SendRequest(req);
  if (rsp.size() < 2) return false;
  size_t off = 0;
  return UnpackHeader(rsp.data(), off) == RSP_PONG;
}

void BrokerClient::Shutdown() {
  std::vector<uint8_t> req;
  PackHeader(req, REQ_SHUTDOWN);
  SendRequest(req);  // might not get reply if broker shuts down fast
}

//^============================================ Subscription thread

void BrokerClient::SubscriptionLoop() {
  zmq_pollitem_t items[1];
  items[0] = { zmqSub, 0, ZMQ_POLLIN, 0 };

  while (!subThreadStop) {
    int rc = zmq_poll(items, 1, 200); // 200ms timeout
    if (rc <= 0) continue;

    if (!(items[0].revents & ZMQ_POLLIN)) continue;

    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int size = zmq_msg_recv(&msg, zmqSub, 0);
    if (size < 2) { zmq_msg_close(&msg); continue; }

    const uint8_t* data = static_cast<const uint8_t*>(zmq_msg_data(&msg));
    size_t off = 0;
    uint8_t pubType = UnpackHeader(data, off);

    switch (pubType) {
      case PUB_SCALAR: {
        uint8_t digiIdx = UnpackU8(data, off);
        if (digiIdx >= MaxNumberOfDigitizer) break;

        {
          std::lock_guard<std::mutex> lock(scalarMutex);
          scalarData[digiIdx].serialNumber = UnpackU16(data, off);
          scalarData[digiIdx].nChannels = UnpackU8(data, off);
          int nCh = scalarData[digiIdx].nChannels;

          for (int ch = 0; ch < nCh && off < (size_t)size; ch++) {
            scalarData[digiIdx].trgRate[ch]    = UnpackU32(data, off);
            scalarData[digiIdx].savedCount[ch] = UnpackU64(data, off);
            scalarData[digiIdx].acceptRate[ch]  = UnpackFloat(data, off);
            scalarData[digiIdx].realTime[ch]    = UnpackU64(data, off);
          }
          scalarData[digiIdx].totalFileSize = UnpackU64(data, off);
          scalarData[digiIdx].acqOn = (UnpackU8(data, off) != 0);
        } // unlock before callback

        if (onScalarUpdate) onScalarUpdate(digiIdx);
        break;
      }

      case PUB_HIT_SUMMARY: {
        uint8_t digiIdx = UnpackU8(data, off);
        if (digiIdx >= MaxNumberOfDigitizer) break;
        uint16_t nHits = UnpackU16(data, off);

        for (int h = 0; h < nHits && off + 5 <= (size_t)size; h++) {
          uint8_t ch = UnpackU8(data, off);
          uint16_t energy = UnpackU16(data, off);
          uint16_t energy_short = UnpackU16(data, off);
          if (ch < MaxNumberOfChannel) {
            sub->ringBuffer[digiIdx][ch].push({energy, energy_short});
          }
        }

        if (onHitSummary) onHitSummary(digiIdx, nHits);
        break;
      }

      case PUB_TRACE: {
        uint8_t digiIdx = UnpackU8(data, off);
        if (digiIdx >= MaxNumberOfDigitizer) break;
        /*uint8_t ch =*/ UnpackU8(data, off); // channel
        uint32_t traceLen = UnpackU32(data, off);

        TraceSnapshot& ts = sub->traceRingBuffer[digiIdx].nextSlot();
        ts.traceLenght = std::min(traceLen, (uint32_t)MaxTraceLenght);

        // Analog probes
        for (int p = 0; p < 2; p++) {
          for (size_t s = 0; s < ts.traceLenght && off + 4 <= (size_t)size; s++) {
            ts.analog_probes[p][s] = static_cast<int32_t>(UnpackU32(data, off));
          }
        }
        // Digital probes
        for (int p = 0; p < 4; p++) {
          size_t copyLen = std::min(ts.traceLenght, (size_t)(size - off));
          memcpy(ts.digital_probes[p], data + off, copyLen);
          off += copyLen;
        }

        sub->traceRingBuffer[digiIdx].advance();
        if (onTraceSnapshot) onTraceSnapshot(digiIdx);
        break;
      }

      case PUB_STATUS_CHANGE: {
        uint8_t event = UnpackU8(data, off);
        uint8_t digiIdx = UnpackU8(data, off);
        if (onStatusChange) onStatusChange(static_cast<StatusEvent>(event), digiIdx);
        break;
      }

      case PUB_LOG_MESSAGE: {
        std::string logMsg = UnpackString(data, off);
        if (onLogMessage) onLogMessage(logMsg);
        break;
      }
    }

    zmq_msg_close(&msg);
  }
}
