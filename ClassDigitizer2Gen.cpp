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
}

void Digitizer2Gen::Initialization(){
  //printf("======== %s \n",__func__);

  handle = 0;
  ret = 0;
  isConnected = false;
  isDummy = false;

  serialNumber = 0;
  FPGAType = "";
  nChannels = 0;
  tick2ns = 0;
  CupVer = 0;

  outFileIndex = 0;
  FinishedOutFilesSize = 0;
  dataStartIndetifier = 0xAAA0;
  outFile = NULL;
  outFileSize = 0;

  hit = NULL;

  acqON = false;

  settingFileName = "";

  boardSettings = PHA::DIG::AllSettings;
  for( int ch = 0; ch < MaxNumberOfChannel ; ch ++) chSettings[ch] = PHA::CH::AllSettings;
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
    case TYPE::LVDS: return LVDSMap[para.GetPara()];
    case TYPE::GROUP : return 0;
  }
  return -1;
}

std::string Digitizer2Gen::ReadValue(const char * parameter, bool verbose){
  if( !isConnected ) return "not connected";
  //printf(" %s|%s \n", __func__, parameter);
  ret = CAEN_FELib_GetValue(handle, parameter, retValue);
  if (ret != CAEN_FELib_Success) {
    printf("  %s|%d|%-45s| read fail\n", __func__, serialNumber, parameter);
    return ErrorMsg(__func__);
  }else{
    if( verbose ) printf("  %s|%d|%-45s:%s\n", __func__, serialNumber, parameter, retValue);
  }
  return retValue;
}

std::string Digitizer2Gen::ReadValue(const Reg para, int ch_index,  bool verbose){
  std:: string ans = ReadValue(para.GetFullPara(ch_index).c_str(), verbose); 

  int index = FindIndex(para);
  switch( para.GetType()){
    case TYPE::CH  : chSettings[ch_index][index].SetValue(ans); break;
    case TYPE::DIG : boardSettings[index].SetValue(ans); break;
    case TYPE::VGA : VGASetting[ch_index].SetValue(ans); break;
    case TYPE::LVDS: LVDSSettings[ch_index][index].SetValue(ans);break;
    case TYPE::GROUP: InputDelay[ch_index].SetValue(ans); break; 
  }
  
  //printf("%s | %s | index %d | %s \n", para.GetFullPara(ch_index).c_str(), ans.c_str(), index, chSettings[ch_index][index].GetValue().c_str());

  return ans;
}

bool Digitizer2Gen::WriteValue(const char * parameter, std::string value, bool verbose){
  if( !isConnected ) return false; 
  //ReadValue(parameter, 1);
  if( verbose) printf(" %s|%d|%-45s|%s|\n", __func__, serialNumber, parameter, value.c_str());
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
        
        case TYPE::LVDS : {
          LVDSSettings[ch_index][index].SetValue(value); 
        }break;
        
        case TYPE::GROUP : {
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
  
  //printf("======== %s \n",__func__);

  ret = CAEN_FELib_Open(url, &handle);

  //printf("===  ret : %d | %d \n", ret, CAEN_FELib_Success);
  
  if (ret != CAEN_FELib_Success) {
    ErrorMsg(__func__);
    return -1;
  }
  
  isConnected = true;

  printf("#################################################\n");

  //========== PHA and PSD are the same
  serialNumber = atoi(ReadValue(PHA::DIG::SerialNumber).c_str());
  FPGAType = ReadValue(PHA::DIG::FirmwareType);
  FPGAVer = atoi(ReadValue(PHA::DIG::CupVer).c_str());
  nChannels = atoi(ReadValue(PHA::DIG::NumberOfChannel).c_str());
  ModelName = ReadValue(PHA::DIG::ModelName);
  CupVer = atoi(ReadValue(PHA::DIG::CupVer).c_str());
  int adcRate = atoi(ReadValue(PHA::DIG::ADC_SampleRate).c_str());
  tick2ns = 1000/adcRate;
  
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
    for( int ch = 0; ch < nChannels ; ch ++) chSettings[ch] = PHA::CH::AllSettings;
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
    for( int ch = 0; ch < nChannels ; ch ++) chSettings[ch] = PSD::CH::AllSettings;
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

  ReadAllSettings();
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

void Digitizer2Gen::SetDataFormat(unsigned short dataFormat){
  
  printf("%s : %d for digi-%d %s\n", __func__, dataFormat, serialNumber, FPGAType.c_str() );

  ///========== get endpoint and endpoint folder handle
  if( dataFormat == DataFormat::Raw ){

    ret  = CAEN_FELib_GetHandle(handle, "/endpoint/raw", &ep_handle);
    ret |= CAEN_FELib_GetParentHandle(ep_handle, NULL, &ep_folder_handle);
    ret |= CAEN_FELib_SetValue(ep_folder_handle, "/par/activeendpoint", "raw");
    
    if (ret != CAEN_FELib_Success) {
      ErrorMsg("Set active endpoint");
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
      ErrorMsg("DPP-Type not supported.");
      return;
    }

    if (ret != CAEN_FELib_Success) {
      ErrorMsg("Set active endpoint");
      return;
    }
  }

  if( hit ) delete hit;
  hit = new Hit();
  hit->SetDataType(dataFormat, FPGAType);
  dataStartIndetifier = 0xAA00 + dataFormat;
  if(FPGAType == DPPType::PSD ) dataStartIndetifier += 0x0010;

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
    ErrorMsg("Set Read Data Format");
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

  for( int ch = 0; ch < nChannels; ch++) ReadValue( PHA::CH::SelfTrgRate, ch);

  return ret;
}

void Digitizer2Gen::PrintStat(){
  printf("ch | Real Time[ns] | Dead Time[ns] | Live Time[ns] | Trigger |  Saved  | Rate[Hz] | Self Trig Rate [Hz] \n");
  for( int i = 0; i < nChannels; i++){
    //if( triggerCount[i] == 0 ) continue;
    if( atoi(chSettings[i][0].GetValue().c_str()) == 0 ) continue;
    printf("%02d | %13lu | %13lu | %13lu | %7u | %7u | %8.3f | %d\n", 
         i, realTime[i], deadTime[i], liveTime[i], triggerCount[i], savedEventCount[i], triggerCount[i]*1e9*1.0/realTime[i], atoi(chSettings[i][0].GetValue().c_str()));
  }
}

int Digitizer2Gen::ReadData(){
  //printf("Digitizer2Gen::%s, DPP : %s, dataFormat : %d \n", __func__, FPGAType.c_str(), hit->dataType);

  if( FPGAType != DPPType::PHA && FPGAType != DPPType::PSD ) return -404;

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
    ret = CAEN_FELib_ReadData(ep_handle, 100, hit->data, &hit->dataSize, &hit->n_events );
    //printf("data size: %lu byte\n", evt.dataSize);

    hit->isTraceAllZero = true; //assume no trace, as the trace need to be extracted.
  }else{
    return CAEN_FELib_UNKNOWN;
  }

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
    outFile = fopen(outFileName, "wb"); //overwrite binary
  }

  if( hit->dataType == DataFormat::ALL){
    fwrite(&dataStartIndetifier,      2, 1, outFile);
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
    fwrite(&dataStartIndetifier,        2, 1, outFile);
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
    fwrite(&dataStartIndetifier,      2, 1, outFile);
    fwrite(&hit->channel,             1, 1, outFile);
    fwrite(&hit->energy,              2, 1, outFile);
    if( FPGAType == DPPType::PSD ) fwrite(&hit->energy_short, 2, 1, outFile);
    fwrite(&hit->timestamp,           6, 1, outFile);
    fwrite(&hit->fine_timestamp,      2, 1, outFile);
    fwrite(&hit->flags_high_priority, 1, 1, outFile);
    fwrite(&hit->flags_low_priority,  2, 1, outFile);

  }else if( hit->dataType == DataFormat::MiniWithFineTime ){
    fwrite(&dataStartIndetifier, 2, 1, outFile);
    fwrite(&hit->channel,        1, 1, outFile);
    fwrite(&hit->energy,         2, 1, outFile);
    if( FPGAType == DPPType::PSD ) fwrite(&hit->energy_short, 2, 1, outFile);
    fwrite(&hit->timestamp,      6, 1, outFile);
    fwrite(&hit->fine_timestamp, 2, 1, outFile);

  }else if( hit->dataType == DataFormat::Minimum ){
    fwrite(&dataStartIndetifier, 2, 1, outFile);
    fwrite(&hit->channel,        1, 1, outFile);
    fwrite(&hit->energy,         2, 1, outFile);
    if( FPGAType == DPPType::PSD ) fwrite(&hit->energy_short, 2, 1, outFile);
    fwrite(&hit->timestamp,      6, 1, outFile);

  }else if( hit->dataType == DataFormat::Raw){
    fwrite(&dataStartIndetifier,  2, 1, outFile);
    fwrite(&hit->dataSize,        8, 1, outFile);
    fwrite(hit->data, hit->dataSize, 1, outFile);
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

  WriteValue("/par/EnAutoDisarmAcq" , "true");
  if( FPGAType == DPPType::PHA ) WriteValue("/par/EnStatEvents"    , "true");
  if( FPGAType == DPPType::PSD ) WriteValue("/par/EnStatEvents"    , "false");
  WriteValue("/par/EnAutoDisarmAcq"    , "False");
  
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


  // Channel setting  
  if( testPulse){
    WriteValue("/ch/0..63/par/ChEnable"   , "false");
    WriteValue("/ch/0..63/par/ChEnable"   , "true");

    WriteValue("/ch/0..63/par/EventTriggerSource", "GlobalTriggerSource");
    WriteValue("/ch/0..63/par/WaveTriggerSource" , "GlobalTriggerSource"); // EventTriggerSource enought

    WriteValue("/par/GlobalTriggerSource", "SwTrg | TestPulse");
    WriteValue("/par/TestPulsePeriod"    , "1000000"); // 1.0 msec = 1000Hz, tested, 1 trace recording
    WriteValue("/par/TestPulseWidth"     , "1000"); // nsec
    WriteValue("/par/TestPulseLowLevel"  , "0");
    WriteValue("/par/TestPulseHighLevel" , "10000");

  }else{

    //======== Self trigger for each channel 
    WriteValue("/ch/0..63/par/ChEnable"                     ,  "True");
    WriteValue("/ch/0..63/par/DCOffset"                     ,  "50");
    WriteValue("/ch/0..63/par/TriggerThr"                   ,  "1000");
    WriteValue("/ch/0..63/par/WaveDataSource"               ,  "ADC_DATA");
    WriteValue("/ch/0..63/par/PulsePolarity"                ,  "Positive");
    WriteValue("/ch/0..63/par/ChRecordLengthT"              ,  "4096");      /// 4096 ns, S and T are not Sync
    WriteValue("/ch/0..63/par/ChPreTriggerT"                ,  "1000");

    WriteValue("/ch/0..63/par/WaveSaving"                   ,  "OnRequest");
    WriteValue("/ch/0..63/par/WaveResolution"               ,  "RES8");

    //======== Trigger setting
    WriteValue("/ch/0..63/par/EventTriggerSource"  , "ChSelfTrigger");
    WriteValue("/ch/0..63/par/WaveTriggerSource"   , "Disabled"); 
    WriteValue("/ch/0..63/par/ChannelVetoSource"   , "Disabled"); 
    WriteValue("/ch/0..63/par/ChannelsTriggerMask" , "0x0");
    WriteValue("/ch/0..63/par/CoincidenceMask"     , "Disabled");
    WriteValue("/ch/0..63/par/AntiCoincidenceMask" , "Disabled");
    WriteValue("/ch/0..63/par/CoincidenceLengthT"  , "0");
    WriteValue("/ch/0..63/par/ADCVetoWidth"        , "0");

    //======== Other Setting
    WriteValue("/ch/0..63/par/EventSelector"               , "All");
    WriteValue("/ch/0..63/par/WaveSelector"                , "All");
    WriteValue("/ch/0..63/par/EnergySkimLowDiscriminator"  , "0");
    WriteValue("/ch/0..63/par/EnergySkimHighDiscriminator" , "65534");
    WriteValue("/ch/0..63/par/ITLConnect"                  , "Disabled");

    if( FPGAType == DPPType::PHA){

      WriteValue("/ch/0..63/par/TimeFilterRiseTimeT"       , "80");   // 80 ns
      WriteValue("/ch/0..63/par/TimeFilterRetriggerGuardT" , "80");   // 80 ns

      WriteValue("/ch/0..63/par/EnergyFilterLFLimitation" , "Off");
  
      //======== Trapezoid setting
      WriteValue("/ch/0..63/par/EnergyFilterRiseTimeT"       , "496");   //  496 ns
      WriteValue("/ch/0..63/par/EnergyFilterFlatTopT"        , "1600");  // 1600 ns
      WriteValue("/ch/0..63/par/EnergyFilterPoleZeroT"       , "50000"); // 50 us
      WriteValue("/ch/0..63/par/EnergyFilterPeakingPosition" , "20");   // 20 % = Flatup * 20% = 320 ns
      WriteValue("/ch/0..63/par/EnergyFilterBaselineGuardT"   , "800");  // 800 ns
      WriteValue("/ch/0..63/par/EnergyFilterPileupGuardT"     , "80");   // 80 ns
      WriteValue("/ch/0..63/par/EnergyFilterBaselineAvg"     , "Medium"); // 1024 sample
      WriteValue("/ch/0..63/par/EnergyFilterFineGain"        , "1.0");
      WriteValue("/ch/0..63/par/EnergyFilterPeakingAvg"      , "LowAVG");

      //======== Probe Setting
      WriteValue("/ch/0..63/par/WaveAnalogProbe0"  , "ADCInput");
      WriteValue("/ch/0..63/par/WaveAnalogProbe1"  , "EnergyFilterMinusBaseline");
      WriteValue("/ch/0..63/par/WaveDigitalProbe0" , "Trigger");
      WriteValue("/ch/0..63/par/WaveDigitalProbe1" , "EnergyFilterPeaking");
      WriteValue("/ch/0..63/par/WaveDigitalProbe2" , "TimeFilterArmed");
      WriteValue("/ch/0..63/par/WaveDigitalProbe3" , "EnergyFilterPeakReady");

    }

    if( FPGAType == DPPType::PSD ){

      WriteValue("/ch/0..63/par/WaveAnalogProbe0"             ,  "ADCInput");
      WriteValue("/ch/0..63/par/WaveAnalogProbe1"             ,  "CFDFilter");
      WriteValue("/ch/0..63/par/WaveDigitalProbe0"            ,  "Trigger");
      WriteValue("/ch/0..63/par/WaveDigitalProbe1"            ,  "LongGate");
      WriteValue("/ch/0..63/par/WaveDigitalProbe2"            ,  "ShortGate");
      WriteValue("/ch/0..63/par/WaveDigitalProbe3"            ,  "ChargeReady");

      //=========== QDC
      WriteValue("/ch/0..63/par/GateLongLengthT"                ,  "400");
      WriteValue("/ch/0..63/par/GateShortLengthT"               ,  "100");
      WriteValue("/ch/0..63/par/GateOffsetT"                    ,  "50");
      WriteValue("/ch/0..63/par/LongChargeIntegratorPedestal"   ,  "0");
      WriteValue("/ch/0..63/par/ShortChargeIntegratorPedestal"  ,  "0");
      WriteValue("/ch/0..63/par/EnergyGain"                     ,  "x1");

      //=========== Discrimination
      WriteValue("/ch/0..63/par/TriggerFilterSelection"         ,  "LeadingEdge");
      WriteValue("/ch/0..63/par/CFDDelayT"                      ,  "32");
      WriteValue("/ch/0..63/par/CFDFraction"                    ,  "25");
      WriteValue("/ch/0..63/par/TimeFilterSmoothing"            ,  "Disabled");
      WriteValue("/ch/0..63/par/ChargeSmoothing"                ,  "Disabled");
      WriteValue("/ch/0..63/par/SmoothingFactor"                ,  "1");
      WriteValue("/ch/0..63/par/PileupGap"                      ,  "1000");

      //=========== Input
      WriteValue("/ch/0..63/par/ADCInputBaselineAvg"            ,  "MediumHigh");
      WriteValue("/ch/0..63/par/AbsoluteBaseline"               ,  "1000");
      WriteValue("/ch/0..63/par/ADCInputBaselineGuardT"         ,  "0");
      WriteValue("/ch/0..63/par/TimeFilterRetriggerGuardT"      ,  "0");
      WriteValue("/ch/0..63/par/TriggerHysteresis"              ,  "Enabled");
      
      //========== Other
      WriteValue("/ch/0..63/par/NeutronThreshold"               ,  "0");
      WriteValue("/ch/0..63/par/EventNeutronReject"             ,  "Disabled");
      WriteValue("/ch/0..63/par/WaveNeutronReject"              ,  "Disabled");

    }

  }
}

void Digitizer2Gen::PrintBoardSettings(){

  for(int i = 0; i < (int) boardSettings.size(); i++){
    if( boardSettings[i].ReadWrite() == RW::WriteOnly) continue;
    
    //--- exclude some TempSens for Not VX2745
    if( ModelName != "VX2745" && 
        ( boardSettings[i].GetPara() == PHA::DIG::TempSensADC1.GetPara() ||
          boardSettings[i].GetPara() == PHA::DIG::TempSensADC2.GetPara() ||
          boardSettings[i].GetPara() == PHA::DIG::TempSensADC3.GetPara() ||
          boardSettings[i].GetPara() == PHA::DIG::TempSensADC4.GetPara() ||
          boardSettings[i].GetPara() == PHA::DIG::TempSensADC5.GetPara() ||
          boardSettings[i].GetPara() == PHA::DIG::TempSensADC6.GetPara() ) ) {
      continue;
    }

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

  if( CupVer >= 2023091800 ){
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

  for( int i = 0; i < (int) chSettings[0].size(); i++){
    if( chSettings[ch][i].ReadWrite() == RW::WriteOnly) continue;
    printf("%-45s  %d  %s\n", chSettings[ch][i].GetFullPara(ch).c_str(), 
                              chSettings[ch][i].ReadWrite(),
                              chSettings[ch][i].GetValue().c_str());
  }
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
  if( ret != CAEN_FELib_Stop ) printf("Error msg (%d): %s\n", ret, msg);
  return msg;
}

//^===================================================== Settings
void Digitizer2Gen::ReadAllSettings(){
  if( !isConnected ) return;

  printf("Digitizer2Gen::%s | %s \n", __func__, FPGAType.c_str());

  for(int i = 0; i < (int) boardSettings.size(); i++){
    if( boardSettings[i].ReadWrite() == RW::WriteOnly) continue;

    // here TempSens is same for PHA and PSD
    if( !(ModelName == "VX2745") && 
     (boardSettings[i].GetPara() == PHA::DIG::TempSensADC1.GetPara() ||
      boardSettings[i].GetPara() == PHA::DIG::TempSensADC2.GetPara() ||
      boardSettings[i].GetPara() == PHA::DIG::TempSensADC3.GetPara() ||
      boardSettings[i].GetPara() == PHA::DIG::TempSensADC4.GetPara() ||
      boardSettings[i].GetPara() == PHA::DIG::TempSensADC5.GetPara() ||
      boardSettings[i].GetPara() == PHA::DIG::TempSensADC6.GetPara()
     )
    ) continue;

    if( ModelName == "VX2730" && 
      (boardSettings[i].GetPara() == PHA::DIG::FreqSensCore.GetPara() ||  
       boardSettings[i].GetPara() == PHA::DIG::DutyCycleSensDCDC.GetPara()
      )
    ) continue;
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
      //--- exclude Gateway
      if( boardSettings[i].GetPara() == PHA::DIG::Gateway.GetPara()) {
        totCount --;
        continue;
      }

      //--- exclude some TempSens for Not VX2745
      if( ModelName != "VX2745" && 
         ( boardSettings[i].GetPara() == PHA::DIG::TempSensADC1.GetPara() ||
           boardSettings[i].GetPara() == PHA::DIG::TempSensADC2.GetPara() ||
           boardSettings[i].GetPara() == PHA::DIG::TempSensADC3.GetPara() ||
           boardSettings[i].GetPara() == PHA::DIG::TempSensADC4.GetPara() ||
           boardSettings[i].GetPara() == PHA::DIG::TempSensADC5.GetPara() ||
           boardSettings[i].GetPara() == PHA::DIG::TempSensADC6.GetPara() ) ) {
        totCount --;
        continue;
      }

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

    if( CupVer >= 2023091800 ){
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
                                               7000 + 4 * index + i,
                                               LVDSSettings[index][i].GetValue().c_str());
        count ++;
      }
    }

    for( int i = 0; i < (int) chSettings[0].size(); i++){
      for(int ch = 0; ch < nChannels ; ch++ ){
        if( chSettings[ch][i].ReadWrite() == RW::WriteOnly) continue;
        totCount ++;
        if( chSettings[ch][i].GetValue() == "") {
          printf("[%i] No value for %s , ch-%02d\n", i, chSettings[ch][i].GetPara().c_str(), ch);
          continue;
        }
        fprintf(saveFile, "%-45s!%d!%4d!%s\n", chSettings[ch][i].GetFullPara(ch).c_str(), 
                                               chSettings[ch][i].ReadWrite(),
                                               ch*100 + i,
                                               chSettings[ch][i].GetValue().c_str());
        count ++;
      }
    }    
    fclose(saveFile);

    if( count != totCount ) {
      printf("!!!!! some setting is empty. !!!!!! ");
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
    printf("Opened %s\n", settingFileName.c_str());
    char * para      = new char[100];
    char * readWrite = new char[100];
    char * idStr     = new char[100];
    char * value     = new char[100];

    char line[100];
    while(fgets(line, sizeof(line), loadFile) != NULL){

      //printf("%s", line);
      char* token = std::strtok(line, "!");
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
        token = std::strtok(nullptr, "!");
      }

      int id = atoi(idStr);
      if( id < 7000){ // channel
        int ch = id / 100;
        int index = id - ch * 100;
        chSettings[ch][index].SetValue(value);
        //printf("-------id : %d, ch: %d, index : %d\n", id,  ch, index);
        //printf("%s|%d|%d|%s|\n", chSettings[ch][index].GetFullPara(ch).c_str(), 
        //                         chSettings[ch][index].ReadWrite(), id, 
        //                         chSettings[ch][index].GetValue().c_str());

      }else if ( 7000 <= id && id < 8000){ // LVDS
        int index = (id-7000)/4;
        int ch = id - 7000 - index * 4;
        LVDSSettings[index][ch].SetValue(value);

      }else if ( 8000 <= id && id < 9000){ // board
        boardSettings[id - 8000].SetValue(value);
        //printf("%s|%d|%d|%s\n", boardSettings[id-8000].GetFullPara().c_str(),
        //                        boardSettings[id-8000].ReadWrite(), id,
        //                        boardSettings[id-8000].GetValue().c_str());
      }else if ( 9000 <= id && id < 9050){ // vga
        VGASetting[id - 9000].SetValue(value);
      }else{ // group
        if( CupVer >= 2023091800 ) InputDelay[id - 9050].SetValue(value);
      }
      //printf("%s|%s|%d|%s|\n", para, readWrite, id, value);
      if( std::strcmp(readWrite, "2") == 0 && isConnected)  WriteValue(para, value, false);
    }

    delete [] para;
    delete [] readWrite;
    delete [] idStr;
    delete [] value;

    return true;
  }else{
    printf("Fail to opened %s\n", settingFileName.c_str());
  }

  return false;
  
}

std::string Digitizer2Gen::GetSettingValueFromMemory(const Reg para, unsigned int ch_index) {
  int index = FindIndex(para);
  switch (para.GetType()){
    case TYPE::DIG:   return boardSettings[index].GetValue();
    case TYPE::CH:    return chSettings[ch_index][index].GetValue();
    case TYPE::VGA:   return VGASetting[ch_index].GetValue();
    case TYPE::LVDS:  return LVDSSettings[ch_index][index].GetValue();
    case TYPE::GROUP: return InputDelay[ch_index].GetValue();
    default : return "invalid";
  }
  return "no such parameter";
}
