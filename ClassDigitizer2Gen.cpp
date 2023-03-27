#include "ClassDigitizer2Gen.h"

#include <cstring>
#include <algorithm>
#include <sys/stat.h>

Digitizer2Gen::Digitizer2Gen(){  
  printf("======== %s \n",__func__);
  Initialization();
}

Digitizer2Gen::~Digitizer2Gen(){
  printf("========Digitizer2Gen::%s (%d)\n",__func__, serialNumber);
  if(isConnected ) CloseDigitizer();
}

void Digitizer2Gen::Initialization(){
  printf("======== %s \n",__func__);

  handle = 0;
  ret = 0;
  isConnected = false;
  isDummy = false;

  serialNumber = 0;
  FPGAType = "";
  nChannels = 0;
  ch2ns = 0;

  outFileIndex = 0;
  FinishedOutFilesSize = 0;
  dataStartIndetifier = 0xAAA0;
  outFile = NULL;
  outFileSize = 0;

  evt = NULL;

  acqON = false;

  settingFileName = "";
  boardSettings = PHA::DIG::AllSettings;
  for( int ch = 0; ch < MaxNumberOfChannel ; ch ++) chSettings[ch] = PHA::CH::AllSettings;
  for( int index = 0 ; index < 4; index ++) VGASetting[index] = PHA::VGA::VGAGain;

  //build map
  for( int i = 0; i < (int) PHA::DIG::AllSettings.size(); i++) boardMap[PHA::DIG::AllSettings[i].GetPara()] = i;
  for( int i = 0; i < (int) PHA::CH::AllSettings.size(); i++) chMap[PHA::CH::AllSettings[i].GetPara()] = i;
  
}

void Digitizer2Gen::SetDummy(unsigned short sn){

  isDummy = true;
  serialNumber = sn;
  nChannels = 64;
  FPGAType = "DPP_PHA";

}

//########################################### Handles functions
uint64_t Digitizer2Gen::GetHandle(const char * parameter){
  
  uint64_t par_handle;
  ret = CAEN_FELib_GetHandle(handle, parameter, &par_handle);
  if(ret != CAEN_FELib_Success) {
    ErrorMsg(__func__);
    return 0;
  }
  return par_handle;
  
}

uint64_t Digitizer2Gen::GetParentHandle(uint64_t handle){
  uint64_t par_handle;
  ret = CAEN_FELib_GetParentHandle(handle, NULL, &par_handle);
  if(ret != CAEN_FELib_Success) {
    ErrorMsg(__func__);
    return 0;
  }
  return par_handle;
}

std::string Digitizer2Gen::GetPath(uint64_t handle){
  char path[256];
  ret = CAEN_FELib_GetPath(handle, path);
  if(ret != CAEN_FELib_Success) {
    ErrorMsg(__func__);
    return "Error";
  }
  return path;
  
}

//########################################### Read Write

int Digitizer2Gen::FindIndex(const Reg para){  
  switch (para.GetType() ){
    case TYPE::CH: return chMap[para.GetPara()];
    case TYPE::DIG: return boardMap[para.GetPara()];
    case TYPE::VGA: return 0;
    case TYPE::LVDS: return -1;
  }
  return -1;
}

std::string Digitizer2Gen::ReadValue(const char * parameter, bool verbose){
  if( !isConnected ) return "not connected";
  //printf(" %s|%s \n", __func__, parameter);
  ret = CAEN_FELib_GetValue(handle, parameter, retValue);
  if (ret != CAEN_FELib_Success) {
    printf("%-45s | read fail\n", parameter);
    return ErrorMsg(__func__);
  }else{
    if( verbose ) printf("%-45s : %s\n", parameter, retValue);
  }
  return retValue;
}

std::string Digitizer2Gen::ReadValue(const Reg para, int ch_index,  bool verbose){
  std:: string ans = ReadValue(para.GetFullPara(ch_index).c_str(), verbose); 
  //printf("%s | %s \n", para.GetFullPara(ch_index).c_str(), ans.c_str());

  int index = FindIndex(para);
  switch( para.GetType()){
    case TYPE::CH  : chSettings[ch_index][index].SetValue(ans); break;
    case TYPE::DIG : boardSettings[index].SetValue(ans); break;
    case TYPE::VGA : VGASetting[ch_index].SetValue(ans); break;
    case TYPE::LVDS: return "LVDS not implemented.";
  }

  return ans;
}

bool Digitizer2Gen::WriteValue(const char * parameter, std::string value){
  if( !isConnected ) return false; 
  printf(" %s|%d|%-45s|%s|\n", __func__, serialNumber, parameter, value.c_str());
  ret = CAEN_FELib_SetValue(handle, parameter, value.c_str());
  if (ret != CAEN_FELib_Success) {
    printf("WriteError|%s||%s|\n", parameter, value.c_str());
    ErrorMsg(__func__);
    return false;
  }
  return true;
}

bool Digitizer2Gen::WriteValue(const Reg para, std::string value, int ch_index){
  if( WriteValue(para.GetFullPara(ch_index).c_str(), value) || isDummy){
    int index = FindIndex(para);
    if( index != -1 ){    
      switch(para.GetType()){
        case TYPE::CH :{
          if( ch_index >= 0 ){
            chSettings[ch_index][index].SetValue(value);
          }else{
            for( int ch = 0; ch < nChannels; ch++ ) chSettings[ch][index].SetValue(value);
          }

          //if( ch_index < 0 ) ch_index = 0;
          //printf("%s %s %s |%s|\n", __func__, para.GetPara().c_str(),
          //                     chSettings[ch_index][index].GetFullPara(ch_index).c_str(), 
          //                     chSettings[ch_index][index].GetValue().c_str());
        }break;
        case TYPE::VGA : {
          VGASetting[ch_index].SetValue(value); 

          //printf("%s %s %s |%s|\n", __func__,  para.GetPara().c_str(),
          //                      VGASetting[ch_index].GetFullPara(ch_index).c_str(), 
          //                      VGASetting[ch_index].GetValue().c_str());
        }break;
        case TYPE::DIG : {
          boardSettings[index].SetValue(value);
          //printf("%s %s %s |%s|\n", __func__,  para.GetPara().c_str(),
          //                     boardSettings[index].GetFullPara(ch_index).c_str(), 
          //                     boardSettings[index].GetValue().c_str()); 
        }break;
        case TYPE::LVDS : break;
      }
    }
    return true;
  }else{
    return false;
  }
}

void Digitizer2Gen::SendCommand(const char * parameter){
  if( !isConnected ) return;
  printf("Send Command : %s \n", parameter);
  ret = CAEN_FELib_SendCommand(handle, parameter);
  if (ret != CAEN_FELib_Success) {
    ErrorMsg(__func__);
    return;
  }
}

void Digitizer2Gen::SendCommand(std::string shortPara){
  std::string haha = "/cmd/" + shortPara;
  SendCommand(haha.c_str());
}

//########################################### Open digitizer
int Digitizer2Gen::OpenDigitizer(const char * url){
  
  printf("======== %s \n",__func__);

  ret = CAEN_FELib_Open(url, &handle);

  printf("===  ret : %d | %d \n", ret, CAEN_FELib_Success);
  
  if (ret != CAEN_FELib_Success) {
    ErrorMsg(__func__);
    return -1;
  }
  
  isConnected = true;

  printf("#################################################\n");
  ReadAllSettings();

  serialNumber = atoi(GetSettingValue(PHA::DIG::SerialNumber).c_str());
  FPGAType = GetSettingValue(PHA::DIG::FirmwareType);
  FPGAVer = atoi(GetSettingValue(PHA::DIG::CupVer).c_str());
  nChannels = atoi(GetSettingValue(PHA::DIG::NumberOfChannel).c_str());
  ModelName = GetSettingValue(PHA::DIG::ModelName);
  int adcRate = atoi(GetSettingValue(PHA::DIG::ADC_SampleRate).c_str());
  ch2ns = 1000/adcRate;
  
  printf("   IP address : %s\n", GetSettingValue(PHA::DIG::IPAddress).c_str());
  printf("     Net Mask : %s\n", GetSettingValue(PHA::DIG::NetMask).c_str());
  printf("      Gateway : %s\n", GetSettingValue(PHA::DIG::Gateway).c_str());
  
  printf("   Model name : %s\n", ModelName.c_str());
  printf("  CUP version : %s\n", GetSettingValue(PHA::DIG::CupVer).c_str());
  printf("     DPP Type : %s\n", GetSettingValue(PHA::DIG::FirmwareType).c_str());
  printf("Serial number : %d\n", serialNumber);
  printf("     ADC bits : %s\n", GetSettingValue(PHA::DIG::ADC_bit).c_str());
  printf("     ADC rate : %d Msps, ch2ns : %d ns\n", adcRate, ch2ns);
  printf("     Channels : %d\n", nChannels);

  //------ set default setting file name
  settingFileName = "settings_"+ std::to_string(serialNumber) + ".dat";

  printf("====================== \n");

  return 0;
}

int Digitizer2Gen::CloseDigitizer(){
  printf("========Digitizer2Gen::%s \n",__func__);
  if( isConnected == true ){
    ret = CAEN_FELib_Close(handle);
    if (ret != CAEN_FELib_Success) {
      ErrorMsg(__func__);
      return 0;
    }
    isConnected = false;
  }
  return 0;
}

//########################################### DAQ
void Digitizer2Gen::StartACQ(){
  
  SendCommand("/cmd/armacquisition"); // this will also clear data
  SendCommand("/cmd/swstartacquisition");
  
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

void Digitizer2Gen::SetPHADataFormat(unsigned short dataFormat){
  
  printf("%s : %d\n", __func__, dataFormat);

  ///========== get endpoint and endpoint folder handle
  if( dataFormat < 15 ){

    ret  = CAEN_FELib_GetHandle(handle, "/endpoint/dpppha", &ep_handle);
    ret |= CAEN_FELib_GetParentHandle(ep_handle, NULL, &ep_folder_handle);
    ret |= CAEN_FELib_SetValue(ep_folder_handle, "/par/activeendpoint", "dpppha");

    if (ret != CAEN_FELib_Success) {
      ErrorMsg("Set active endpoint");
      return;
    }

  }else{
    ret  = CAEN_FELib_GetHandle(handle, "/endpoint/raw", &ep_handle);
    ret |= CAEN_FELib_GetParentHandle(ep_handle, NULL, &ep_folder_handle);
    ret |= CAEN_FELib_SetValue(ep_folder_handle, "/par/activeendpoint", "raw");
    
    if (ret != CAEN_FELib_Success) {
      ErrorMsg("Set active endpoint");
      return;
    }
  }

  if( evt ) delete evt;
  evt = new Event();
  evt->SetDataType(dataFormat);
  dataStartIndetifier += dataFormat;

  if( dataFormat == 0 ){  
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

  if( dataFormat == 1 ){
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

  if( dataFormat == 2 ){
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

  if( dataFormat == 3 ){
    ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
    "[ \
      { \"name\" : \"CHANNEL\",   \"type\" : \"U8\" }, \
      { \"name\" : \"TIMESTAMP\", \"type\" : \"U64\" }, \
      { \"name\" : \"ENERGY\",    \"type\" : \"U16\" } \
    ]");
  }

  if( dataFormat == 15 ){
    ret = CAEN_FELib_SetReadDataFormat(ep_handle, 
      " [ \
         { \"name\": \"DATA\",     \"type\": \"U8\", \"dim\": 1 }, \
         { \"name\": \"SIZE\",     \"type\": \"SIZE_T\" }, \
         { \"name\": \"N_EVENTS\", \"type\": \"U32\" } \
      ]"
    );
  }

  if (ret != CAEN_FELib_Success) {
    ErrorMsg("Set Read Data Format");
    return;
  }

  //TODO Statistic handle and endpoint
  ret  = CAEN_FELib_GetHandle(handle, "/endpoint/dpppha/stats", &stat_handle);
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
    ErrorMsg("Set Statistics");
    return;
  }

}

int Digitizer2Gen::ReadStat(){

  ret = CAEN_FELib_ReadData(stat_handle, 100,
        realTime, 
        deadTime, 
        liveTime,
        triggerCount,
        savedEventCount
      );

  if (ret != CAEN_FELib_Success) ErrorMsg("Read Statistics");

  return ret;
}

void Digitizer2Gen::PrintStat(){
  printf("ch | Real Time[ns] | Dead Time[ns] | Live Time[ns] | Trigger |  Saved  | Rate[Hz] \n");
  for( int i = 0; i < MaxNumberOfChannel; i++){
    if( triggerCount[i] == 0 ) continue;
    printf("%02d | %13lu | %13lu | %13lu | %7u | %7u | %.3f\n", 
         i, realTime[i], deadTime[i], liveTime[i], triggerCount[i], savedEventCount[i], triggerCount[i]*1e9*1.0/realTime[i]);
  }
}

int Digitizer2Gen::ReadData(){
  //printf("========= %s \n", __func__);

  if( evt->dataType == 0){
    ret = CAEN_FELib_ReadData(ep_handle, 100,
      &evt->channel,
      &evt->timestamp,
      &evt->fine_timestamp,
      &evt->energy,
      evt->analog_probes[0],
      evt->analog_probes[1],
      evt->digital_probes[0],
      evt->digital_probes[1],
      evt->digital_probes[2],
      evt->digital_probes[3],
      &evt->analog_probes_type[0],
      &evt->analog_probes_type[1],
      &evt->digital_probes_type[0],
      &evt->digital_probes_type[1],
      &evt->digital_probes_type[2],
      &evt->digital_probes_type[3],
      &evt->traceLenght,
      &evt->flags_low_priority,
      &evt->flags_high_priority,
      &evt->trigger_threashold,
      &evt->downSampling,
      &evt->board_fail,
      &evt->flush,
      &evt->aggCounter,
      &evt->event_size
    );
  }else if( evt->dataType == 1){
    ret = CAEN_FELib_ReadData(ep_handle, 100,
      &evt->channel,
      &evt->timestamp,
      &evt->fine_timestamp,
      &evt->energy,
      evt->analog_probes[0],
      &evt->analog_probes_type[0],
      &evt->traceLenght,
      &evt->flags_low_priority,
      &evt->flags_high_priority,
      &evt->trigger_threashold,
      &evt->downSampling,
      &evt->board_fail,
      &evt->flush,
      &evt->aggCounter,
      &evt->event_size
    );
  }else if( evt->dataType == 2){
    ret = CAEN_FELib_ReadData(ep_handle, 100,
      &evt->channel,
      &evt->timestamp,
      &evt->fine_timestamp,
      &evt->energy,
      &evt->flags_low_priority,
      &evt->flags_high_priority,
      &evt->trigger_threashold,
      &evt->downSampling,
      &evt->board_fail,
      &evt->flush,
      &evt->aggCounter,
      &evt->event_size
    );
  }else if( evt->dataType == 3){
    ret = CAEN_FELib_ReadData(ep_handle, 100,
      &evt->channel,
      &evt->timestamp,
      &evt->energy
    );
  }else if( evt->dataType == 15){
    ret = CAEN_FELib_ReadData(ep_handle, 100, evt->data, &evt->dataSize, &evt->n_events );
    //printf("data size: %lu byte\n", evt.dataSize);
  }else{
    return CAEN_FELib_UNKNOWN;
  }

  evt->isTraceAllZero = false;

  if( ret != CAEN_FELib_Success) {
    //ErrorMsg("ReadData()");
    return ret;
  }

  return ret;
}

//###########################################

void Digitizer2Gen::OpenOutFile(std::string fileName, const char * mode){
  outFileNameBase = fileName;
  sprintf(outFileName, "%s_%03d.sol", fileName.c_str(), outFileIndex);
  outFile = fopen(outFileName, mode);
  fseek(outFile, 0L, SEEK_END);
  outFileSize = ftell(outFile);  // unsigned int =  Max ~4GB

}

void Digitizer2Gen::CloseOutFile(){
  if( outFile != NULL ) {
    fclose(outFile);
    int result = chmod(outFileName, S_IRUSR | S_IRGRP | S_IROTH);
    if( result != 0 ) printf("somewrong when set file (%s) to read only.", outFileName);
  }
}

void Digitizer2Gen::SaveDataToFile(){

  if( outFileSize > (unsigned int) MaxOutFileSize){
    FinishedOutFilesSize += ftell(outFile);
    CloseOutFile();
    outFileIndex ++;
    sprintf(outFileName, "%s_%03d.sol", outFileNameBase.c_str(), outFileIndex);
    outFile = fopen(outFileName, "a+b");
  }

  if( evt->dataType == 0){
    fwrite(&dataStartIndetifier, 2, 1, outFile);
    fwrite(&evt->channel, 1, 1, outFile);
    fwrite(&evt->energy, 2, 1, outFile);
    fwrite(&evt->timestamp, 6, 1, outFile);
    fwrite(&evt->fine_timestamp, 2, 1, outFile);
    fwrite(&evt->flags_high_priority, 1, 1, outFile);
    fwrite(&evt->flags_low_priority, 2, 1, outFile);
    fwrite(&evt->downSampling, 1, 1, outFile);
    fwrite(&evt->board_fail, 1, 1, outFile);
    fwrite(&evt->flush, 1, 1, outFile);
    fwrite(&evt->trigger_threashold, 2, 1, outFile);
    fwrite(&evt->event_size, 8, 1, outFile);
    fwrite(&evt->aggCounter, 4, 1, outFile);
    fwrite(&evt->traceLenght, 8, 1, outFile);
    fwrite(evt->analog_probes_type, 2, 1, outFile);
    fwrite(evt->digital_probes_type, 4, 1, outFile);
    fwrite(evt->analog_probes[0], evt->traceLenght*4, 1, outFile);
    fwrite(evt->analog_probes[1], evt->traceLenght*4, 1, outFile);
    fwrite(evt->digital_probes[0], evt->traceLenght, 1, outFile);
    fwrite(evt->digital_probes[1], evt->traceLenght, 1, outFile);
    fwrite(evt->digital_probes[2], evt->traceLenght, 1, outFile);
    fwrite(evt->digital_probes[3], evt->traceLenght, 1, outFile);
  }else if( evt->dataType == 1){
    fwrite(&dataStartIndetifier, 2, 1, outFile);
    fwrite(&evt->channel, 1, 1, outFile);
    fwrite(&evt->energy, 2, 1, outFile);
    fwrite(&evt->timestamp, 6, 1, outFile);
    fwrite(&evt->fine_timestamp, 2, 1, outFile);
    fwrite(&evt->flags_high_priority, 1, 1, outFile);
    fwrite(&evt->flags_low_priority, 2, 1, outFile);
    fwrite(&evt->traceLenght, 8, 1, outFile);
    fwrite(&evt->analog_probes_type[0], 1, 1, outFile);
    fwrite(evt->analog_probes[0], evt->traceLenght*4, 1, outFile);
  }else if( evt->dataType == 2){
    fwrite(&dataStartIndetifier, 2, 1, outFile);
    fwrite(&evt->channel, 1, 1, outFile);
    fwrite(&evt->energy, 2, 1, outFile);
    fwrite(&evt->timestamp, 6, 1, outFile);
    fwrite(&evt->fine_timestamp, 2, 1, outFile);
    fwrite(&evt->flags_high_priority, 1, 1, outFile);
    fwrite(&evt->flags_low_priority, 2, 1, outFile);
  }else if( evt->dataType == 3){
    fwrite(&dataStartIndetifier, 2, 1, outFile);
    fwrite(&evt->channel, 1, 1, outFile);
    fwrite(&evt->energy, 2, 1, outFile);
    fwrite(&evt->timestamp, 6, 1, outFile);
  }else if( evt->dataType == 15){
    fwrite(&dataStartIndetifier, 2, 1, outFile);
    fwrite(&evt->dataSize, 8, 1, outFile);
    fwrite(evt->data, evt->dataSize, 1, outFile);
  }
  
  outFileSize = ftell(outFile);  // unsigned int =  Max ~4GB

}


//###########################################
void Digitizer2Gen::Reset(){ SendCommand("/cmd/Reset"); }

void Digitizer2Gen::ProgramPHA(bool testPulse){
  if( !isConnected ) return ;

  // Acquistion
  WriteValue("/par/StartSource"  , "SWcmd | SINedge");
  WriteValue("/par/TrgOutMode", "Disabled");
  WriteValue("/par/GPIOMode",   "Disabled");
  WriteValue("/par/SyncOutMode", "Disabled");  
  WriteValue("/par/RunDelay", "0"); // ns, that is for sync time with multi board
  WriteValue("/par/IOlevel", "NIM"); 
  WriteValue("/par/EnStatEvents", "true");
  
  // Channel setting  
  if( testPulse){
    WriteValue("/ch/0..63/par/ChEnable"   , "false");
    WriteValue("/ch/0/par/ChEnable"   , "true");
    WriteValue("/ch/1/par/ChEnable"   , "true");
    WriteValue("/ch/2/par/ChEnable"   , "true");
    WriteValue("/ch/3/par/ChEnable"   , "true");
    
    //WriteValue("/ch/0..63/par/ChEnable"   , "true");

    WriteValue("/ch/0..63/par/EventTriggerSource", "GlobalTriggerSource");
    WriteValue("/ch/0..63/par/WaveTriggerSource" , "GlobalTriggerSource"); // EventTriggerSource enought

    WriteValue("/par/GlobalTriggerSource", "SwTrg | TestPulse");
    WriteValue("/par/TestPulsePeriod"    , "1000000"); // 1.0 msec = 1000Hz, tested, 1 trace recording
    WriteValue("/par/TestPulseWidth"     , "1000"); // nsec
    WriteValue("/par/TestPulseLowLevel"     , "0");
    WriteValue("/par/TestPulseHighLevel"     , "10000");

  }else{
    //======= this is for manual send trigger signal via software
    //WriteValue("/ch/0..63/par/EventTriggerSource", "SwTrg");
    //WriteValue("/ch/0..63/par/WaveTriggerSource" , "SwTrg"); 
    
    
    //======== Self trigger for each channel 
    WriteValue("/ch/0..63/par/EventTriggerSource", "ChSelfTrigger");
    WriteValue("/ch/0..63/par/WaveTriggerSource" , "ChSelfTrigger"); 

    //======== One (or more) slef-trigger can trigger whole board, ??? depend on Channel Trigger mask
    //WriteValue("/ch/0..63/par/EventTriggerSource", "Ch64Trigger");
    //WriteValue("/ch/0..63/par/WaveTriggerSource" , "Ch64Trigger"); 

    //WriteValue("/ch/0..63/par/ChannelsTriggerMask", "0x0000FFFF000F000F");

    //WriteValue("/ch/0..3/par/ChannelsTriggerMask", "0x1");
    //WriteValue("/ch/4..7/par/ChannelsTriggerMask", "0x10");

    //WriteValue("/ch/0/par/ChannelsTriggerMask", "0x000F");
    //WriteValue("/ch/12/par/ChannelsTriggerMask", "0x000F");
    //WriteValue("/ch/38/par/ChannelsTriggerMask", "0x000F"); // when channel has no input, it still record.

    //----------- coincident trigger to ch-4n
    //WriteValue("/ch/0..63/par/EventTriggerSource", "ChSelfTrigger");
    //WriteValue("/ch/0..63/par/WaveTriggerSource" , "ChSelfTrigger"); 

    //for(int i = 0 ; i < 16; i++){
    //  WriteValue(("/ch/"+ std::to_string(4*i+1) + ".." + std::to_string(4*i+3) + "/par/ChannelsTriggerMask").c_str(), "0x1");
    //  WriteValue(("/ch/"+ std::to_string(4*i+1) + ".." + std::to_string(4*i+3) + "/par/CoincidenceMask").c_str(), "Ch64Trigger");
    //  WriteValue(("/ch/"+ std::to_string(4*i+1) + ".." + std::to_string(4*i+3) + "/par/CoincidenceLengthT").c_str(), "100"); // ns
    //}
    //======== ACQ trigger?
    //WriteValue("/ch/0..63/par/EventTriggerSource", "GlobalTriggerSource");
    //WriteValue("/ch/0..63/par/WaveTriggerSource" , "GlobalTriggerSource"); 
   
    //WriteValue("/par/GlobalTriggerSource", "SwTrg");

    
    WriteValue("/ch/0..63/par/ChEnable"   , "true");
    //WriteValue("/ch/0..15/par/ChEnable"   , "true");
  }
  
  WriteValue("/ch/0..63/par/DCOffset"   , "10");  /// 10% 
  WriteValue("/ch/0..63/par/WaveSaving" , "Always");
  
  WriteValue("/ch/0..63/par/ChRecordLengthT"   , "4096");  /// 4096 ns, S and T are not Sync
  WriteValue("/ch/0..63/par/ChPreTriggerT"     , "1000");  /// 1000 ns
  WriteValue("/ch/0..63/par/WaveResolution"    , "RES8");  /// 8 ns 
  
  WriteValue("/ch/0..63/par/WaveAnalogProbe0"  , "ADCInput");
  WriteValue("/ch/0..63/par/WaveAnalogProbe1"  , "EnergyFilterMinusBaseline");
  WriteValue("/ch/0..63/par/WaveDigitalProbe0" , "Trigger");
  WriteValue("/ch/0..63/par/WaveDigitalProbe1" , "EnergyFilterPeaking");
  WriteValue("/ch/0..63/par/WaveDigitalProbe2" , "TimeFilterArmed");
  WriteValue("/ch/0..63/par/WaveDigitalProbe3" , "EnergyFilterPeakReady");

  // Filter parameters
  WriteValue("/ch/0..63/par/TimeFilterRiseTimeS"         , "10");   // 80 ns
  WriteValue("/ch/0..63/par/TriggerThr"                  , "1000"); 
  WriteValue("/ch/0..63/par/PulsePolarity"               , "Positive");
  WriteValue("/ch/0..63/par/EnergyFilterBaselineAvg"     , "Medium"); // 1024 sample
  WriteValue("/ch/0..63/par/EnergyFilterFineGain"        , "1.0");

  WriteValue("/ch/0..63/par/EnergyFilterRiseTimeS"       , "62");   //  496 ns
  WriteValue("/ch/0..63/par/EnergyFilterFlatTopS"        , "200");  // 1600 ns
  WriteValue("/ch/0..63/par/EnergyFilterPoleZeroS"       , "6250"); // 50 us

  WriteValue("/ch/0..63/par/EnergyFilterPeakingPosition" , "20");   // 20 % = Flatup * 20% = 320 ns

  WriteValue("/ch/0..63/par/TimeFilterRetriggerGuardS"   , "10");   // 80 ns
  WriteValue("/ch/0..63/par/EnergyFilterPileupGuardS"    , "10");   // 80 ns
  WriteValue("/ch/0..63/par/EnergyFilterBaselineGuardS"  , "100");  // 800 ns

  WriteValue("/ch/0..63/par/EnergyFilterLFLimitation"    , "Off");
  
}

std::string Digitizer2Gen::ErrorMsg(const char * funcName){
  printf("======== %s | %5d | %s\n",__func__, serialNumber, funcName);
  char msg[1024];
  int ec = CAEN_FELib_GetErrorDescription((CAEN_FELib_ErrorCode) ret, msg);
  if (ec != CAEN_FELib_Success) {
    std::string errMsg = __func__;
    errMsg += " failed";
    printf("%s failed\n", __func__);
    return errMsg;
  }
  printf("Error msg (%d): %s\n", ret, msg);
  return msg;
}

//^===================================================== Settings
void Digitizer2Gen::ReadAllSettings(){
  if( !isConnected ) return;
  for(int i = 0; i < (int) boardSettings.size(); i++){
    if( boardSettings[i].ReadWrite() == RW::WriteOnly) continue;
    ReadValue(boardSettings[i]);
  }

  for(int i = 0; i < 4 ; i ++) ReadValue(VGASetting[i], i);

  for(int ch = 0; ch < nChannels ; ch++ ){
    for( int i = 0; i < (int) chSettings[ch].size(); i++){
      if( chSettings[ch][i].ReadWrite() == RW::WriteOnly) continue;
      ReadValue(chSettings[ch][i], ch);
    }
  }
}

int Digitizer2Gen::SaveSettingsToFile(const char * saveFileName, bool setReadOnly){
  if( saveFileName != NULL) settingFileName = saveFileName;

  int totCount = 0;
  int count = 0;
  FILE * saveFile = fopen(settingFileName.c_str(), "w");
  if( saveFile ){
    for(int i = 0; i < (int) boardSettings.size(); i++){
      if( boardSettings[i].ReadWrite() == RW::WriteOnly) continue;
      totCount ++;
      if( boardSettings[i].GetValue() == "" && boardSettings[i].GetPara() != "Gateway") break;
      fprintf(saveFile, "%-45s|%d|%4d|%s\n", boardSettings[i].GetFullPara().c_str(),  
                                             boardSettings[i].ReadWrite(),
                                             8000 + i, 
                                             boardSettings[i].GetValue().c_str());
      count ++;
    }

    for(int i = 0; i < 4 ; i ++){
      totCount ++;
      if( VGASetting[i].GetValue() == "" ) break;
      fprintf(saveFile, "%-45s|%d|%4d|%s\n", VGASetting[i].GetFullPara(i).c_str(), 
                                             VGASetting[i].ReadWrite(), 
                                             9000 + i,
                                             VGASetting[i].GetValue().c_str());
      count ++;
    }
    for(int ch = 0; ch < nChannels ; ch++ ){
      for( int i = 0; i < (int) chSettings[ch].size(); i++){
        if( chSettings[ch][i].ReadWrite() == RW::WriteOnly) continue;
        totCount ++;
        if( chSettings[ch][i].GetValue() == "") break;
        fprintf(saveFile, "%-45s|%d|%4d|%s\n", chSettings[ch][i].GetFullPara(ch).c_str(), 
                                               chSettings[ch][i].ReadWrite(),
                                               ch*100 + i,
                                               chSettings[ch][i].GetValue().c_str());
        count ++;
      }
    }    
    fclose(saveFile);

    if( count != totCount ) {
      remove(saveFileName);
      return -1;
    }

    if( setReadOnly ){
      int result = chmod(saveFileName, S_IRUSR | S_IRGRP | S_IROTH);
      if( result != 0 ) printf("somewrong when set file (%s) to read only.", saveFileName);
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
    char * para      = new char[100];
    char * readWrite = new char[100];
    char * idStr     = new char[100];
    char * value     = new char[100];

    char line[100];
    while(fgets(line, sizeof(line), loadFile) != NULL){

      //printf("%s", line);
      char* token = std::strtok(line, "|");
      int count = 0;
      while( token != nullptr){

        char * end = std::remove_if(token, token + std::strlen(token), [](char c) {
          return std::isspace(c);
        });
        *end = '\0';

        size_t len = std::strcspn(token, "\n");
        if( len > 0 ) token[len] = '\0';

        if( count == 0 ) std::strcpy(para, token);
        if( count == 1 ) std::strcpy(readWrite, token);
        if( count == 2 ) std::strcpy(idStr, token);
        if( count == 3 ) std::strcpy(value, token);
        if( count > 3) break;
        
        count ++;
        token = std::strtok(nullptr, "|");
      }

      int id = atoi(idStr);
      if( id < 8000){ // channel
        int ch = id / 100;
        int index = id - ch * 100;
        chSettings[ch][index].SetValue(value);
        //printf("-------id : %d, ch: %d, index : %d\n", id,  ch, index);
        //printf("%s|%d|%d|%s|\n", chSettings[ch][index].GetFullPara(ch).c_str(), 
        //                         chSettings[ch][index].ReadWrite(), id, 
        //                         chSettings[ch][index].GetValue().c_str());

      }else if ( 8000 <= id && id < 9000){ // board
        boardSettings[id - 8000].SetValue(value);
        //printf("%s|%d|%d|%s\n", boardSettings[id-8000].GetFullPara().c_str(),
        //                        boardSettings[id-8000].ReadWrite(), id,
        //                        boardSettings[id-8000].GetValue().c_str());
      }else{ // vga
        VGASetting[id - 9000].SetValue(value);
      }
      //printf("%s|%s|%d|%s|\n", para, readWrite, id, value);
      if( std::strcmp(readWrite, "2") == 0 && isConnected)  WriteValue(para, value);
    }

    delete [] para;
    delete [] readWrite;
    delete [] idStr;
    delete [] value;

    return true;
  }else{
    //printf("Fail to load file %s\n", loadFileName);
  }

  return false;
  
}

std::string Digitizer2Gen::GetSettingValue(const Reg para, unsigned int ch_index) {
  int index = FindIndex(para);
  switch (para.GetType()){
    case TYPE::DIG:  return boardSettings[index].GetValue();
    case TYPE::CH:   return chSettings[ch_index][index].GetValue();
    case TYPE::VGA:  return VGASetting[ch_index].GetValue();
    case TYPE::LVDS: return "not defined";
    default : return "invalid";
  }
  return "no such parameter";
}
