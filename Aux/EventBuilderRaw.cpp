#include "../RawDecoder.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cstring>

#include "TFile.h"
#include "TTree.h"
#include "TMath.h"
#include "TString.h"
#include "TMacro.h"

#include <sys/time.h>

inline unsigned int getTime_us(){
  unsigned int time_us;
  struct timeval t1;
  struct timezone tz;
  gettimeofday(&t1, &tz);
  time_us = (t1.tv_sec) * 1000 * 1000 + t1.tv_usec;
  return time_us;
}

#define MAX_MULTI 50000

//^#################### Raw File Reader ############################
class RawFileReader {
public:
  FILE * inFile;
  uint64_t inFileSize;
  uint64_t filePos;
  unsigned int numBlock;

  uint8_t * blobBuffer;
  size_t blobSize;
  unsigned short identifier;
  std::string dppType;

  RawDecoder decoder;
  std::vector<RawDecoder::DecodedHit> hits;
  size_t hitCursor;

  // Current hit fields (for compatibility with EventBuilder pattern)
  uint8_t  channel;
  uint16_t energy;
  uint16_t energy_short;
  uint64_t timestamp;
  uint16_t fine_timestamp;
  uint16_t flags_low_priority;
  uint16_t flags_high_priority;

  unsigned short tick2ns;

  RawFileReader(){
    inFile = NULL;
    inFileSize = 0;
    filePos = 0;
    numBlock = 0;
    blobBuffer = new uint8_t[20*1024*1024];
    blobSize = 0;
    hitCursor = 0;
    tick2ns = 2; // default for VX2730 (500 Msps)
    dppType = DPPType::PSD;
  }

  ~RawFileReader(){
    if( inFile ) fclose(inFile);
    delete[] blobBuffer;
  }

  void OpenFile(const char * fileName){
    inFile = fopen(fileName, "rb");
    if( inFile == NULL ){
      printf("Cannot open file : %s \n", fileName);
      return;
    }
    fseek(inFile, 0L, SEEK_END);
    inFileSize = ftell(inFile);
    rewind(inFile);
  }

  bool IsEndOfFile() const { return (filePos >= inFileSize); }
  uint64_t GetFilePos() const { return filePos; }
  uint64_t GetFileSize() const { return inFileSize; }
  unsigned int GetBlockID() const { return numBlock > 0 ? numBlock - 1 : 0; }

  int ReadNextBlob(){
    if( inFile == NULL ) return -1;
    if( feof(inFile) ) return -1;
    if( filePos >= inFileSize ) return -1;

    // Read identifier (2 bytes)
    if( fread(&identifier, 2, 1, inFile) != 1 ) return -1;
    if( (identifier & 0xAA00) != 0xAA00 ){
      printf("RawFileReader: bad identifier 0x%04X at pos %lu\n", identifier, filePos);
      return -2;
    }

    // Detect DPP type from identifier
    dppType = ((identifier >> 4) & 0xF) == 0 ? DPPType::PHA : DPPType::PSD;

    // Read blob size (8 bytes)
    if( fread(&blobSize, 8, 1, inFile) != 1 ) return -1;

    // Read blob data
    if( blobSize > 20*1024*1024 ) return -3; // too large
    if( fread(blobBuffer, blobSize, 1, inFile) != 1 ) return -1;

    filePos = ftell(inFile);
    numBlock++;

    // Decode
    decoder.LoadBlob(blobBuffer, blobSize, dppType);

    // Collect all physics hits
    hits.clear();
    RawDecoder::DecodedHit dh;
    while( decoder.Next(dh) ){
      hits.push_back(dh);
    }
    hitCursor = 0;

    return 0;
  }

  // Read next hit: yields one decoded hit at a time, fetches new blob when needed
  // Returns 0 on success, -1 on end of file
  int ReadNextHit(){
    while( true ){
      if( hitCursor < hits.size() ){
        const auto& h = hits[hitCursor];
        channel          = h.channel;
        energy           = h.energy;
        energy_short     = h.energy_short;
        timestamp        = h.timestamp * tick2ns;
        fine_timestamp   = h.fine_timestamp * tick2ns;
        flags_low_priority  = h.flags_low_priority;
        flags_high_priority = h.flags_high_priority;
        hitCursor++;
        return 0;
      }
      // Need more data
      int ret = ReadNextBlob();
      if( ret != 0 ) return -1;
      // If blob had no physics hits, loop will try next blob
    }
  }
};

//^#################### Globals ############################

RawFileReader ** reader;

std::vector<std::vector<int>> idList;

unsigned long totFileSize = 0;
unsigned long processedFileSize = 0;

std::vector<int> activeFileID;
std::vector<int> activeGroupID;
std::vector<int> groupIndex;
std::vector<std::vector<int>> group;

void findEarliestTime(int &fileID, int &groupID){
  unsigned long firstTime = 0;
  for( int i = 0; i < (int) activeFileID.size(); i++){
    int id = activeFileID[i];
    if( i == 0 ) {
      firstTime = reader[id]->timestamp;
      fileID = id;
      groupID = i;
      continue;
    }
    if( reader[id]->timestamp <= firstTime) {
      firstTime = reader[id]->timestamp;
      fileID = id;
      groupID = i;
    }
  }
}

unsigned long long            evID  = 0;
unsigned int                  multi = 0;
unsigned short        bd[MAX_MULTI] = {0};
unsigned short        sn[MAX_MULTI] = {0};
unsigned short        ch[MAX_MULTI] = {0};
unsigned short         e[MAX_MULTI] = {0};
unsigned short        e2[MAX_MULTI] = {0};
unsigned long long   e_t[MAX_MULTI] = {0};
unsigned short       e_f[MAX_MULTI] = {0};
unsigned short   lowFlag[MAX_MULTI] = {0};
unsigned short  highFlag[MAX_MULTI] = {0};

void fillData(int &fileID){
  if( multi >= MAX_MULTI ) {
    reader[fileID]->ReadNextHit();
    return;
  }

  bd[multi]       = idList[fileID][1];
  sn[multi]       = idList[fileID][3];
  ch[multi]       = reader[fileID]->channel;
  e[multi]        = reader[fileID]->energy;
  e2[multi]       = reader[fileID]->energy_short;
  e_t[multi]      = reader[fileID]->timestamp;
  e_f[multi]      = reader[fileID]->fine_timestamp;
  lowFlag[multi]  = reader[fileID]->flags_low_priority;
  highFlag[multi] = reader[fileID]->flags_high_priority;

  multi++;
  reader[fileID]->ReadNextHit();
}

//^##################################################################################
int main(int argc, char ** argv){

  printf("=======================================================\n");
  printf("===    SOLARIS Raw Event Builder sol_raw --> root    ===\n");
  printf("=======================================================\n");

  if( argc <= 3){
    printf("%s [outfile] [timeWindow] [tick2ns] [sol_raw-1] [sol_raw-2] ... \n", argv[0]);
    printf("      outfile : output root file name\n");
    printf("   timeWindow : nano-sec; if < 0, no event build\n");
    printf("      tick2ns : time tick in ns (2 for VX2730 500Msps, 8 for VX2740 125Msps)\n");
    printf("    sol_raw-X : the sol_raw file(s)\n");
    return -1;
  }

  unsigned int runStartTime = getTime_us();

  TString outFileName = argv[1];
  int timeWindow = atoi(argv[2]);
  unsigned short tick2ns = atoi(argv[3]);

  const int nFile = argc - 4;
  std::vector<TString> inFileName(nFile);
  for( int i = 0 ; i < nFile ; i++){
    inFileName[i] = argv[i+4];
  }

  //*======================================== setup reader
  reader = new RawFileReader*[nFile];

  for( int i = 0 ; i < nFile ; i++){
    reader[i] = new RawFileReader();
    reader[i]->tick2ns = tick2ns;
    reader[i]->OpenFile(inFileName[i].Data());
    reader[i]->ReadNextHit(); // read the first hit
  }

  //*======================================== group files
  idList.clear();
  for( int i = 0; i < nFile; i++){
    TString fn = inFileName[i];

    int pos = fn.Last('/');
    fn.Remove(0, pos+1);

    pos = fn.First('_'); // expName
    fn.Remove(0, pos+1);

    pos = fn.First('_'); // runNum
    fn.Remove(0, pos+1);

    pos = fn.First('_'); // digiID
    TString f1 = fn;
    int digiID = f1.Remove(pos).Atoi();
    fn.Remove(0, pos+1);

    pos = fn.Last('_'); // digi serial num
    f1 = fn;
    int digisn = f1.Remove(pos).Atoi();
    fn.Remove(0, pos+1);

    pos = fn.First('.'); // file index
    int indexID = fn.Remove(pos).Atoi();

    int fileID = i;
    std::vector<int> haha = {fileID, digiID, indexID, digisn};
    idList.push_back(haha);
  }

  // sort by digiID
  std::sort(idList.begin(), idList.end(), [](const std::vector<int>& a, const std::vector<int>& b){
    if (a[1] == b[1]) return a[2] < b[2];
    return a[1] < b[1];
  });

  group.clear();
  int last_id = 0;
  std::vector<int> kaka;
  for( int i = 0; i < (int) idList.size() ; i++){
    if( i == 0 ) {
      kaka.clear();
      last_id = idList[i][1];
      kaka.push_back(idList[i][0]);
      continue;
    }
    if( idList[i][1] != last_id ) {
      last_id = idList[i][1];
      group.push_back(kaka);
      kaka.clear();
      kaka.push_back(idList[i][0]);
    }else{
      kaka.push_back(idList[i][0]);
    }
  }
  group.push_back(kaka);

  printf(" out file : \033[1;33m%s\033[m\n", outFileName.Data());
  if ( timeWindow < 0 ){
    printf(" Event building time window : no event build\n");
  }else{
    printf(" Event building time window : %d nsec \n", timeWindow);
  }
  printf(" tick2ns : %d \n", tick2ns);
  printf(" Number of input file : %d \n", nFile);
  for( int i = 0; i < nFile; i++){
    printf("  %2d| %5.1f MB| %s \n", i, reader[i]->GetFileSize()/1024./1024., inFileName[i].Data());
    totFileSize += reader[i]->GetFileSize();
  }
  printf("------------------------------------\n");
  for( int i = 0; i < (int) group.size(); i++){
    printf("Group %d :", i);
    for( int j = 0; j < (int) group[i].size(); j ++){
      printf("%d, ", group[i][j]);
    }
    printf("\n");
  }
  printf("------------------------------------\n");

  //*======================================== setup tree
  TFile * outRootFile = new TFile(outFileName, "recreate");
  outRootFile->cd();

  TTree * tree = new TTree("tree", outFileName);

  tree->Branch("evID",         &evID, "evID/l");
  tree->Branch("multi",       &multi, "multi/i");
  tree->Branch("bd",              bd, "bd[multi]/s");
  tree->Branch("sn",              sn, "sn[multi]/s");
  tree->Branch("ch",              ch, "ch[multi]/s");
  tree->Branch("e",                e, "e[multi]/s");
  tree->Branch("e2",              e2, "e2[multi]/s");
  tree->Branch("e_t",            e_t, "e_t[multi]/l");
  tree->Branch("e_f",            e_f, "e_f[multi]/s");
  tree->Branch("lowFlag",    lowFlag, "lowFlag[multi]/s");
  tree->Branch("highFlag",  highFlag, "highFlag[multi]/s");

  //*=========================================== build event

  activeFileID.clear();
  activeGroupID.clear();
  groupIndex.clear();

  for(int i = 0; i < (int) group.size(); i++) {
    groupIndex.push_back(0);
    activeFileID.push_back(group[i][0]);
    activeGroupID.push_back(i);
  }

  int fileID = 0;
  int groupID = 0;
  findEarliestTime(fileID, groupID);
  fillData(fileID);

  unsigned long long firstTimeStamp = e_t[0];
  unsigned long long lastTimeStamp = 0;

  int last_percentage = 0;
  while((activeFileID.size() > 0)){

    findEarliestTime(fileID, groupID);
    if( reader[fileID]->IsEndOfFile() ){
      int origGroup = activeGroupID[groupID];
      groupIndex[groupID] ++;
      if( groupIndex[groupID] < (int) group[origGroup].size() ){
        activeFileID[groupID] = group[origGroup][groupIndex[groupID]];
        fileID = activeFileID[groupID];
      }else{
        activeFileID.erase(activeFileID.begin() + groupID);
        activeGroupID.erase(activeGroupID.begin() + groupID);
        groupIndex.erase(groupIndex.begin() + groupID);
      }
    }

    if (timeWindow < 0 ){
      lastTimeStamp = e_t[0];
      outRootFile->cd();
      tree->Fill();
      evID ++;
      multi = 0;
      fillData(fileID);
    }else{
      if( reader[fileID]->timestamp - e_t[0] < (unsigned long long) timeWindow ){
        fillData(fileID);
      }else{
        lastTimeStamp = e_t[0];
        outRootFile->cd();
        tree->Fill();
        evID ++;
        multi = 0;
        fillData(fileID);
      }
    }

    processedFileSize = 0;
    for( int p = 0; p < (int) activeGroupID.size(); p ++){
      int gID = activeGroupID[p];
      for( int q = 0; q <= groupIndex[p]; q++){
        int id = group[gID][q];
        processedFileSize += reader[id]->GetFilePos();
      }
    }
    double percentage = processedFileSize * 100.0 / totFileSize;
    if( percentage >= last_percentage ) {
      printf("Processed : %llu, %.0f%% | %lu/%lu\n\033[A\r", evID, percentage, processedFileSize, totFileSize);
      last_percentage = percentage + 1.0;
    }
  };

  processedFileSize = 0;
  for( int p = 0; p < (int) group.size(); p ++){
    for( int q = 0; q < (int) group[p].size(); q++){
      int id = group[p][q];
      processedFileSize += reader[id]->GetFilePos();
    }
  }
  double percentage = processedFileSize * 100.0 / totFileSize;
  printf("Processed : %llu, %.0f%% | %lu/%lu            \n", evID, percentage, processedFileSize, totFileSize);

  lastTimeStamp = e_t[0];
  //*=========================================== save file
  outRootFile->cd();
  tree->Fill();
  evID ++;
  tree->Write();

  TMacro timeStamp;
  TString str;
  str.Form("%llu", firstTimeStamp); timeStamp.AddLine( str.Data() );
  str.Form("%llu", lastTimeStamp); timeStamp.AddLine( str.Data() );
  timeStamp.Write("timeStamp");

  unsigned int numBlock = 0;
  for( int i = 0; i < nFile; i++){
    numBlock += reader[i]->GetBlockID() + 1;
  }

  unsigned int runEndTime = getTime_us();
  double runTime = (runEndTime - runStartTime) * 1e-6;
  printf("===================================== done. \n");
  printf("    event building time : %.2f sec = %.2f min\n", runTime, runTime/60.);
  printf("  Number of Blob Scanned : %u\n", numBlock);
  printf("  Number of Event Built : %lld\n", evID);
  printf("  Output Root File Size : %.2f MB\n", outRootFile->GetSize()/1024./1024.);
  printf("        first timestamp : %llu ns \n", firstTimeStamp);
  printf("         last timestamp : %llu ns \n", lastTimeStamp);
  unsigned long long duration = lastTimeStamp - firstTimeStamp;
  printf("        total duration* : %llu ns = %.2f sec \n", duration, duration * 1.0 / 1e9 );
  printf("===================================== end of summary. \n");

  for( int i = 0; i < nFile; i++) delete reader[i];
  delete [] reader;
  outRootFile->Close();

  return 0;
}
