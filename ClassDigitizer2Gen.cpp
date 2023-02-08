#include "ClassDigitizer2Gen.h"

Digitizer2Gen::Digitizer2Gen(){  
  printf("======== %s \n",__func__);
  Initialization();
}

Digitizer2Gen::~Digitizer2Gen(){
  printf("======== %s \n",__func__);
  if(isConnected ) CloseDigitizer();
}

void Digitizer2Gen::Initialization(){
  printf("======== %s \n",__func__);

  handle = 0;
  ret = 0;
  isConnected = false;
  isDummy = false;
  
  modelName = "";
  cupVersion = "";
  DPPVersion = "";
  DPPType = "";
  serialNumber = 0;
  adcBits = 0;
  nChannels = 0;
  adcRate = 0;
  ch2ns = 0;

  IPAddress = "";
  netMask = "";
  gateway = "";

  outFileIndex = 0;
  dataStartIndetifier = 0xAAA0;
  outFile = NULL;
  outFileSize = 0;

  evt = NULL;

  acqON = false;

}

void Digitizer2Gen::SetDummy(){

  isDummy = true;
  nChannels = 64;

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
std::string Digitizer2Gen::ReadValue(const char * parameter, bool verbose){
  if( !isConnected ) return "not connected";
  //printf(" %s|%s \n", __func__, parameter);
  ret = CAEN_FELib_GetValue(handle, parameter, retValue);
  if (ret != CAEN_FELib_Success) {
    return ErrorMsg(__func__);
  }else{
    if( verbose ) printf("%-45s : %s\n", parameter, retValue);
  }
  return retValue;
}

std::string Digitizer2Gen::ReadDigValue(std::string shortPara, bool verbose){
  std::string haha = "/par/" + shortPara;
  return ReadValue(haha.c_str(), verbose);
}

std::string Digitizer2Gen::ReadChValue(std::string ch, std::string shortPara, bool verbose){
  std::string haha = "/ch/" + ch + "/par/" + shortPara;
  return ReadValue(haha.c_str(), verbose);
}

void Digitizer2Gen::WriteValue(const char * parameter, std::string value){
  if( !isConnected ) return;
  printf(" %s| %-45s : %s\n", __func__, parameter, value.c_str());
  ret = CAEN_FELib_SetValue(handle, parameter, value.c_str());
  if (ret != CAEN_FELib_Success) {
    ErrorMsg(__func__);
    return;
  }
}

void Digitizer2Gen::WriteDigValue(std::string shortPara, std::string value){
  std::string haha = "/par/" + shortPara;
  WriteValue(haha.c_str(), value);
}

void Digitizer2Gen::WriteChValue(std::string ch, std::string shortPara, std::string value){
  std::string haha = "/ch/" + ch + "/par/" + shortPara;
  WriteValue(haha.c_str(), value);
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

  modelName  = ReadValue("/par/ModelName");
  
  cupVersion = ReadValue("/par/cupver");  
  DPPVersion = ReadValue("/par/FPGA_FwVer");
  DPPType    = ReadValue("/par/FwType");

  serialNumber = atoi(ReadValue("/par/SerialNum").c_str());
  nChannels    = atoi(ReadValue("/par/NumCh").c_str());
  adcBits      = atoi(ReadValue("/par/ADC_Nbit").c_str());  
  adcRate      = atoi(ReadValue("/par/ADC_SamplRate").c_str());
  ch2ns = 1000/adcRate;
  
  IPAddress   = ReadValue("/par/IPAddress");
  netMask     = ReadValue("/par/Netmask");
  gateway     = ReadValue("/par/Gateway");

  printf("   IP address : %s\n", IPAddress.c_str());
  printf("     Net Mask : %s\n", netMask.c_str());
  printf("      Gateway : %s\n", gateway.c_str());
  
  printf("   Model name : %s\n", modelName.c_str());
  printf("  CUP version : %s\n", cupVersion.c_str());
  printf("     DPP Type : %s\n", DPPType.c_str());
  printf("  DPP Version : %s\n", DPPVersion.c_str());
  printf("Serial number : %d\n", serialNumber);
  printf("     ADC bits : %d\n", adcBits);
  printf("     ADC rate : %d Msps, ch2ns : %d ns\n", adcRate, ch2ns);
  printf("     Channels : %d\n", nChannels);

  //ReadValue("/par/InputRange", true);
  //ReadValue("/par/InputType", true);
  //ReadValue("/par/Zin", true);
  printf("====================== \n");

  return 0;
}

int Digitizer2Gen::CloseDigitizer(){
  printf("======== %s \n",__func__);
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

  evt->traceZero = false;

  if( ret != CAEN_FELib_Success) {
    //ErrorMsg("ReadData()");
    return ret;
  }

  return ret;
}

//###########################################

void Digitizer2Gen::OpenOutFile(std::string fileName){
  outFileNameBase = fileName;
  sprintf(outFileName, "%s_%03d.sol", fileName.c_str(), outFileIndex);
  outFile = fopen(outFileName, "a+");
  fseek(outFile, 0L, SEEK_END);
  outFileSize = ftell(outFile);  // unsigned int =  Max ~4GB

}

void Digitizer2Gen::CloseOutFile(){
  fclose(outFile);
}

void Digitizer2Gen::SaveDataToFile(){

  if( outFileSize > (unsigned int) MaxOutFileSize){
    fclose(outFile);
    outFileIndex ++;
    sprintf(outFileName, "%s_%03d.sol", outFileNameBase.c_str(), outFileIndex);
    outFile = fopen(outFileName, "a+");
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

void Digitizer2Gen::ReadDigitizerSettings(){

  ReadValue("/ch/4/par/ChRecordLengthS"  , true);
  ReadValue("/ch/4/par/ChPreTriggerS"    , true);
  ReadValue("/ch/4/par/WaveResolution"   , true);
  ReadValue("/ch/4/par/WaveAnalogProbe0" , true);
  ReadValue("/ch/4/par/WaveAnalogProbe1" , true);
  ReadValue("/ch/4/par/WaveDigitalProbe0", true);
  ReadValue("/ch/4/par/WaveDigitalProbe1", true);
  ReadValue("/ch/4/par/WaveDigitalProbe2", true); 
  ReadValue("/ch/4/par/WaveDigitalProbe3", true); 

  ReadValue("/ch/4/par/ChannelsTriggerMask", true); 

  ReadValue("/ch/0/par/ChannelsTriggerMask", true); 

}

std::string Digitizer2Gen::ErrorMsg(const char * funcName){
  printf("======== %s | %s\n",__func__, funcName);
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
