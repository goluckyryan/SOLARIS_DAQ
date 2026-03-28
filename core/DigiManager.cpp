#include "DigiManager.h"

#include <cstring>
#include <chrono>

DigiManager::DigiManager(Mode mode) : mode(mode), nDigi(0), client(nullptr) {
  for (int i = 0; i < MaxNumberOfDigitizer; i++) {
    digi[i] = nullptr;
    readThreadStop[i] = false;
    isSaveData[i] = false;
  }
  if (mode == Mode::Broker) {
    client = new BrokerClient();
  }
}

DigiManager::~DigiManager() {
  CloseAll();
  if (client) {
    delete client;
    client = nullptr;
  }
}

//============================================ Connection (broker mode)
int DigiManager::Connect(const std::string& cmdEndpoint, const std::string& pubEndpoint) {
  if (mode != Mode::Broker || !client) return -1;
  int ret = client->Connect(cmdEndpoint, pubEndpoint);
  if (ret != 0) return ret;

  // Forward broker callbacks
  client->onScalarUpdate  = [this](int i) { if (onScalarUpdate)  onScalarUpdate(i); };
  client->onHitSummary    = [this](int i, int n) { if (onHitSummary) onHitSummary(i, n); };
  client->onTraceSnapshot = [this](int i) { if (onTraceSnapshot) onTraceSnapshot(i); };
  client->onLogMessage    = [this](const std::string& m) { if (onLogMessage) onLogMessage(m); };

  // Sync existing digitizers from broker
  auto list = client->ListDigitizers();
  nDigi = (int)list.size();
  for (int i = 0; i < nDigi; i++) {
    if (!list[i].isConnected) continue;

    infoCache[i].serialNumber = list[i].serialNumber;
    infoCache[i].modelName    = list[i].modelName;
    infoCache[i].fpgaType     = list[i].fpgaType;
    infoCache[i].nChannels    = list[i].nChannels;
    infoCache[i].isConnected  = true;
    infoCache[i].isDummy      = false;
    RefreshDigiInfo(i);  // get tick2ns, fpgaVersion, cupVersion

    // Create a local dummy Digitizer2Gen for UI construction
    digi[i] = new Digitizer2Gen();
    digi[i]->SetDummy(infoCache[i].serialNumber);
  }

  return 0;
}

void DigiManager::Disconnect() {
  if (mode != Mode::Broker || !client) return;
  // Clean up local dummies
  for (int i = 0; i < nDigi; i++) {
    if (digi[i]) { delete digi[i]; digi[i] = nullptr; }
  }
  client->Disconnect();
  nDigi = 0;
}

//============================================ Digitizer lifecycle
int DigiManager::OpenDigitizer(const std::string& url) {
  if (mode == Mode::Standalone) {
    if (nDigi >= MaxNumberOfDigitizer) return -1;
    int idx = nDigi;
    digi[idx] = new Digitizer2Gen();
    digi[idx]->OpenDigitizer(url.c_str());
    if (!digi[idx]->IsConnected()) {
      digi[idx]->SetDummy(idx);
    }
    nDigi++;
    return idx;

  } else { // Broker
    if (!client || !client->IsConnected()) return -1;
    int idx = client->OpenDigitizer(url);
    if (idx >= 0) {
      nDigi = std::max(nDigi, idx + 1);
      RefreshDigiInfo(idx);
      // Create local dummy for UI construction (settings panel, etc.)
      if (!digi[idx]) {
        digi[idx] = new Digitizer2Gen();
        digi[idx]->SetDummy(infoCache[idx].serialNumber);
      }
    }
    return idx;
  }
}

void DigiManager::CloseDigitizer(int index) {
  if (index < 0 || index >= nDigi) return;

  if (mode == Mode::Standalone) {
    if (digi[index]) {
      // Stop read thread if running
      if (readThread[index].joinable()) {
        readThreadStop[index] = true;
        readThread[index].join();
      }
      digi[index]->CloseDigitizer();
      delete digi[index];
      digi[index] = nullptr;
    }
  } else {
    // Broker mode: only clean up local dummy, don't close remote digitizer
    if (digi[index]) { delete digi[index]; digi[index] = nullptr; }
    infoCache[index] = DigiInfoCache();
  }
}

void DigiManager::CloseAll() {
  if (mode == Mode::Standalone) {
    for (int i = 0; i < nDigi; i++) {
      CloseDigitizer(i);
    }
  } else {
    // Broker mode: clean up local dummies only, leave remote digitizers running
    for (int i = 0; i < nDigi; i++) {
      if (digi[i]) { delete digi[i]; digi[i] = nullptr; }
      infoCache[i] = DigiInfoCache();
    }
    Disconnect();
  }
  nDigi = 0;
}

//============================================ Info queries
int DigiManager::GetNChannels(int d) const {
  if (d < 0 || d >= nDigi) return 0;
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->GetNChannels() : 0;
  return infoCache[d].nChannels;
}

uint16_t DigiManager::GetSerialNumber(int d) const {
  if (d < 0 || d >= nDigi) return 0;
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->GetSerialNumber() : 0;
  return infoCache[d].serialNumber;
}

std::string DigiManager::GetModelName(int d) const {
  if (d < 0 || d >= nDigi) return "";
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->GetModelName() : "";
  return infoCache[d].modelName;
}

std::string DigiManager::GetFPGAType(int d) const {
  if (d < 0 || d >= nDigi) return "";
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->GetFPGAType() : "";
  return infoCache[d].fpgaType;
}

unsigned short DigiManager::GetTick2ns(int d) const {
  if (d < 0 || d >= nDigi) return 0;
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->GetTick2ns() : 0;
  return infoCache[d].tick2ns;
}

bool DigiManager::IsDummy(int d) const {
  if (d < 0 || d >= nDigi) return true;
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->IsDummy() : true;
  return infoCache[d].isDummy;
}

bool DigiManager::IsDigiConnected(int d) const {
  if (d < 0 || d >= nDigi) return false;
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->IsConnected() : false;
  return infoCache[d].isConnected;
}

//============================================ Parameter access
std::string DigiManager::ReadValue(int d, const Reg& reg, int ch) {
  if (d < 0 || d >= nDigi) return "";
  if (mode == Mode::Standalone) {
    if (!digi[d] || digi[d]->IsDummy()) return "";
    std::lock_guard<std::mutex> lock(digiMutex[d]);
    return digi[d]->ReadValue(reg, ch);
  } else {
    if (!client || !client->IsConnected()) return "";
    return client->ReadValue(d, reg.GetFullPara(ch));
  }
}

bool DigiManager::WriteValue(int d, const Reg& reg, const std::string& value, int ch) {
  if (d < 0 || d >= nDigi) return false;
  if (mode == Mode::Standalone) {
    if (!digi[d] || digi[d]->IsDummy()) return false;
    std::lock_guard<std::mutex> lock(digiMutex[d]);
    return digi[d]->WriteValue(reg, value, ch);
  } else {
    if (!client || !client->IsConnected()) return false;
    return client->WriteValue(d, reg.GetFullPara(ch), value);
  }
}

void DigiManager::SendCommand(int d, const Reg& reg) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    if (!digi[d] || digi[d]->IsDummy()) return;
    std::lock_guard<std::mutex> lock(digiMutex[d]);
    digi[d]->SendCommand(reg.GetFullPara());
  } else {
    if (!client || !client->IsConnected()) return;
    client->SendCommand(d, reg.GetFullPara());
  }
}

void DigiManager::SendCommand(int d, const std::string& cmd) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    if (!digi[d] || digi[d]->IsDummy()) return;
    std::lock_guard<std::mutex> lock(digiMutex[d]);
    digi[d]->SendCommand(cmd);
  } else {
    if (!client || !client->IsConnected()) return;
    client->SendCommand(d, cmd);
  }
}

//============================================ ACQ control
void DigiManager::StartACQ(int d, int dataFormat, bool saveData, const std::string& fileNameBase) {
  if (d < 0 || d >= nDigi) return;

  if (mode == Mode::Standalone) {
    if (!digi[d] || digi[d]->IsDummy()) return;
    digi[d]->SetDataFormat(dataFormat);
    if (saveData && !fileNameBase.empty()) {
      digi[d]->OpenOutFile(fileNameBase);
    }
    isSaveData[d] = saveData;
    digi[d]->StartACQ();

    // Launch read thread
    readThreadStop[d] = false;
    readThread[d] = std::thread(&DigiManager::ReadDataLoop, this, d);

  } else {
    if (!client || !client->IsConnected()) return;
    client->StartACQ(d, dataFormat, saveData, fileNameBase);
  }
}

void DigiManager::StopACQ(int d) {
  if (d < 0 || d >= nDigi) return;

  if (mode == Mode::Standalone) {
    if (!digi[d] || digi[d]->IsDummy()) return;

    {
      std::lock_guard<std::mutex> lock(digiMutex[d]);
      digi[d]->StopACQ();
    }

    // Wait for read thread
    if (readThread[d].joinable()) {
      readThreadStop[d] = true;
      readThread[d].join();
    }

    digi[d]->CloseOutFile();

  } else {
    if (!client || !client->IsConnected()) return;
    client->StopACQ(d);
  }
}

bool DigiManager::IsACQOn(int d) const {
  if (d < 0 || d >= nDigi) return false;
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->IsAcqOn() : false;
  // In broker mode, check cached scalar data
  if (client) {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(client->scalarMutex));
    return client->scalarData[d].acqOn;
  }
  return false;
}

//============================================ File I/O
void DigiManager::SetSaveData(int d, bool onOff) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    isSaveData[d] = onOff;
  } else {
    if (client && client->IsConnected()) client->SetSaveData(d, onOff);
  }
}

void DigiManager::OpenFile(int d, const std::string& fileName) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    if (digi[d]) digi[d]->OpenOutFile(fileName);
  } else {
    if (client && client->IsConnected()) client->OpenFile(d, fileName);
  }
}

void DigiManager::CloseFile(int d) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    if (digi[d]) digi[d]->CloseOutFile();
  } else {
    if (client && client->IsConnected()) client->CloseFile(d);
  }
}

uint64_t DigiManager::GetTotalFileSize(int d) const {
  if (d < 0 || d >= nDigi) return 0;
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->GetTotalFilesSize() : 0;
  if (client && client->IsConnected()) {
    auto fs = const_cast<BrokerClient*>(client)->GetFileStatus(d);
    return fs.totalFileSize;
  }
  return 0;
}

//============================================ Settings
void DigiManager::SaveSettings(int d, const std::string& fileName) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    if (digi[d]) digi[d]->SaveSettingsToFile(fileName.empty() ? NULL : fileName.c_str());
  } else {
    if (client && client->IsConnected()) client->SaveSettingsFile(d, fileName);
  }
}

void DigiManager::ReadAllSettings(int d) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    if (digi[d]) digi[d]->ReadAllSettings();
  } else {
    if (client && client->IsConnected()) client->ReadAllSettings(d);
  }
}

void DigiManager::LoadSettings(int d, const std::string& fileName) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    if (digi[d]) digi[d]->LoadSettingsFromFile(fileName.empty() ? NULL : fileName.c_str());
  } else {
    if (client && client->IsConnected()) client->LoadSettingsFile(d, fileName);
  }
}

void DigiManager::SetSettingFileName(int d, const std::string& fileName) {
  if (d < 0 || d >= nDigi) return;
  if (mode == Mode::Standalone) {
    if (digi[d]) digi[d]->SetSettingFileName(fileName);
  } else {
    infoCache[d].settingFileName = fileName;
  }
}

std::string DigiManager::GetSettingFileName(int d) const {
  if (d < 0 || d >= nDigi) return "";
  if (mode == Mode::Standalone) return digi[d] ? digi[d]->GetSettingFileName() : "";
  return infoCache[d].settingFileName;
}

//============================================ Data access
RingBuffer<HitSummary, RingBufferSize>& DigiManager::GetRingBuffer(int d, int ch) {
  if (mode == Mode::Standalone) {
    return digi[d]->ringBuffer[ch];
  } else {
    return client->GetRingBuffer(d, ch);
  }
}

RingBuffer<TraceSnapshot, TraceRingBufferSize>& DigiManager::GetTraceRingBuffer(int d) {
  if (mode == Mode::Standalone) {
    return digi[d]->traceRingBuffer;
  } else {
    return client->GetTraceRingBuffer(d);
  }
}

//============================================ Scalar data
DigiManager::ScalarSnapshot DigiManager::GetScalarSnapshot(int d) {
  ScalarSnapshot snap = {};
  if (d < 0 || d >= nDigi) return snap;

  if (mode == Mode::Standalone) {
    if (!digi[d] || digi[d]->IsDummy()) return snap;
    // Poll values from digitizer (same as old MainWindow::UpdateScalar)
    int nCh = digi[d]->GetNChannels();
    for (int ch = 0; ch < nCh; ch++) {
      std::string timeStr  = digi[d]->ReadValue(PHA::CH::ChannelRealtime, ch);
      std::string rateStr  = digi[d]->ReadValue(PHA::CH::SelfTrgRate, ch);
      std::string countStr = digi[d]->ReadValue(PHA::CH::ChannelSavedCount, ch);
      snap.realTime[ch]   = timeStr.empty()  ? 0 : std::stoull(timeStr);
      snap.trgRate[ch]    = rateStr.empty()  ? 0 : std::stoul(rateStr);
      snap.savedCount[ch] = countStr.empty() ? 0 : std::stoull(countStr);
    }
    snap.totalFileSize = digi[d]->GetTotalFilesSize();
    snap.acqOn = digi[d]->IsAcqOn();

  } else {
    if (!client) return snap;
    std::lock_guard<std::mutex> lock(client->scalarMutex);
    auto& sd = client->scalarData[d];
    memcpy(snap.trgRate,    sd.trgRate,    sizeof(snap.trgRate));
    memcpy(snap.savedCount, sd.savedCount, sizeof(snap.savedCount));
    memcpy(snap.acceptRate, sd.acceptRate, sizeof(snap.acceptRate));
    memcpy(snap.realTime,   sd.realTime,   sizeof(snap.realTime));
    snap.totalFileSize = sd.totalFileSize;
    snap.acqOn = sd.acqOn;
  }
  return snap;
}

//============================================ Direct access (migration helper)
Digitizer2Gen* DigiManager::GetDigitizer(int d) {
  if (d < 0 || d >= nDigi) return nullptr;
  return digi[d];
}

//============================================ Internal
void DigiManager::ReadDataLoop(int digiIndex) {
  if (onLogMessage) {
    onLogMessage("Digi-" + std::to_string(digi[digiIndex]->GetSerialNumber())
                 + " ReadDataLoop started.");
  }

  while (!readThreadStop[digiIndex]) {
    int ret;
    {
      std::lock_guard<std::mutex> lock(digiMutex[digiIndex]);
      ret = digi[digiIndex]->ReadData();
    }

    if (ret < 0) break;  // CAEN_FELib_Stop or error

    if (isSaveData[digiIndex] && ret > 0) {
      digi[digiIndex]->SaveDataToFile();
    }

    if (ret == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  if (onLogMessage) {
    onLogMessage("Digi-" + std::to_string(digi[digiIndex]->GetSerialNumber())
                 + " ReadDataLoop stopped.");
  }
}

void DigiManager::RefreshDigiInfo(int index) {
  if (mode != Mode::Broker || !client || !client->IsConnected()) return;
  auto info = client->GetDigiInfo(index);
  infoCache[index].serialNumber = info.serialNumber;
  infoCache[index].modelName    = info.modelName;
  infoCache[index].fpgaType     = info.fpgaType;
  infoCache[index].nChannels    = info.nChannels;
  infoCache[index].tick2ns      = info.tick2ns;
  infoCache[index].isConnected  = info.isConnected;
}
