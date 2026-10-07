#include "ClassDigitizer2Gen.h"

#include <cstring>
#include <algorithm>
#include <sys/stat.h>

Digitizer2Gen::Digitizer2Gen(){  
  //printf("======== %s \n",__func__);
  Initialization();
}

Digitizer2Gen::~Digitizer2Gen(){
  printf("========Digitizer2Gen::%s (%d)\n",__func__, serialNumber);
  if(isConnected ) CloseDigitizer();

  /// hit is allocated by SetDataFormat() and was never freed here, so every digitizer leaked its
  /// whole Hit on destruction: 97 KB of probe arrays under the DPP formats, and 20 MB under Raw
  /// because of the blob buffer. CloseDigitizers() deletes every board, so an open/close cycle on
  /// 20 boards in Raw leaked ~400 MB. ~Hit() releases everything it owns.
  /// Safe here: MainWindow::CloseDigitizers() joins the read thread before deleting the digitizer,
  /// and ReadDataThread::run() is the only other thing that touches hit.
  if( hit ){ delete hit; hit = NULL; }
}

void Digitizer2Gen::Initialization(){
  //printf("======== %s \n",__func__);

  handle = 0;
  ep_handle = 0;
  ep_folder_handle = 0;
  stat_handle = 0;
  isConnected = false;
  isDummy = false;

  serialNumber = 0;
  FPGAType = "";
  nChannels = 0;
  tick2ns = 0;
  CupVer = 0;

  outFileIndex = 0;
  FinishedOutFilesSize = 0;
  fileWriteError = false;
  droppedHitCount = 0;
  dataStartIdentifier = 0xAAA0;
  outFile = NULL;
  outFileSize = 0;

  hit = NULL;

  acqON = false;


  settingFileName = "";

  /// Nothing is known about the board yet, so claim the fewest sensors any model has. Raised by
  /// ResolveModelSensors() once ModelName has been read. A dummy never gets that far and keeps 1.
  nTempSensADC = 1;

  boardSettings = PHA::DIG::AllSettings;
  SetChSettingTable(PHA::CH::AllSettings);
  for( int index = 0 ; index < 4; index ++) {
    VGASetting[index] = PHA::VGA::VGAGain;
    LVDSSettings[index] = PHA::LVDS::AllSettings;
  }
  for( int idx = 0; idx < 16; idx ++){
    InputDelay[idx] = PHA::GROUP::InputDelay;
  }

  //build map
  for( int i = 0; i < (int) PHA::DIG::AllSettings.size(); i++) boardMap[PHA::DIG::AllSettings[i].GetPara()] = i;
  for( int i = 0; i < (int) PHA::LVDS::AllSettings.size(); i++) LVDSMap[PHA::LVDS::AllSettings[i].GetPara()] = i;
  for( int i = 0; i < (int) PHA::CH::AllSettings.size(); i++) chMap[PHA::CH::AllSettings[i].GetPara()] = i;


}

/// Point the channel parameters at a firmware's table and give every channel -- all 64, not just
/// the ones this board has -- a matching row of empty values. Sizing all 64 is what makes the
/// stale-slot case impossible: an index that is in range for the table is in range for every
/// channel, whatever the board reported.
void Digitizer2Gen::SetChSettingTable(const std::vector<Reg> & table){
  chSettingTable = table;
  for( int ch = 0; ch < MaxNumberOfChannel; ch ++){
    chValue[ch].assign(chSettingTable.size(), "");
  }
}

void Digitizer2Gen::SetDummy(unsigned int sn){

  isDummy = true;
  serialNumber = sn;
  nChannels = 64;
  FPGAType = "DPP_PHA";
}

//########################################### Handles functions
uint64_t Digitizer2Gen::GetHandle(const char * parameter){
  
  uint64_t par_handle;
  int ret = CAEN_FELib_GetHandle(handle, parameter, &par_handle);
  if(ret != CAEN_FELib_Success) {
    ErrorMsg(__func__, ret);
    return 0;
  }
  return par_handle;

}

uint64_t Digitizer2Gen::GetParentHandle(uint64_t handle){
  uint64_t par_handle;
  int ret = CAEN_FELib_GetParentHandle(handle, NULL, &par_handle);
  if(ret != CAEN_FELib_Success) {
    ErrorMsg(__func__, ret);
    return 0;
  }
  return par_handle;
}

std::string Digitizer2Gen::GetPath(uint64_t handle){
  char path[256];
  int ret = CAEN_FELib_GetPath(handle, path);
  if(ret != CAEN_FELib_Success) {
    ErrorMsg(__func__, ret);
    return "Error";
  }
  return path;
  
}

//########################################### Read Write

/// Look-up only: operator[] would INSERT on a miss, which mutates (and can rehash) the map from
/// whichever thread happens to ask. Callers must handle the -1.
static int LookUp(const std::unordered_map<std::string, int> & map, const std::string & key){
  auto it = map.find(key);
  return ( it == map.end() ) ? -1 : it->second;
}

int Digitizer2Gen::FindIndex(const Reg &para){
  switch (para.GetType() ){
    case TYPE::CH: return LookUp(chMap, para.GetPara());
    case TYPE::DIG: return LookUp(boardMap, para.GetPara());
    case TYPE::VGA: return 0;
    case TYPE::LVDS: return LookUp(LVDSMap, para.GetPara());
    case TYPE::GROUP : return 0;
  }
  return -1;
}

std::string Digitizer2Gen::ReadValue(const char * parameter, bool verbose){
  if( !isConnected ) return "not connected";
  //printf(" %s|%s \n", __func__, parameter);
  char retValue[256] = {0};
  int ret = CAEN_FELib_GetValue(handle, parameter, retValue);
  if (ret != CAEN_FELib_Success) {
    printf("  %s|%d|%-45s| read fail\n", __func__, serialNumber, parameter);
    return ErrorMsg(__func__, ret);
  }else{
    if( verbose ) printf("  %s|%d|%-45s:%s\n", __func__, serialNumber, parameter, retValue);
  }
  if( ModelName == "VX2730" && (strstr(parameter, "ChPreTriggerT") != nullptr
                             || strstr(parameter, "ChRecordLengthT") != nullptr
                             || strstr(parameter, "CFDDelayT") != nullptr
                             || strstr(parameter, "GateLongLengthT") != nullptr
                             || strstr(parameter, "GateShortLengthT") != nullptr
                             || strstr(parameter, "GateOffsetT") != nullptr )){
    return std::to_string(atoi(retValue) * 4);
  }
  return retValue;
}

std::string Digitizer2Gen::ReadValue(const Reg &para, int ch_index,  bool verbose){
  std:: string ans = ReadValue(para.GetFullPara(ch_index, nChannels).c_str(), verbose); 

  int index = FindIndex(para);
  if( index < 0 ) return ans; // unknown parameter, nothing to cache
  /// All four indexed cases are bounded, not just CH. ch_index defaults to -1, and the VGA, LVDS
  /// and GROUP arrays are 4, 4 and 16 long -- far shorter than the channel counts that reach here.
  switch( para.GetType()){
    case TYPE::CH  : if( ch_index >= 0 && ch_index < MaxNumberOfChannel && index < (int) chValue[ch_index].size() ) chValue[ch_index][index] = ans; break;
    case TYPE::DIG : if( index < (int) boardSettings.size() ) boardSettings[index].SetValue(ans); break;
    case TYPE::VGA : if( ch_index >= 0 && ch_index < 4 ) VGASetting[ch_index].SetValue(ans); break;
    case TYPE::LVDS: if( ch_index >= 0 && ch_index < 4 && index < (int) LVDSSettings[ch_index].size() ) LVDSSettings[ch_index][index].SetValue(ans);break;
    case TYPE::GROUP: if( ch_index >= 0 && ch_index < MaxNumberOfGroup ) InputDelay[ch_index].SetValue(ans); break;
  }
  
  //printf("%s | %s | index %d | %s \n", para.GetFullPara(ch_index).c_str(), ans.c_str(), index, chValue[ch_index][index].c_str());

  return ans;
}

bool Digitizer2Gen::WriteValue(const char * parameter, std::string value, bool verbose){
  if( !isConnected ) return false;
  //ReadValue(parameter, 1);
  if( verbose) printf(" %s|%d|%-45s|%s|\n", __func__, serialNumber, parameter, value.c_str());

  // Clamp numeric values to the parameter's valid range before writing
  // Look up the parameter in settings arrays by matching the base name
  const char * parName = strrchr(parameter, '/');
  if( parName ) parName++; else parName = parameter;

  /// The name->index maps exist for exactly this. Scanning the table with string compares was a
  /// second, divergent way of finding a parameter, and it ran on every write.
  auto clampToRange = [&](const std::unordered_map<std::string, int> & map,
                          const std::vector<Reg> & settings) {
    const int index = LookUp(map, parName);
    if( index < 0 || index >= (int) settings.size() ) return;
    const Reg & reg = settings[index];
    if( reg.GetAnswerType() != ANSTYPE::INTEGER ) return;
    const auto & answers = reg.GetAnswers();
    if( answers.size() < 3 ) return;
    long long minVal = atoll(answers[0].first.c_str());
    long long maxVal = atoll(answers[1].first.c_str());
    long long step   = atoll(answers[2].first.c_str());
    long long val    = atoll(value.c_str());
    if( val < minVal ) val = minVal;
    if( val > maxVal ) val = maxVal;
    if( step > 0 ) val = ((val - minVal + step/2) / step) * step + minVal;
    value = std::to_string(val);
  };

  if( strstr(parameter, "/ch/") != nullptr ){
    clampToRange(chMap, chSettingTable);
  } else {
    clampToRange(boardMap, boardSettings);
  }

  if( ModelName == "VX2730" && (strstr(parameter, "ChPreTriggerT") != nullptr
                             || strstr(parameter, "ChRecordLengthT") != nullptr
                             || strstr(parameter, "CFDDelayT") != nullptr
                             || strstr(parameter, "GateLongLengthT") != nullptr
                             || strstr(parameter, "GateShortLengthT") != nullptr
                             || strstr(parameter, "GateOffsetT") != nullptr )){
    value = std::to_string(atoi(value.c_str()) / 4);
  }
  int ret = CAEN_FELib_SetValue(handle, parameter, value.c_str());
  if (ret != CAEN_FELib_Success) {
    printf("WriteError|%s||%s|\n", parameter, value.c_str());
    ErrorMsg(__func__, ret);
    return false;
  }
  return true;
}

bool Digitizer2Gen::WriteValue(const Reg &para, std::string value, int ch_index){
  if( WriteValue(para.GetFullPara(ch_index, nChannels).c_str(), value) || isDummy){
    int index = FindIndex(para);
    if( index != -1 ){    
      switch(para.GetType()){
        case TYPE::CH :{
          if( ch_index >= 0 ){
            if( ch_index < MaxNumberOfChannel && index < (int) chValue[ch_index].size() )
              chValue[ch_index][index] = value;
          }else{
            for( int ch = 0; ch < nChannels && ch < MaxNumberOfChannel; ch++ ){
              if( index < (int) chValue[ch].size() ) chValue[ch][index] = value;
            }
          }

          //if( ch_index < 0 ) ch_index = 0;
          //printf("%s %s %s |%s|\n", __func__, para.GetPara().c_str(),
          //                     chSettingTable[index].GetFullPara(ch_index).c_str(),
          //                     chValue[ch_index][index].c_str());
        }break;
        
        case TYPE::VGA : {
          /// Bounded like the CH case above: VGASetting is 4 long and ch_index defaults to -1.
          if( ch_index < 0 || ch_index >= 4 ) break;
          VGASetting[ch_index].SetValue(value);

          //printf("%s %s %s |%s|\n", __func__,  para.GetPara().c_str(),
          //                      VGASetting[ch_index].GetFullPara(ch_index).c_str(), 
          //                      VGASetting[ch_index].GetValue().c_str());
        }break;

        case TYPE::DIG : {
          if( index >= (int) boardSettings.size() ) break;
          boardSettings[index].SetValue(value);
          //printf("%s %s %s |%s|\n", __func__,  para.GetPara().c_str(),
          //                     boardSettings[index].GetFullPara(ch_index).c_str(), 
          //                     boardSettings[index].GetValue().c_str()); 
        }break;
        
        case TYPE::LVDS : {
          if( ch_index < 0 || ch_index >= 4 || index >= (int) LVDSSettings[ch_index].size() ) break;
          LVDSSettings[ch_index][index].SetValue(value);
        }break;

        case TYPE::GROUP : {
          /// InputDelay is MaxNumberOfGroup (16) long, but the panel can hand this a channel
          /// number when a GROUP parameter is edited with the channel picker active.
          if( ch_index < 0 || ch_index >= MaxNumberOfGroup ) break;
          InputDelay[ch_index].SetValue(value);
          // printf("%s %s %s |%s|\n", __func__,  para.GetPara().c_str(),
          //                     InputDelay[ch_index].GetFullPara(ch_index).c_str(), 
          //                     InputDelay[ch_index].GetValue().c_str()); 
        }break;

      }
    }
    return true;
  }else{
    return false;
  }
}

void Digitizer2Gen::SendCommand(const char * parameter){
  if( !isConnected ) return;
  printf(" %s|%d|Send Command : %s \n", __func__, serialNumber, parameter);
  int ret = CAEN_FELib_SendCommand(handle, parameter);
  if (ret != CAEN_FELib_Success) {
    ErrorMsg(__func__, ret);
    return;
  }
}

void Digitizer2Gen::SendCommand(std::string shortPara){
  std::string haha = "/cmd/" + shortPara;
  SendCommand(haha.c_str());
}

//########################################### Open digitizer
int Digitizer2Gen::OpenDigitizer(const char * url){
  
  //printf("======== %s \n",__func__);

  int ret = CAEN_FELib_Open(url, &handle);

  //printf("===  ret : %d | %d \n", ret, CAEN_FELib_Success);

  if (ret != CAEN_FELib_Success) {
    ErrorMsg(__func__, ret);
    return -1;
  }
  
  isConnected = true;

  printf("#################################################\n");

  //========== PHA and PSD are the same
  serialNumber = atoi(ReadValue(PHA::DIG::SerialNumber).c_str());
  FPGAType = ReadValue(PHA::DIG::FirmwareType);
  FPGAVer = atoi(ReadValue(PHA::DIG::FPGA_firmwareVersion).c_str());
  nChannels = atoi(ReadValue(PHA::DIG::NumberOfChannel).c_str());
  ModelName = ReadValue(PHA::DIG::ModelName);
  ResolveModelSensors();
  CupVer = atoi(ReadValue(PHA::DIG::CupVer).c_str());
  int adcRate = atoi(ReadValue(PHA::DIG::ADC_SampleRate).c_str());
  tick2ns = (adcRate > 0) ? 1000/adcRate : 1;
  
  printf("   IP address : %s\n", ReadValue(PHA::DIG::IPAddress).c_str());
  printf("     Net Mask : %s\n", ReadValue(PHA::DIG::NetMask).c_str());
  printf("      Gateway : %s\n", ReadValue(PHA::DIG::Gateway).c_str());
  printf("   Model name : %s\n", ModelName.c_str());
  printf("     DPP Type : %s (%d)\n", FPGAType.c_str(), FPGAVer);
  printf("Serial number : %d\n", serialNumber);
  printf("     ADC bits : %s\n", ReadValue(PHA::DIG::ADC_bit).c_str());
  printf("     ADC rate : %d Msps, tick2ns : %d ns\n", adcRate, tick2ns);
  printf("     Channels : %d\n", nChannels);

  if( FPGAType == DPPType::PHA) {

    printf("========== defining setting arrays for %s \n", FPGAType.c_str());

    boardSettings = PHA::DIG::AllSettings;
    SetChSettingTable(PHA::CH::AllSettings);
    for( int index = 0 ; index < 4; index ++) {
      VGASetting[index] = PHA::VGA::VGAGain;
      LVDSSettings[index] = PHA::LVDS::AllSettings;
    }
    for( int idx = 0; idx < 16; idx ++ ){
      InputDelay[idx] = PHA::GROUP::InputDelay;
    }

    //build map
    for( int i = 0; i < (int) PHA::DIG::AllSettings.size(); i++) boardMap[PHA::DIG::AllSettings[i].GetPara()] = i;
    for( int i = 0; i < (int) PHA::LVDS::AllSettings.size(); i++) LVDSMap[PHA::LVDS::AllSettings[i].GetPara()] = i;
    for( int i = 0; i < (int) PHA::CH::AllSettings.size(); i++) chMap[PHA::CH::AllSettings[i].GetPara()] = i;

  }else if (FPGAType == DPPType::PSD){

    printf("========== defining setting arrays for %s \n", FPGAType.c_str());

    boardSettings = PSD::DIG::AllSettings;
    SetChSettingTable(PSD::CH::AllSettings);
    for( int index = 0 ; index < 4; index ++) {
      VGASetting[index] = PSD::VGA::VGAGain;
      LVDSSettings[index] = PSD::LVDS::AllSettings;
    }
    for( int idx = 0; idx < 16; idx ++ ){
      InputDelay[idx] = PSD::GROUP::InputDelay;
    }

    //build map
    for( int i = 0; i < (int) PSD::DIG::AllSettings.size(); i++) boardMap[PSD::DIG::AllSettings[i].GetPara()] = i;
    for( int i = 0; i < (int) PSD::LVDS::AllSettings.size(); i++) LVDSMap[PSD::LVDS::AllSettings[i].GetPara()] = i;
    for( int i = 0; i < (int) PSD::CH::AllSettings.size(); i++) chMap[PSD::CH::AllSettings[i].GetPara()] = i;

  }else{
    printf(" DPP Type %s is not supported.\n", FPGAType.c_str());
    return -303;
  }

  AdjustParameterRanges();
  ReadAllSettings();
  //------ set default setting file name
  settingFileName = "settings_"+ std::to_string(serialNumber) + ".dat";

  printf("====================== \n");

  return 0;
}

int Digitizer2Gen::CloseDigitizer(){
  printf("========Digitizer2Gen::%s \n",__func__);
  if( isConnected == true ){
    int ret = CAEN_FELib_Close(handle);
    if (ret != CAEN_FELib_Success) {
      ErrorMsg(__func__, ret);
      return 0;
    }
    isConnected = false;
  }
  return 0;
}

//########################################### Adjust parameter ranges for model/firmware
void Digitizer2Gen::AdjustParameterRanges(){

  printf("Digitizer2Gen::%s | Model=%s, FPGA=%s\n", __func__, ModelName.c_str(), FPGAType.c_str());

  // Helper to update a channel parameter's answers (min/max/step for INTEGER, or value list for COMBOX)
  /// One table, so one write -- this used to loop over nChannels setting the same answer list 64
  /// times, and it skipped the channels above nChannels, which is where the stale-range half of
  /// the old per-channel-table bug came from.
  auto updateChParam = [&](const std::string & paraName, std::vector<std::pair<std::string,std::string>> newAnswers){
    const int idx = LookUp(chMap, paraName);
    if( idx < 0 || idx >= (int) chSettingTable.size() ) return;
    chSettingTable[idx].SetAnswers(newAnswers);
  };

  //========== VX2730 adjustments
  // VX2730 has 500 Msps (2ns tick), but time parameters use an internal 4x scaling:
  //   ReadValue multiplies firmware value by 4 to return ns
  //   WriteValue divides user ns by 4 before sending to firmware
  // So user-facing range = firmware internal range × 4
  if( ModelName == "VX2730" ){
    // firmware internal: min=32, step=8 → user: min=128, step=32
    updateChParam("ChRecordLengthT", {{"128",""},{"16200",""},{"32",""}});
    updateChParam("ChPreTriggerT",   {{"128",""},{"8000",""},{"32",""}});

    // PSD-specific time params
    if( FPGAType == DPPType::PSD ){
      // firmware internal: min=2, step=2 → user: min=8, step=8
      updateChParam("CFDDelayT",          {{"8",""},{"2040",""},{"8",""}});
      updateChParam("GateLongLengthT",    {{"8",""},{"8000",""},{"8",""}});
      updateChParam("GateShortLengthT",   {{"8",""},{"8000",""},{"8",""}});
      // firmware internal: min=16, step=2 → user: min=64, step=8
      updateChParam("GateOffsetT",        {{"64",""},{"2000",""},{"8",""}});
      updateChParam("AbsoluteBaseline",   {{"0",""},{"16383",""},{"1",""}});
      updateChParam("SignalOffset",       {{"-1000000",""},{"1000000",""},{"50",""}});
    }

    // WaveDataSource: VX2730 doesn't support ADC_TEST_* values
    updateChParam("WaveDataSource", {{"ADC_DATA","Input ADC"},
                                     {"Ramp","Ramp generator"},
                                     {"IPE","Internal Pulse Emulator"},
                                     {"SquareWave","Test Pulse (Square Wave)"}});

    // AntiCoincidenceMask: VX2730 firmware has issues with ITLA/ITLB
    updateChParam("AntiCoincidenceMask", {{"Disabled","Disabled"},
                                          {"Ch64Trigger","Channel 64-Trigger"},
                                          {"TrgIn","TRG-IN"},
                                          {"GlobalTriggerSource","Global Trigger Source"}});
  }

  //========== PSD firmware adjustments (both VX2730 and VX2745)
  if( FPGAType == DPPType::PSD ){
    // ChPreTriggerT: PSD max is 8000 (PHA max is 32000)
    if( ModelName != "VX2730" ){ // VX2730 already set above
      updateChParam("ChPreTriggerT", {{"32",""},{"8000",""},{"8",""}});
    }
  }
}

//########################################### DAQ
void Digitizer2Gen::StartACQ(){
  
  SendCommand("/cmd/armacquisition"); // this will also clear data
  SendCommand("/cmd/swstartacquisition");

  /// Kept, but OpenOutFile() (which MainWindow calls BEFORE this) is the authoritative reset for a
  /// saving run: it must open index 0 even when the previous run ended mid-rollover. These lines
  /// still matter for no-save runs, which never open a file and would otherwise show the previous
  /// run's total in the file-size label.
  outFileIndex = 0;
  outFileSize = 0;
  FinishedOutFilesSize = 0;

  acqON = true;
}

void Digitizer2Gen::StopACQ(){
  
  SendCommand("/cmd/SwStopAcquisition");
  SendCommand("/cmd/disarmacquisition");
  
  acqON = false;
}

void Digitizer2Gen::SetDataFormat(unsigned short dataFormat){
  
  printf("%s : %d for digi-%d %s\n", __func__, dataFormat, serialNumber, FPGAType.c_str() );

  /// initialised, because the dataFormat if-ladder below is not exhaustive
  int ret = CAEN_FELib_Success;

  ///========== get endpoint and endpoint folder handle
  if( dataFormat == DataFormat::Raw ){

    ret  = CAEN_FELib_GetHandle(handle, "/endpoint/raw", &ep_handle);
    ret |= CAEN_FELib_GetParentHandle(ep_handle, NULL, &ep_folder_handle);
    ret |= CAEN_FELib_SetValue(ep_folder_handle, "/par/activeendpoint", "raw");

    if (ret != CAEN_FELib_Success) {
      ErrorMsg("Set active endpoint", ret);
      return;
    }

  }else{

    if( FPGAType == DPPType::PHA ){
      ret  = CAEN_FELib_GetHandle(handle, "/endpoint/dpppha", &ep_handle);
      ret |= CAEN_FELib_GetParentHandle(ep_handle, NULL, &ep_folder_handle);
      ret |= CAEN_FELib_SetValue(ep_folder_handle, "/par/activeendpoint", "dpppha");
    }else if(FPGAType == DPPType::PSD) {
      ret  = CAEN_FELib_GetHandle(handle, "/endpoint/dpppsd", &ep_handle);
      ret |= CAEN_FELib_GetParentHandle(ep_handle, NULL, &ep_folder_handle);
      ret |= CAEN_FELib_SetValue(ep_folder_handle, "/par/activeendpoint", "dpppsd");
    }else{
      ErrorMsg("DPP-Type not supported.", CAEN_FELib_GenericError);
      return;
    }

    if (ret != CAEN_FELib_Success) {
      ErrorMsg("Set active endpoint", ret);
      return;
    }
  }



  if( hit ) delete hit;
  hit = new Hit();
  hit->SetDataType(dataFormat, FPGAType);
  dataStartIdentifier = 0xAA00 + dataFormat;
  if(FPGAType == DPPType::PSD ) dataStartIdentifier += 0x0010;

  //^===================================================== PSD
  if( FPGAType == DPPType::PHA) {
    if( dataFormat == DataFormat::ALL ){  
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",              \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",            \"type\" : \"U64\" }, \
        { \"name\" : \"FINE_TIMESTAMP\",       \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY\",               \"type\" : \"U16\" }, \
        { \"name\" : \"ANALOG_PROBE_1\",       \"type\" : \"I32\", \"dim\" : 1 }, \
        { \"name\" : \"ANALOG_PROBE_2\",       \"type\" : \"I32\", \"dim\" : 1 }, \
        { \"name\" : \"DIGITAL_PROBE_1\",      \"type\" : \"U8\",  \"dim\" : 1 }, \
        { \"name\" : \"DIGITAL_PROBE_2\",      \"type\" : \"U8\",  \"dim\" : 1 }, \
        { \"name\" : \"DIGITAL_PROBE_3\",      \"type\" : \"U8\",  \"dim\" : 1 }, \
        { \"name\" : \"DIGITAL_PROBE_4\",      \"type\" : \"U8\",  \"dim\" : 1 }, \
        { \"name\" : \"ANALOG_PROBE_1_TYPE\",  \"type\" : \"U8\" }, \
        { \"name\" : \"ANALOG_PROBE_2_TYPE\",  \"type\" : \"U8\" }, \
        { \"name\" : \"DIGITAL_PROBE_1_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"DIGITAL_PROBE_2_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"DIGITAL_PROBE_3_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"DIGITAL_PROBE_4_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"WAVEFORM_SIZE\",        \"type\" : \"SIZE_T\" }, \
        { \"name\" : \"FLAGS_LOW_PRIORITY\",   \"type\" : \"U16\"}, \
        { \"name\" : \"FLAGS_HIGH_PRIORITY\",  \"type\" : \"U16\" }, \
        { \"name\" : \"TRIGGER_THR\",          \"type\" : \"U16\" }, \
        { \"name\" : \"TIME_RESOLUTION\",      \"type\" : \"U8\" }, \
        { \"name\" : \"BOARD_FAIL\",           \"type\" : \"BOOL\" }, \
        { \"name\" : \"FLUSH\",                \"type\" : \"BOOL\" }, \
        { \"name\" : \"AGGREGATE_COUNTER\",    \"type\" : \"U32\" }, \
        { \"name\" : \"EVENT_SIZE\",           \"type\" : \"SIZE_T\" } \
      ]");
    }

    if( dataFormat == DataFormat::OneTrace ){
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",             \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",           \"type\" : \"U64\" }, \
        { \"name\" : \"FINE_TIMESTAMP\",      \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY\",              \"type\" : \"U16\" }, \
        { \"name\" : \"ANALOG_PROBE_1\",      \"type\" : \"I32\", \"dim\" : 1 }, \
        { \"name\" : \"ANALOG_PROBE_1_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"WAVEFORM_SIZE\",       \"type\" : \"SIZE_T\" }, \
        { \"name\" : \"FLAGS_LOW_PRIORITY\",  \"type\" : \"U16\"}, \
        { \"name\" : \"FLAGS_HIGH_PRIORITY\", \"type\" : \"U16\" }, \
        { \"name\" : \"TRIGGER_THR\",         \"type\" : \"U16\" }, \
        { \"name\" : \"TIME_RESOLUTION\",     \"type\" : \"U8\" }, \
        { \"name\" : \"BOARD_FAIL\",          \"type\" : \"BOOL\" }, \
        { \"name\" : \"FLUSH\",               \"type\" : \"BOOL\" }, \
        { \"name\" : \"AGGREGATE_COUNTER\",   \"type\" : \"U32\" }, \
        { \"name\" : \"EVENT_SIZE\",          \"type\" : \"SIZE_T\" } \
      ]");
    }

    if( dataFormat == DataFormat::NoTrace ){
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",             \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",           \"type\" : \"U64\" }, \
        { \"name\" : \"FINE_TIMESTAMP\",      \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY\",              \"type\" : \"U16\" }, \
        { \"name\" : \"FLAGS_LOW_PRIORITY\",  \"type\" : \"U16\"}, \
        { \"name\" : \"FLAGS_HIGH_PRIORITY\", \"type\" : \"U16\" }, \
        { \"name\" : \"TRIGGER_THR\",         \"type\" : \"U16\" }, \
        { \"name\" : \"TIME_RESOLUTION\",     \"type\" : \"U8\" }, \
        { \"name\" : \"BOARD_FAIL\",          \"type\" : \"BOOL\" }, \
        { \"name\" : \"FLUSH\",               \"type\" : \"BOOL\" }, \
        { \"name\" : \"AGGREGATE_COUNTER\",   \"type\" : \"U32\" }, \
        { \"name\" : \"EVENT_SIZE\",          \"type\" : \"SIZE_T\" } \
      ]");
    }

    if( dataFormat == DataFormat::Minimum ){
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",   \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\", \"type\" : \"U64\" }, \
        { \"name\" : \"ENERGY\",    \"type\" : \"U16\" } \
      ]");
    }

    if( dataFormat == DataFormat::MiniWithFineTime ){
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",           \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",         \"type\" : \"U64\" }, \
        { \"name\" : \"FINE_TIMESTAMP\",    \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY\",            \"type\" : \"U16\" } \
      ]");
    }

  //^===================================================== PSD
  }else if ( FPGAType == DPPType::PSD ){

    if( dataFormat == DataFormat::ALL ){  
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",              \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",            \"type\" : \"U64\" }, \
        { \"name\" : \"FINE_TIMESTAMP\",       \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY\",               \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY_SHORT\",         \"type\" : \"U16\" }, \
        { \"name\" : \"ANALOG_PROBE_1\",       \"type\" : \"I32\", \"dim\" : 1 }, \
        { \"name\" : \"ANALOG_PROBE_2\",       \"type\" : \"I32\", \"dim\" : 1 }, \
        { \"name\" : \"DIGITAL_PROBE_1\",      \"type\" : \"U8\",  \"dim\" : 1 }, \
        { \"name\" : \"DIGITAL_PROBE_2\",      \"type\" : \"U8\",  \"dim\" : 1 }, \
        { \"name\" : \"DIGITAL_PROBE_3\",      \"type\" : \"U8\",  \"dim\" : 1 }, \
        { \"name\" : \"DIGITAL_PROBE_4\",      \"type\" : \"U8\",  \"dim\" : 1 }, \
        { \"name\" : \"ANALOG_PROBE_1_TYPE\",  \"type\" : \"U8\" }, \
        { \"name\" : \"ANALOG_PROBE_2_TYPE\",  \"type\" : \"U8\" }, \
        { \"name\" : \"DIGITAL_PROBE_1_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"DIGITAL_PROBE_2_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"DIGITAL_PROBE_3_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"DIGITAL_PROBE_4_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"WAVEFORM_SIZE\",        \"type\" : \"SIZE_T\" }, \
        { \"name\" : \"FLAGS_LOW_PRIORITY\",   \"type\" : \"U16\"}, \
        { \"name\" : \"FLAGS_HIGH_PRIORITY\",  \"type\" : \"U16\" }, \
        { \"name\" : \"TRIGGER_THR\",          \"type\" : \"U16\" }, \
        { \"name\" : \"TIME_RESOLUTION\",      \"type\" : \"U8\" }, \
        { \"name\" : \"BOARD_FAIL\",           \"type\" : \"BOOL\" }, \
        { \"name\" : \"FLUSH\",                \"type\" : \"BOOL\" }, \
        { \"name\" : \"AGGREGATE_COUNTER\",    \"type\" : \"U32\" }, \
        { \"name\" : \"EVENT_SIZE\",           \"type\" : \"SIZE_T\" } \
      ]");
    }

    if( dataFormat == DataFormat::OneTrace ){
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",             \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",           \"type\" : \"U64\" }, \
        { \"name\" : \"FINE_TIMESTAMP\",      \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY\",              \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY_SHORT\",        \"type\" : \"U16\" }, \
        { \"name\" : \"ANALOG_PROBE_1\",      \"type\" : \"I32\", \"dim\" : 1 }, \
        { \"name\" : \"ANALOG_PROBE_1_TYPE\", \"type\" : \"U8\" }, \
        { \"name\" : \"WAVEFORM_SIZE\",       \"type\" : \"SIZE_T\" }, \
        { \"name\" : \"FLAGS_LOW_PRIORITY\",  \"type\" : \"U16\"}, \
        { \"name\" : \"FLAGS_HIGH_PRIORITY\", \"type\" : \"U16\" }, \
        { \"name\" : \"TRIGGER_THR\",         \"type\" : \"U16\" }, \
        { \"name\" : \"TIME_RESOLUTION\",     \"type\" : \"U8\" }, \
        { \"name\" : \"BOARD_FAIL\",          \"type\" : \"BOOL\" }, \
        { \"name\" : \"FLUSH\",               \"type\" : \"BOOL\" }, \
        { \"name\" : \"AGGREGATE_COUNTER\",   \"type\" : \"U32\" }, \
        { \"name\" : \"EVENT_SIZE\",          \"type\" : \"SIZE_T\" } \
      ]");
    }

    if( dataFormat == DataFormat::NoTrace ){
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",             \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",           \"type\" : \"U64\" }, \
        { \"name\" : \"FINE_TIMESTAMP\",      \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY\",              \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY_SHORT\",        \"type\" : \"U16\" }, \
        { \"name\" : \"FLAGS_LOW_PRIORITY\",  \"type\" : \"U16\"}, \
        { \"name\" : \"FLAGS_HIGH_PRIORITY\", \"type\" : \"U16\" }, \
        { \"name\" : \"TRIGGER_THR\",         \"type\" : \"U16\" }, \
        { \"name\" : \"TIME_RESOLUTION\",     \"type\" : \"U8\" }, \
        { \"name\" : \"BOARD_FAIL\",          \"type\" : \"BOOL\" }, \
        { \"name\" : \"FLUSH\",               \"type\" : \"BOOL\" }, \
        { \"name\" : \"AGGREGATE_COUNTER\",   \"type\" : \"U32\" }, \
        { \"name\" : \"EVENT_SIZE\",          \"type\" : \"SIZE_T\" } \
      ]");
    }

    if( dataFormat == DataFormat::MiniWithFineTime ){
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",             \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",           \"type\" : \"U64\" }, \
        { \"name\" : \"FINE_TIMESTAMP\",      \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY\",              \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY_SHORT\",        \"type\" : \"U16\" } \
      ]");
    }

    if( dataFormat == DataFormat::Minimum ){
      ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      "[ \
        { \"name\" : \"CHANNEL\",             \"type\" : \"U8\" }, \
        { \"name\" : \"TIMESTAMP\",           \"type\" : \"U64\" }, \
        { \"name\" : \"ENERGY\",              \"type\" : \"U16\" }, \
        { \"name\" : \"ENERGY_SHORT\",        \"type\" : \"U16\" } \
      ]");
    }

  }

  if( dataFormat == DataFormat::Raw ){
    ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      " [ \
         { \"name\": \"DATA\",     \"type\": \"U8\", \"dim\": 1 }, \
         { \"name\": \"SIZE\",     \"type\": \"SIZE_T\" }, \
         { \"name\": \"N_EVENTS\", \"type\": \"U32\" } \
      ]"
    );
  }

  if (ret != CAEN_FELib_Success) {
    ErrorMsg("Set Read Data Format", ret);
    return;
  }

  //TODO Statistic handle and endpoint
  if( FPGAType == DPPType::PHA ) ret  = CAEN_FELib_GetHandle(handle, "/endpoint/dpppha/stats", &stat_handle);
  if( FPGAType == DPPType::PSD ) ret  = CAEN_FELib_GetHandle(handle, "/endpoint/dpppsd/stats", &stat_handle);
  ret |= CAEN_FELib_SetReadDataFormat(stat_handle, 
    " [ \
        { \"name\": \"REAL_TIME_NS\",    \"type\": \"U64\", \"dim\": 1 }, \
        { \"name\": \"DEAD_TIME_NS\",    \"type\": \"U64\", \"dim\": 1 }, \
        { \"name\": \"LIVE_TIME_NS\",    \"type\": \"U64\", \"dim\": 1 }, \
        { \"name\": \"TRIGGER_CNT\",     \"type\": \"U32\", \"dim\": 1 }, \
        { \"name\": \"SAVED_EVENT_CNT\", \"type\": \"U32\", \"dim\": 1 } \
    ]"
  );

  if (ret != CAEN_FELib_Success) {
    ErrorMsg("Set Statistics", ret);
    return;
  }

}

int Digitizer2Gen::ReadStat(){

  /// SelfTrgRate is the one field the statistics endpoint does not carry, so it is read per
  /// channel either way. It sits above the Raw early-out because the scalar panel has always read
  /// it live on the Raw path too, and it lands in chValue, not in the five arrays below.
  for( int ch = 0; ch < nChannels && ch < MaxNumberOfChannel; ch++) ReadValue( PHA::CH::SelfTrgRate, ch);

  /// Under Raw, ReadData() on the DAQ thread already fills realTime/deadTime/liveTime/
  /// triggerCount/savedEventCount from the decoder's time-counter events. Reading the stats
  /// endpoint here, from the GUI timer thread, would write the same five arrays concurrently --
  /// and the decoder's numbers are the authoritative ones on that path anyway.
  if( hit && hit->dataType == DataFormat::Raw ) return CAEN_FELib_Success;

  /// stat_handle is only created by SetDataFormat() at start-run (scope start too). The scalar
  /// timer runs whenever the panel is visible, so before the first run -- or after the digitizers
  /// are closed and the CAEN_FELib handles are gone -- it would ReadData on an uninitialized or
  /// stale handle and spam "-9: not a valid handle ... closed connection" every tick.
  if( !isConnected || stat_handle == 0 ) return CAEN_FELib_Success;

  int ret = CAEN_FELib_ReadData(stat_handle, 100,
        realTime,
        deadTime,
        liveTime,
        triggerCount,
        savedEventCount
      );

  if (ret != CAEN_FELib_Success) ErrorMsg("Read Statistics", ret);

  return ret; // the status of the stat read; the ReadValue calls above no longer clobber it
}

void Digitizer2Gen::PrintStat(){
  printf("ch | Real Time[ns] | Dead Time[ns] | Live Time[ns] | Trigger |  Saved  | Rate[Hz] | Self Trig Rate [Hz] \n");
  for( int i = 0; i < nChannels && i < MaxNumberOfChannel; i++){
    //if( triggerCount[i] == 0 ) continue;
    if( chValue[i].empty() ) continue;
    const int chEnable = atoi(chValue[i][0].c_str());
    if( chEnable == 0 ) continue;
    printf("%02d | %13lu | %13lu | %13lu | %7u | %7u | %8.3f | %d\n",
         i, realTime[i], deadTime[i], liveTime[i], triggerCount[i], savedEventCount[i], triggerCount[i]*1e9*1.0/realTime[i], chEnable);
  }
}

int Digitizer2Gen::ReadData(){
  //printf("Digitizer2Gen::%s, DPP : %s, dataFormat : %d \n", __func__, FPGAType.c_str(), hit->dataType);

  /// Above the FPGAType guard, because that guard does NOT stop a dummy: SetDummy() sets
  /// FPGAType to DPP_PHA, so a dummy falls straight through to hit->dataType below — and hit is
  /// null for a dummy, since only SetDataFormat() allocates it and that returns early when the
  /// CAEN handle is 0. Stop rather than Timeout: ReadDataThread::run() has no sleep of its own,
  /// so every other code loops straight back in and spins a core.
  if( isDummy || hit == NULL ) return CAEN_FELib_Stop;

  if( FPGAType != DPPType::PHA && FPGAType != DPPType::PSD ) return -404;

  /// local, not a member: the GUI thread calls ReadValue() on this same object while we are here,
  /// and a shared member would let it overwrite our status between the read and the check below.
  int ret = CAEN_FELib_GenericError;

  if( hit->dataType == DataFormat::ALL ){
    if( FPGAType == DPPType::PHA ){
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->fine_timestamp,
        &hit->energy,
        hit->analog_probes[0],
        hit->analog_probes[1],
        hit->digital_probes[0],
        hit->digital_probes[1],
        hit->digital_probes[2],
        hit->digital_probes[3],
        &hit->analog_probes_type[0],
        &hit->analog_probes_type[1],
        &hit->digital_probes_type[0],
        &hit->digital_probes_type[1],
        &hit->digital_probes_type[2],
        &hit->digital_probes_type[3],
        &hit->traceLenght,
        &hit->flags_low_priority,
        &hit->flags_high_priority,
        &hit->trigger_threashold,
        &hit->downSampling,
        &hit->board_fail,
        &hit->flush,
        &hit->aggCounter,
        &hit->event_size
      );

      //printf("ch:%02d, trace Length %ld \n", hit->channel, hit->traceLenght);
    }else{
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->fine_timestamp,
        &hit->energy,
        &hit->energy_short,
        hit->analog_probes[0],
        hit->analog_probes[1],
        hit->digital_probes[0],
        hit->digital_probes[1],
        hit->digital_probes[2],
        hit->digital_probes[3],
        &hit->analog_probes_type[0],
        &hit->analog_probes_type[1],
        &hit->digital_probes_type[0],
        &hit->digital_probes_type[1],
        &hit->digital_probes_type[2],
        &hit->digital_probes_type[3],
        &hit->traceLenght,
        &hit->flags_low_priority,
        &hit->flags_high_priority,
        &hit->trigger_threashold,
        &hit->downSampling,
        &hit->board_fail,
        &hit->flush,
        &hit->aggCounter,
        &hit->event_size
      );

      //printf("ch:%02d, energy: %d, trace Length %ld \n", hit->channel, hit->energy, hit->traceLenght);

    }

    hit->isTraceAllZero = false;

  }else if( hit->dataType == DataFormat::OneTrace){
    if( FPGAType == DPPType::PHA ){
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->fine_timestamp,
        &hit->energy,
        hit->analog_probes[0],
        &hit->analog_probes_type[0],
        &hit->traceLenght,
        &hit->flags_low_priority,
        &hit->flags_high_priority,
        &hit->trigger_threashold,
        &hit->downSampling,
        &hit->board_fail,
        &hit->flush,
        &hit->aggCounter,
        &hit->event_size
      );
    }else{
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->fine_timestamp,
        &hit->energy,
        &hit->energy_short,
        hit->analog_probes[0],
        &hit->analog_probes_type[0],
        &hit->traceLenght,
        &hit->flags_low_priority,
        &hit->flags_high_priority,
        &hit->trigger_threashold,
        &hit->downSampling,
        &hit->board_fail,
        &hit->flush,
        &hit->aggCounter,
        &hit->event_size
      );
    }

    hit->isTraceAllZero = false;

  }else if( hit->dataType == DataFormat::NoTrace){
    if( FPGAType == DPPType::PHA ){
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->fine_timestamp,
        &hit->energy,
        &hit->flags_low_priority,
        &hit->flags_high_priority,
        &hit->trigger_threashold,
        &hit->downSampling,
        &hit->board_fail,
        &hit->flush,
        &hit->aggCounter,
        &hit->event_size
      );
    }else{
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->fine_timestamp,
        &hit->energy,
        &hit->energy_short,
        &hit->flags_low_priority,
        &hit->flags_high_priority,
        &hit->trigger_threashold,
        &hit->downSampling,
        &hit->board_fail,
        &hit->flush,
        &hit->aggCounter,
        &hit->event_size
      );
    }

    hit->isTraceAllZero = true;

  }else if( hit->dataType == DataFormat::MiniWithFineTime){
    if( FPGAType == DPPType::PHA ){
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->fine_timestamp,
        &hit->energy
      );
    }else{
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->fine_timestamp,
        &hit->energy,
        &hit->energy_short
      );
    }

    hit->isTraceAllZero = true;

  }else if( hit->dataType == DataFormat::Minimum){
    if( FPGAType == DPPType::PHA ){
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->energy
      );
    }else{
      ret = CAEN_FELib_ReadData(ep_handle, 100,
        &hit->channel,
        &hit->timestamp,
        &hit->energy,
        &hit->energy_short
      );
    }

    hit->isTraceAllZero = true;

  }else if( hit->dataType == DataFormat::Raw){

    ret = CAEN_FELib_ReadData(ep_handle, 100, hit->data, &hit->dataSize, &hit->n_events);

    if( ret == CAEN_FELib_Success && hit->dataSize > 0 ){
      rawDecoder.LoadBlob(hit->data, hit->dataSize, FPGAType);

      // Decode all hits and push to ring buffers (waveform samples are skipped)
      RawDecoder::DecodedHit decoded;
      while( rawDecoder.Next(decoded) ){
        if( decoded.channel < nChannels ){
          ringBuffer[decoded.channel].push({decoded.energy, decoded.energy_short});
          /// Both timestamps are raw here (RawDecoder.h:16-17), so apply the same scaling the DPP
          /// path below applies in place: timestamp -> ns, fine_timestamp -> units of 1/1024 ns.
          /// Only filled when something is actually analysing; see fillHitRing.
          if( fillHitRing.load(std::memory_order_relaxed) ){
            hitRing.push({ decoded.timestamp * tick2ns,
                           decoded.energy, decoded.energy_short,
                           (uint16_t)(decoded.fine_timestamp * tick2ns),
                           decoded.channel,
                           (uint8_t)(decoded.flags_high_priority & 0xFF) });
          }
        }
      }

      // Apply stat updates from time/counter events
      for( const auto& su : rawDecoder.GetStatUpdates() ){
        if( su.channel < nChannels ){
          realTime[su.channel]       = su.realTime;
          deadTime[su.channel]       = su.deadTime;
          liveTime[su.channel]       = (su.realTime > su.deadTime) ? (su.realTime - su.deadTime) : 0;
          triggerCount[su.channel]   = su.triggerCount;
          savedEventCount[su.channel] = su.savedEventCount;
        }
      }
    }

    /// Suppresses the trace-ring fill below, which is the intent: under Raw the waveform stays
    /// inside the undecoded blob and is never unpacked here, so there is nothing to push. The
    /// scope therefore cannot run on Raw -- StartScope() forces DataFormat::ALL for exactly this
    /// reason.
    hit->isTraceAllZero = true;

  }else{
    return CAEN_FELib_UNKNOWN;
  }

  if( ret != CAEN_FELib_Success) {
    //ErrorMsg("ReadData()");
    return ret;
  }

  //======== fill the ring buffers for the histograms and the event builder
  /// Done BEFORE the in-place scaling below, so both fields are scaled explicitly here and the
  /// raw-decode path above can apply exactly the same scaling to its own raw values.
  /// The channel guard also covers the histogram ring, which was missing one (the raw path has it).
  if( hit->dataType != DataFormat::Raw && hit->channel < nChannels ){
    ringBuffer[hit->channel].push({hit->energy, hit->energy_short});
    /// Only filled when something is actually analysing; see fillHitRing.
    if( fillHitRing.load(std::memory_order_relaxed) ){
      hitRing.push({ hit->timestamp * tick2ns,
                     hit->energy, hit->energy_short,
                     (uint16_t)(hit->fine_timestamp * tick2ns),
                     hit->channel,
                     (uint8_t)(hit->flags_high_priority & 0xFF) });
    }
  }

  hit->timestamp     *= tick2ns;

  /// Both fields are meant to carry physical time: timestamp in ns, fine_timestamp in ps.
  /// timestamp is exact. fine_timestamp is within 2.4%: the raw field is a 10-bit fraction of one
  /// tick, so a true picosecond value is raw * tick2ns * 1000/1024 (= 7.8125 ps per LSB at
  /// tick2ns 8, per format_RAW.md:308), while this scales by tick2ns alone and so reads 1024/1000
  /// high. Combining coarse + fine as ts + fine/1000 therefore overshoots the tick boundary by
  /// ~190 ps at the top of the fine range, which shows up as a sawtooth in timing spectra.
  /// Not changed here because the same convention is in Aux/EventBuilderRaw.cpp:137 and is already
  /// baked into every .sol file; correcting it means changing all of them together.
  hit->fine_timestamp *= tick2ns;

  //======== fill trace ring buffer for scope
  if( !hit->isTraceAllZero ){
    TraceSnapshot& ts = traceRingBuffer.nextSlot();
    ts.traceLenght = hit->traceLenght;
    memcpy(ts.analog_probes[0], hit->analog_probes[0], hit->traceLenght * sizeof(int32_t));
    memcpy(ts.analog_probes[1], hit->analog_probes[1], hit->traceLenght * sizeof(int32_t));
    memcpy(ts.digital_probes[0], hit->digital_probes[0], hit->traceLenght);
    memcpy(ts.digital_probes[1], hit->digital_probes[1], hit->traceLenght);
    memcpy(ts.digital_probes[2], hit->digital_probes[2], hit->traceLenght);
    memcpy(ts.digital_probes[3], hit->digital_probes[3], hit->traceLenght);
    traceRingBuffer.advance();
  }

  return ret;
}

//###########################################

void Digitizer2Gen::OpenOutFile(std::string fileName, const char * mode){
  /// A new file series always starts at index 0. This must happen HERE, not in StartACQ():
  /// MainWindow calls OpenOutFile() before Digitizer2Gen::StartACQ(), so a reset that lives only
  /// there keeps the previous run's rollover index -- run N's first file would be named after
  /// run N-1's last file, and run N's own later rollover would then reopen that same name with
  /// "wb" and silently truncate the run's first 2 GB.
  outFileIndex = 0;
  FinishedOutFilesSize = 0;
  fileWriteError = false;
  droppedHitCount = 0;
  outFileNameBase = fileName;
  const char * ext = (hit && hit->dataType == DataFormat::Raw) ? "raw" : "sol";
  snprintf(outFileName, sizeof(outFileName), "%s_%03d.%s", fileName.c_str(), outFileIndex, ext);
  outFile = fopen(outFileName, mode);
  if( outFile == NULL ){
    printf("OpenOutFile: failed to open file '%s'\n", outFileName);
    outFileSize = 0;
    return;
  }
  fseek(outFile, 0L, SEEK_END);
  outFileSize = ftell(outFile);  // unsigned int =  Max ~4GB

}

/// Idempotent: outFile is nulled, so a second call does nothing. Two paths reach here -- the
/// end-of-run loop in MainWindow::StopACQ() and MainWindow::CloseDigitizers() -- and without the
/// null, the second fclose() would be on an already-closed FILE*, and SaveDataToFile()'s
/// "if( outFile == NULL ) return" guard would be looking at a dangling pointer.
void Digitizer2Gen::CloseOutFile(){
  if( outFile != NULL ) {
    fclose(outFile);
    outFile = NULL;
    int result = chmod(outFileName, S_IRUSR | S_IRGRP | S_IROTH);
    if( result != 0 ) printf("somewrong when set file (%s) to read only.", outFileName);
  }
}

void Digitizer2Gen::SaveDataToFile(){

  if( outFile == NULL ){
    /// The error branch below (or the rollover open) closed the file; every hit from here on is
    /// lost. Count them so ReadDataThread can report the damage in the DAQ log.
    if( fileWriteError ) droppedHitCount ++;
    return;
  }

  if( (unsigned long) outFileSize > MaxOutFileSize ){
    FinishedOutFilesSize += ftell(outFile);
    CloseOutFile();
    outFileIndex ++;
    const char * ext = (hit && hit->dataType == DataFormat::Raw) ? "raw" : "sol";
    snprintf(outFileName, sizeof(outFileName), "%s_%03d.%s", outFileNameBase.c_str(), outFileIndex, ext);
    outFile = fopen(outFileName, "wb"); //overwrite binary
    if( outFile == NULL ){
      fileWriteError = true;  /// this hit is lost too
      droppedHitCount ++;
      printf("SaveDataToFile: failed to open new file '%s'\n", outFileName);
      return;
    }
  }

  if( hit->dataType == DataFormat::ALL){
    fwrite(&dataStartIdentifier,      2, 1, outFile);
    fwrite(&hit->channel,             1, 1, outFile);
    fwrite(&hit->energy,              2, 1, outFile);
    if( FPGAType == DPPType::PSD ) fwrite(&hit->energy_short, 2, 1, outFile);
    fwrite(&hit->timestamp,           6, 1, outFile);
    fwrite(&hit->fine_timestamp,      2, 1, outFile);
    fwrite(&hit->flags_high_priority, 1, 1, outFile);
    fwrite(&hit->flags_low_priority,  2, 1, outFile);
    fwrite(&hit->downSampling,        1, 1, outFile);
    fwrite(&hit->board_fail,          1, 1, outFile);
    fwrite(&hit->flush,               1, 1, outFile);
    fwrite(&hit->trigger_threashold,  2, 1, outFile);
    fwrite(&hit->event_size,          8, 1, outFile);
    fwrite(&hit->aggCounter,          4, 1, outFile);
    fwrite(&hit->traceLenght,         8, 1, outFile);
    fwrite(hit->analog_probes_type,   2, 1, outFile);
    fwrite(hit->digital_probes_type,  4, 1, outFile);
    fwrite(hit->analog_probes[0], hit->traceLenght*4, 1, outFile);
    fwrite(hit->analog_probes[1], hit->traceLenght*4, 1, outFile);
    fwrite(hit->digital_probes[0], hit->traceLenght, 1, outFile);
    fwrite(hit->digital_probes[1], hit->traceLenght, 1, outFile);
    fwrite(hit->digital_probes[2], hit->traceLenght, 1, outFile);
    fwrite(hit->digital_probes[3], hit->traceLenght, 1, outFile);

  }else if( hit->dataType == DataFormat::OneTrace){
    fwrite(&dataStartIdentifier,        2, 1, outFile);
    fwrite(&hit->channel,               1, 1, outFile);
    fwrite(&hit->energy,                2, 1, outFile);
    if( FPGAType == DPPType::PSD ) fwrite(&hit->energy_short, 2, 1, outFile);
    fwrite(&hit->timestamp,             6, 1, outFile);
    fwrite(&hit->fine_timestamp,        2, 1, outFile);
    fwrite(&hit->flags_high_priority,   1, 1, outFile);
    fwrite(&hit->flags_low_priority,    2, 1, outFile);
    fwrite(&hit->traceLenght,           8, 1, outFile);
    fwrite(&hit->analog_probes_type[0], 1, 1, outFile);
    fwrite(hit->analog_probes[0], hit->traceLenght*4, 1, outFile);

  }else if( hit->dataType == DataFormat::NoTrace ){
    fwrite(&dataStartIdentifier,      2, 1, outFile);
    fwrite(&hit->channel,             1, 1, outFile);
    fwrite(&hit->energy,              2, 1, outFile);
    if( FPGAType == DPPType::PSD ) fwrite(&hit->energy_short, 2, 1, outFile);
    fwrite(&hit->timestamp,           6, 1, outFile);
    fwrite(&hit->fine_timestamp,      2, 1, outFile);
    fwrite(&hit->flags_high_priority, 1, 1, outFile);
    fwrite(&hit->flags_low_priority,  2, 1, outFile);

  }else if( hit->dataType == DataFormat::MiniWithFineTime ){
    fwrite(&dataStartIdentifier, 2, 1, outFile);
    fwrite(&hit->channel,        1, 1, outFile);
    fwrite(&hit->energy,         2, 1, outFile);
    if( FPGAType == DPPType::PSD ) fwrite(&hit->energy_short, 2, 1, outFile);
    fwrite(&hit->timestamp,      6, 1, outFile);
    fwrite(&hit->fine_timestamp, 2, 1, outFile);

  }else if( hit->dataType == DataFormat::Minimum ){
    fwrite(&dataStartIdentifier, 2, 1, outFile);
    fwrite(&hit->channel,        1, 1, outFile);
    fwrite(&hit->energy,         2, 1, outFile);
    if( FPGAType == DPPType::PSD ) fwrite(&hit->energy_short, 2, 1, outFile);
    fwrite(&hit->timestamp,      6, 1, outFile);

  }else if( hit->dataType == DataFormat::Raw){
    fwrite(&dataStartIdentifier,  2, 1, outFile);
    fwrite(&hit->dataSize,        8, 1, outFile);
    fwrite(hit->data, hit->dataSize, 1, outFile);
  }

  /// A short write corrupts the file from here on. ferror() catches it in any format
  /// branch without counting the expected bytes; stop rather than keep appending.
  /// Set the flag BEFORE closing: from now on every hit is dropped, and ReadDataThread
  /// reports it in the DAQ log -- the printf alone goes to a terminal no operator watches.
  if( ferror(outFile) ){
    fileWriteError = true;  /// this hit is lost too
    droppedHitCount ++;
    printf("SaveDataToFile: write error on '%s'\n", outFileName);
    CloseOutFile();
    return;
  }

  outFileSize = ftell(outFile);  // unsigned int =  Max ~4GB

}


//###########################################
void Digitizer2Gen::Reset(){ SendCommand("/cmd/Reset"); }

void Digitizer2Gen::ProgramBoard(){
  if( !isConnected ) return ;

  //============= Board
  WriteValue("/par/ClockSource"          , "Internal");
  WriteValue("/par/EnClockOutFP"         , "True");
  WriteValue("/par/StartSource"          , "SWcmd");
  WriteValue("/par/GlobalTriggerSource"  , "TrgIn");

  WriteValue("/par/TrgOutMode"      , "Disabled");
  WriteValue("/par/GPIOMode"        , "Disabled");
  WriteValue("/par/BusyInSource"    , "Disabled");
  WriteValue("/par/SyncOutMode"     , "Disabled"); 
  WriteValue("/par/BoardVetoSource" , "Disabled"); 

  WriteValue("/par/RunDelay"  , "0"); // ns, that is for sync time with multi board
  WriteValue("/par/IOlevel"   , "NIM");

  WriteValue("/par/EnAutoDisarmAcq" , "False");
  WriteValue("/par/EnStatEvents", "true"); // enable for both PHA and PSD (needed for raw decoder scaler)
  
  WriteValue("/par/BoardVetoWidth"    , "0");
  WriteValue("/par/VolatileClockOutDelay"    , "0");
  WriteValue("/par/PermanentClockOutDelay"    , "0");

  WriteValue("/par/DACoutMode"         , "ChInput");
  WriteValue("/par/DACoutChSelect"     , "0");

  //============== ITL
  WriteValue("/par/ITLAMainLogic"    , "OR"); 
  WriteValue("/par/ITLAMajorityLev"  , "2"); 
  WriteValue("/par/ITLAPairLogic"    , "NONE"); 
  WriteValue("/par/ITLAPolarity"     , "Direct"); 
  WriteValue("/par/ITLAGateWidth"    , "100"); 

  WriteValue("/par/ITLBMainLogic"    , "OR"); 
  WriteValue("/par/ITLBMajorityLev"  , "2"); 
  WriteValue("/par/ITLBPairLogic"    , "NONE"); 
  WriteValue("/par/ITLBPolarity"     , "Direct"); 
  WriteValue("/par/ITLBGateWidth"    , "100"); 

}

void Digitizer2Gen::ProgramChannels(bool testPulse){

  std::string para_prefix = "/ch/0.." + std::to_string(nChannels-1) + "/par/";

  // Channel setting  
  if( testPulse){
    WriteValue((para_prefix + "ChEnable").c_str()  , "false");

    WriteValue((para_prefix + "EventTriggerSource").c_str(), "GlobalTriggerSource");
    WriteValue((para_prefix + "WaveTriggerSource").c_str() , "GlobalTriggerSource"); // EventTriggerSource enought

    WriteValue("/par/GlobalTriggerSource", "SwTrg | TestPulse");
    WriteValue("/par/TestPulsePeriod"    , "1000000"); // 1.0 msec = 1000Hz, tested, 1 trace recording
    WriteValue("/par/TestPulseWidth"     , "1000"); // nsec
    WriteValue("/par/TestPulseLowLevel"  , "0");
    WriteValue("/par/TestPulseHighLevel" , "10000");

  }else{

    //======== Self trigger for each channel 
    WriteValue((para_prefix + "ChEnable").c_str() ,  "True");

    WriteValue((para_prefix + "DCOffset").c_str()           ,  "50");
    WriteValue((para_prefix + "TriggerThr").c_str()         ,  "1000");
    WriteValue((para_prefix + "WaveDataSource").c_str()     ,  "ADC_DATA");
    WriteValue((para_prefix + "PulsePolarity").c_str()      ,  "Positive");
    WriteValue((para_prefix + "ChRecordLengthT").c_str()    ,  "4096");      /// 4096 ns, S and T are not Sync
    WriteValue((para_prefix + "ChPreTriggerT").c_str()      ,  "1000");

    WriteValue((para_prefix + "WaveSaving").c_str()         ,  "OnRequest");
    WriteValue((para_prefix + "WaveResolution").c_str()     ,  "RES8");

    //======== Trigger setting
    WriteValue((para_prefix + "EventTriggerSource").c_str()  , "ChSelfTrigger");
    WriteValue((para_prefix + "WaveTriggerSource").c_str()   , "Disabled"); 
    WriteValue((para_prefix + "ChannelVetoSource").c_str()   , "Disabled"); 
    WriteValue((para_prefix + "ChannelsTriggerMask").c_str() , "0x0");
    WriteValue((para_prefix + "CoincidenceMask").c_str()     , "Disabled");
    WriteValue((para_prefix + "AntiCoincidenceMask").c_str() , "Disabled");
    WriteValue((para_prefix + "CoincidenceLengthT").c_str()  , "0");
    WriteValue((para_prefix + "ADCVetoWidth").c_str()        , "0");

    //======== Other Setting
    WriteValue((para_prefix + "EventSelector").c_str()                , "All");
    WriteValue((para_prefix + "WaveSelector").c_str()                 , "All");
    WriteValue((para_prefix + "EnergySkimLowDiscriminator").c_str()   , "0");
    WriteValue((para_prefix + "EnergySkimHighDiscriminator").c_str()  , "65534");
    WriteValue((para_prefix + "ITLConnect").c_str()                   , "Disabled");

    if( FPGAType == DPPType::PHA){

      WriteValue((para_prefix + "TimeFilterRiseTimeT").c_str()       , "80");   // 80 ns
      WriteValue((para_prefix + "TimeFilterRetriggerGuardT").c_str() , "80");   // 80 ns

      WriteValue((para_prefix + "EnergyFilterLFLimitation").c_str() , "Off");
  
      //======== Trapezoid setting
      WriteValue((para_prefix + "EnergyFilterRiseTimeT").c_str()       , "496");   //  496 ns
      WriteValue((para_prefix + "EnergyFilterFlatTopT").c_str()        , "1600");  // 1600 ns
      WriteValue((para_prefix + "EnergyFilterPoleZeroT").c_str()       , "50000"); // 50 us
      WriteValue((para_prefix + "EnergyFilterPeakingPosition").c_str() , "20");   // 20 % = Flatup * 20% = 320 ns
      WriteValue((para_prefix + "EnergyFilterBaselineGuardT").c_str()   , "800");  // 800 ns
      WriteValue((para_prefix + "EnergyFilterPileupGuardT").c_str()     , "80");   // 80 ns
      WriteValue((para_prefix + "EnergyFilterBaselineAvg").c_str()     , "Medium"); // 1024 sample
      WriteValue((para_prefix + "EnergyFilterFineGain").c_str()        , "1.0");
      WriteValue((para_prefix + "EnergyFilterPeakingAvg").c_str()      , "LowAVG");

      //======== Probe Setting
      WriteValue((para_prefix + "WaveAnalogProbe0").c_str()  , "ADCInput");
      WriteValue((para_prefix + "WaveAnalogProbe1").c_str()  , "EnergyFilterMinusBaseline");
      WriteValue((para_prefix + "WaveDigitalProbe0").c_str() , "Trigger");
      WriteValue((para_prefix + "WaveDigitalProbe1").c_str() , "EnergyFilterPeaking");
      WriteValue((para_prefix + "WaveDigitalProbe2").c_str() , "TimeFilterArmed");
      WriteValue((para_prefix + "WaveDigitalProbe3").c_str() , "EnergyFilterPeakReady");

    }

    if( FPGAType == DPPType::PSD ){

      WriteValue((para_prefix + "WaveAnalogProbe0").c_str()   ,  "ADCInput");
      WriteValue((para_prefix + "WaveAnalogProbe1").c_str()   ,  "CFDFilter");
      WriteValue((para_prefix + "WaveDigitalProbe0").c_str()  ,  "Trigger");
      WriteValue((para_prefix + "WaveDigitalProbe1").c_str()  ,  "LongGate");
      WriteValue((para_prefix + "WaveDigitalProbe2").c_str()  ,  "ShortGate");
      WriteValue((para_prefix + "WaveDigitalProbe3").c_str()  ,  "ChargeReady");

      //=========== QDC
      WriteValue((para_prefix + "GateLongLengthT").c_str()                ,  "400");
      WriteValue((para_prefix + "GateShortLengthT").c_str()               ,  "100");
      WriteValue((para_prefix + "GateOffsetT").c_str()                    ,  "50");
      WriteValue((para_prefix + "LongChargeIntegratorPedestal").c_str()   ,  "0");
      WriteValue((para_prefix + "ShortChargeIntegratorPedestal").c_str()  ,  "0");
      WriteValue((para_prefix + "EnergyGain").c_str()                     ,  "x1");

      //=========== Discrimination
      WriteValue((para_prefix + "TriggerFilterSelection").c_str()         ,  "LeadingEdge");
      WriteValue((para_prefix + "CFDDelayT").c_str()                      ,  "32");
      WriteValue((para_prefix + "CFDFraction").c_str()                    ,  "25");
      WriteValue((para_prefix + "TimeFilterSmoothing").c_str()            ,  "Disabled");
      WriteValue((para_prefix + "ChargeSmoothing").c_str()                ,  "Disabled");
      WriteValue((para_prefix + "SmoothingFactor").c_str()                ,  "1");
      WriteValue((para_prefix + "PileupGap").c_str()                      ,  "1000");

      //=========== Input
      WriteValue((para_prefix + "ADCInputBaselineAvg").c_str()            ,  "MediumHigh");
      WriteValue((para_prefix + "AbsoluteBaseline").c_str()               ,  "1000");
      WriteValue((para_prefix + "ADCInputBaselineGuardT").c_str()         ,  "0");
      WriteValue((para_prefix + "TimeFilterRetriggerGuardT").c_str()      ,  "8");
      WriteValue((para_prefix + "TriggerHysteresis").c_str()              ,  "Enabled");
      
      //========== Other
      WriteValue((para_prefix + "NeutronThreshold").c_str()               ,  "0");
      WriteValue((para_prefix + "EventNeutronReject").c_str()             ,  "Disabled");
      WriteValue((para_prefix + "WaveNeutronReject").c_str()              ,  "Disabled");

    }

  }
}

/// Model -> sensor complement. One rule, one place. Everything else about a model stays where it
/// is; this is deliberately only the environment sensors, which is where the duplication was.
void Digitizer2Gen::ResolveModelSensors(){
  /// Measured on the test station: a VX2745 answers TempSensADC0 through TempSensADC7.
  /// The other models are carried over from the exclusion lists this replaces, which agreed that
  /// only ADC0 is present -- unverified, no such board here.
  nTempSensADC = ( ModelName == "VX2745" ) ? 8 : 1;
}

bool Digitizer2Gen::IsSensorReadable(const Reg & para) const {
  const std::string & name = para.GetPara();

  for( int i = nTempSensADC; i < (int) PHA::DIG::TempSensADC.size(); i++ ){
    if( name == PHA::DIG::TempSensADC[i].GetPara() ) return false;
  }

  /// VX2740 only. A VX2745 returns CAEN_FELib error -6 for both, confirmed on hardware, which is
  /// what the "(not readable)" in the original comment meant.
  if( name == PHA::DIG::FreqSensCore.GetPara() ||
      name == PHA::DIG::DutyCycleSensDCDC.GetPara() ) return ModelName == "VX2740";

  return true;
}

void Digitizer2Gen::PrintBoardSettings(){

  for(int i = 0; i < (int) boardSettings.size(); i++){
    if( boardSettings[i].ReadWrite() == RW::WriteOnly) continue;
    if( !IsSensorReadable(boardSettings[i]) ) continue;

    printf("%-45s  %d  %s\n", boardSettings[i].GetFullPara().c_str(),  
                              boardSettings[i].ReadWrite() ,
                              boardSettings[i].GetValue().c_str());
  }

  if( ModelName == "VX2745" && FPGAType == DPPType::PHA) {
    for(int i = 0; i < 4 ; i ++){
      printf("%-45s  %d  %s\n", VGASetting[i].GetFullPara(i).c_str(), 
                                VGASetting[i].ReadWrite(), 
                                VGASetting[i].GetValue().c_str());
    }
  }

  /// Match ReadAllSettings()/SaveSettingsToFile(): InputDelay exists only when
  /// CupVer >= 2023091800 AND the model is not VX2730.
  if( CupVer >= 2023091800 && ModelName != "VX2730" ){
    for(int idx = 0; idx < 16 ; idx ++ ){
      printf("%-45s  %d  %s\n", InputDelay[idx].GetFullPara(idx).c_str(), 
                                InputDelay[idx].ReadWrite(), 
                                InputDelay[idx].GetValue().c_str());
    }
  }

  for( int i = 0; i < (int) LVDSSettings[0].size(); i++){
    for( int index = 0; index < 4; index++){
      if( LVDSSettings[index][i].ReadWrite() == RW::WriteOnly) continue;
      printf("%-45s  %d  %s\n", LVDSSettings[index][i].GetFullPara(index).c_str(), 
                                LVDSSettings[index][i].ReadWrite(),
                                LVDSSettings[index][i].GetValue().c_str());
    }
  }

}

void Digitizer2Gen::PrintChannelSettings(unsigned short ch){

  if( ch >= MaxNumberOfChannel ) return;
  for( int i = 0; i < (int) chSettingTable.size(); i++){
    if( chSettingTable[i].ReadWrite() == RW::WriteOnly) continue;
    printf("%-45s  %d  %s\n", chSettingTable[i].GetFullPara(ch, nChannels).c_str(),
                              chSettingTable[i].ReadWrite(),
                              chValue[ch][i].c_str());
  }
}

std::string Digitizer2Gen::ErrorMsg(const char * funcName, int ret){
  printf("======== %s | %5d | %s\n",__func__, serialNumber, funcName);
  char msg[1024];
  int ec = CAEN_FELib_GetErrorDescription((CAEN_FELib_ErrorCode) ret, msg);
  if (ec != CAEN_FELib_Success) {
    std::string errMsg = __func__;
    errMsg += " failed";
    printf("%s failed\n", __func__);
    return errMsg;
  }
  if( ret != CAEN_FELib_Stop ) printf("Error msg (%d): %s\n", ret, msg);
  return msg;
}

//^===================================================== Settings
void Digitizer2Gen::ReadAllSettings(){
  if( !isConnected ) return;

  printf("Digitizer2Gen::%s | %s \n", __func__, FPGAType.c_str());

  for(int i = 0; i < (int) boardSettings.size(); i++){
    if( boardSettings[i].ReadWrite() == RW::WriteOnly) continue;

    /// Same sensor rule as the print and save loops -- see IsSensorReadable. This site used to
    /// carry its own, and it was the broken one: "ModelName == VX2740 && para != TempSensADC0"
    /// without the ReadOnly qualifier its twin in SaveSettingsToFile has, so on a VX2740 this
    /// loop skipped EVERY board setting and left the whole board cache empty.
    if( !IsSensorReadable(boardSettings[i]) ) continue;
    ReadValue(boardSettings[i]);
  }

  if( ModelName == "VX2745") for(int i = 0; i < 4 ; i ++) ReadValue(VGASetting[i], i);

  if( ModelName != "VX2730"){
    if( CupVer >= 2023091800 ) for( int idx = 0; idx < 16; idx++) ReadValue(InputDelay[idx], idx, false);
  }

  for( int index = 0; index < 4; index++){
    for( int i = 0; i < (int) LVDSSettings[index].size(); i++){
      if( LVDSSettings[index][i].ReadWrite() == RW::WriteOnly) continue;
      ReadValue(LVDSSettings[index][i], index, false);
      //printf("%d %d | %s | %s \n", index, i, LVDSSettings[index][i].GetPara().c_str(), LVDSSettings[index][i].GetValue().c_str());
    }
  }

  for(int ch = 0; ch < nChannels && ch < MaxNumberOfChannel ; ch++ ){
    for( int i = 0; i < (int) chSettingTable.size(); i++){
      if( chSettingTable[i].ReadWrite() == RW::WriteOnly) continue;
      if( ModelName != "VX2730" && chSettingTable[i].GetPara() == PSD::CH::ChGain.GetPara()) continue;
      ReadValue(chSettingTable[i], ch);
    }
  }

}

int Digitizer2Gen::SaveSettingsToFile(const char * saveFileName, bool setReadOnly){
  if( saveFileName != NULL) settingFileName = saveFileName;

  /// The file's id column encodes a channel setting as ch*100 + index, with 7000 reserved for the
  /// start of the LVDS block. That leaves two ceilings, and the reader cannot detect either one:
  /// past them the decoded (ch, index) pair is self-consistent, just wrong, so a violated file
  /// loads silently into the wrong channel. Neither is reachable on today's hardware -- 64
  /// channels and 61 PSD settings -- but there is no headroom to speak of, so fail here rather
  /// than write a file that will misroute.
  ///
  /// This is a stopgap. The real fix is a wider id field, which is a file-format change.
  if( nChannels * 100 >= 7000 ){
    printf("%s : %d channels needs ids up to %d, which collides with the LVDS block at 7000. "
           "Not saving.\n", __func__, nChannels, nChannels * 100);
    return -1;
  }
  if( chSettingTable.size() > 100 ){
    printf("%s : %zu channel settings does not fit the 100-per-channel id range; channel ids "
           "would overlap. Not saving.\n", __func__, chSettingTable.size());
    return -1;
  }

  int totCount = 0;
  int count = 0;
  FILE * saveFile = fopen(settingFileName.c_str(), "w");
  if( saveFile ){
    for(int i = 0; i < (int) boardSettings.size(); i++){
      if( boardSettings[i].ReadWrite() == RW::WriteOnly) continue;

      //--- exclude Gateway
      if( boardSettings[i].GetPara() == PHA::DIG::Gateway.GetPara()) continue;

      /// Same sensor rule as the read and print loops -- see IsSensorReadable. The three lists
      /// this replaces disagreed with each other; this site's third clause, "on a VX2740 no
      /// read-only setting but TempSensADC0", also swallowed FreqSensCore and DutyCycleSensDCDC,
      /// which its own clause above exists to keep. Both are ReadOnly, so on a VX2740 they were
      /// never saved at all -- the two clauses cancelled.
      if( !IsSensorReadable(boardSettings[i]) ) continue;

      /// Counted only once the setting is known to be one we mean to write, so that totCount and
      /// count differ in exactly one case: the empty value below, which is what the
      /// count != totCount check at the end of this function reports. The channel loop further
      /// down already has this shape.
      totCount ++;

      if( boardSettings[i].GetValue() == "") {
        printf(" No value for %s \n", boardSettings[i].GetPara().c_str());
        continue;
      }
      fprintf(saveFile, "%-45s!%d!%4d!%s\n", boardSettings[i].GetFullPara().c_str(),  
                                             boardSettings[i].ReadWrite(),
                                             8000 + i, 
                                             boardSettings[i].GetValue().c_str());
      count ++;
    }

    if( CupVer >= 2023091800 && ModelName != "VX2730" ){
      for( int idx = 0; idx < 16; idx ++){
        totCount ++;
        if( InputDelay[idx].GetValue() == "" ) {
          printf(" No value for %s \n", InputDelay[idx].GetPara().c_str());
          continue;
        }
        fprintf(saveFile, "%-45s!%d!%4d!%s\n", InputDelay[idx].GetFullPara(idx).c_str(), 
                                              InputDelay[idx].ReadWrite(), 
                                              9050 + idx,
                                              InputDelay[idx].GetValue().c_str());
        count ++;
      }
    }

    if( ModelName == "VX2745" && FPGAType == DPPType::PHA) {
      for(int i = 0; i < 4 ; i ++){
        totCount ++;
        if( VGASetting[i].GetValue() == "" ) {
          printf(" No value for %s \n", VGASetting[i].GetPara().c_str());
          continue;
        }
        fprintf(saveFile, "%-45s!%d!%4d!%s\n", VGASetting[i].GetFullPara(i).c_str(), 
                                              VGASetting[i].ReadWrite(), 
                                              9000 + i,
                                              VGASetting[i].GetValue().c_str());
        count ++;
      }
    }

    for( int i = 0; i < (int) LVDSSettings[0].size(); i++){
      for( int index = 0; index < 4; index++){
        if( LVDSSettings[index][i].ReadWrite() == RW::WriteOnly) continue;
        totCount ++;
        if( LVDSSettings[index][i].GetValue() == "") {
          printf(" No value for %s \n", LVDSSettings[index][i].GetPara().c_str());
          continue;
        }
        fprintf(saveFile, "%-45s!%d!%4d!%s\n", LVDSSettings[index][i].GetFullPara(index).c_str(), 
                                               LVDSSettings[index][i].ReadWrite(),
                                               7000 + 4 * i + index,
                                               LVDSSettings[index][i].GetValue().c_str());
        count ++;
      }
    }

    for( int i = 0; i < (int) chSettingTable.size(); i++){
      for(int ch = 0; ch < nChannels && ch < MaxNumberOfChannel ; ch++ ){
        if( chSettingTable[i].ReadWrite() == RW::WriteOnly) continue;
        //--- exclude ChGain for non-VX2730 (not readable)
        if( ModelName != "VX2730" && chSettingTable[i].GetPara() == PSD::CH::ChGain.GetPara()) {
          continue;
        }
        totCount ++;
        if( chValue[ch][i] == "") {
          printf("[%i] No value for %s , ch-%02d\n", i, chSettingTable[i].GetPara().c_str(), ch);
          continue;
        }
        fprintf(saveFile, "%-45s!%d!%4d!%s\n", chSettingTable[i].GetFullPara(ch, nChannels).c_str(),
                                               chSettingTable[i].ReadWrite(),
                                               ch*100 + i,
                                               chValue[ch][i].c_str());
        count ++;
      }
    }

    /// A failed fprintf leaves a truncated settings file that LoadSettingsFromFile()
    /// would read back as valid. Report it and do not treat the file as complete.
    if( ferror(saveFile) ){
      printf("SaveSettingsToFile: write error on '%s'\n", settingFileName.c_str());
      fclose(saveFile);
      return -1;
    }

    if( fclose(saveFile) != 0 ){
      printf("SaveSettingsToFile: close error on '%s'\n", settingFileName.c_str());
      return -1;
    }

    if( count != totCount ) {
      printf("!!!!! some setting is empty. !!!!!! ");
      return -1;
    }

    if( setReadOnly ){
      /// settingFileName, not saveFileName: the parameter defaults to NULL (meaning "reuse the
      /// stored name"), and chmod(NULL, ...) is undefined. settingFileName is the name actually
      /// written to, whichever way the caller asked.
      int result = chmod(settingFileName.c_str(), S_IRUSR | S_IRGRP | S_IROTH);
      if( result != 0 ) printf("somewrong when set file (%s) to read only.", settingFileName.c_str());
    }

    //printf("Saved setting files to %s\n", saveFileName);
    return 1;

  }else{
    //printf("Save file accessing error.");
  }

  return 0;
}

int Digitizer2Gen::ReadAndSaveSettingsToFile(const char *saveFileName){
  ReadAllSettings();
  return SaveSettingsToFile(saveFileName);
}

bool Digitizer2Gen::LoadSettingsFromFile(const char * loadFileName){

  if( loadFileName != NULL) settingFileName = loadFileName;

  FILE * loadFile = fopen(settingFileName.c_str(), "r");

  if( loadFile ){
    printf("Opened %s\n", settingFileName.c_str());
    std::string para;
    std::string readWrite;
    std::string idStr;
    std::string value;

    char line[512];
    while(fgets(line, sizeof(line), loadFile) != NULL){

      //printf("%s", line);
      char* token = std::strtok(line, "!");
      int count = 0;
      while( token != nullptr){

        /// Trim the ends only. This used to remove_if() every space in the token, which also
        /// collapsed spaces INSIDE a value: a multi-source setting saved as "SwTrg | TestPulse"
        /// (the form ProgramChannels() writes) came back as "SwTrg|TestPulse" and was written to
        /// the board in that shape.
        size_t len = std::strcspn(token, "\n");
        token[len] = '\0';

        char * beg = token;
        while( *beg != '\0' && std::isspace((unsigned char) *beg) ) beg++;
        char * end = beg + std::strlen(beg);
        while( end > beg && std::isspace((unsigned char) *(end-1)) ) end--;
        *end = '\0';
        token = beg;

        if( count == 0 ) para = token;
        if( count == 1 ) readWrite = token;
        if( count == 2 ) idStr = token;
        if( count == 3 ) value = token;
        if( count > 3) break;

        count ++;
        token = std::strtok(nullptr, "!");
      }

      int id = atoi(idStr.c_str());
      /// A malformed or missing id used to fall through to the channel branch and index with a
      /// negative value. Only the upper bounds were ever checked.
      if( id < 0 ){
        printf("LoadSettingsFromFile: ignoring line with bad id |%s| (%s)\n",
               idStr.c_str(), para.c_str());
      }else if( id < 7000){ // channel
        int ch = id / 100;
        int index = id - ch * 100;
        if( ch < nChannels && ch < MaxNumberOfChannel && index >= 0 && index < (int) chValue[ch].size() )
          chValue[ch][index] = value;
        //printf("-------id : %d, ch: %d, index : %d\n", id,  ch, index);
        //printf("%s|%d|%d|%s|\n", chSettingTable[index].GetFullPara(ch).c_str(),
        //                         chSettingTable[index].ReadWrite(), id,
        //                         chValue[ch][index].c_str());

      }else if ( 7000 <= id && id < 8000){ // LVDS
        int index = (id-7000) % 4;
        int ch = (id-7000) / 4;
        if( index < 4 && ch < (int) LVDSSettings[index].size() )
          LVDSSettings[index][ch].SetValue(value.c_str());

      }else if ( 8000 <= id && id < 9000){ // board
        if( id - 8000 < (int) boardSettings.size() )
          boardSettings[id - 8000].SetValue(value.c_str());
        //printf("%s|%d|%d|%s\n", boardSettings[id-8000].GetFullPara().c_str(),
        //                        boardSettings[id-8000].ReadWrite(), id,
        //                        boardSettings[id-8000].GetValue().c_str());
      }else if ( 9000 <= id && id < 9050){ // vga
        if( id - 9000 < 4 && ModelName == "VX2745" && FPGAType == DPPType::PHA )
          VGASetting[id - 9000].SetValue(value.c_str());
      }else if ( id >= 9050 ){ // group
        if( CupVer >= 2023091800 && ModelName != "VX2730" && id - 9050 < 16 )
          InputDelay[id - 9050].SetValue(value.c_str());
      }
      //printf("%s|%s|%d|%s|\n", para.c_str(), readWrite.c_str(), id, value.c_str());
      if( readWrite == "2" && isConnected)  WriteValue(para.c_str(), value.c_str(), false);
    }

    fclose(loadFile);
    return true;
  }else{
    printf("Fail to opened %s\n", settingFileName.c_str());
  }

  return false;
  
}

/// Every index is bounded here. The arrays have very different lengths -- chValue[64],
/// VGASetting[4], LVDSSettings[4], InputDelay[16] -- and ch_index reaches this from panel code and
/// from a user-supplied Mapping.h, so a channel number landing on the VGA or LVDS case walks off
/// the end of a Reg array and reads std::strings out of unrelated memory. Same guard style as
/// GetChSettingByName() below.
std::string Digitizer2Gen::GetSettingValueFromMemory(const Reg &para, unsigned int ch_index) {
  int index = FindIndex(para);
  if( index < 0 ) return "invalid";
  const unsigned int ch = ch_index;
  switch (para.GetType()){
    case TYPE::DIG:
      return ( index < (int) boardSettings.size() ) ? boardSettings[index].GetValue() : "invalid";
    case TYPE::CH:
      if( ch >= MaxNumberOfChannel || index >= (int) chValue[ch].size() ) return "invalid";
      return chValue[ch][index];
    case TYPE::VGA:
      return ( ch < 4 ) ? VGASetting[ch].GetValue() : "invalid";
    case TYPE::LVDS:
      if( ch >= 4 || index >= (int) LVDSSettings[ch].size() ) return "invalid";
      return LVDSSettings[ch][index].GetValue();
    case TYPE::GROUP:
      return ( ch < MaxNumberOfGroup ) ? InputDelay[ch].GetValue() : "invalid";
    default : return "invalid";
  }
  return "no such parameter";
}

std::string Digitizer2Gen::GetBoardSettingByName(const std::string & name, bool * found) const {
  const int index = LookUp(boardMap, name);
  if( index < 0 || index >= (int) boardSettings.size() ){
    if( found ) *found = false;
    return "";
  }
  if( found ) *found = true;
  return boardSettings[index].GetValue();
}

std::string Digitizer2Gen::GetChSettingByName(const std::string & name, int ch, bool * found) const {
  const int index = LookUp(chMap, name);
  if( index < 0 || ch < 0 || ch >= MaxNumberOfChannel || index >= (int) chValue[ch].size() ){
    if( found ) *found = false;
    return "";
  }
  if( found ) *found = true;
  return chValue[ch][index];
}
