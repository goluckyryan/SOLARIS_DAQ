#include "SolReader.h"
#include <cstdio>
#include <cstdlib>

#include "TFile.h"
#include "TTree.h"
#include "TMath.h"
#include "TString.h"
#include "TMacro.h"
//#include "TClonesArray.h" // plan to save trace as TVector with TClonesArray
//#include "TVector.h"

#define MAX_MULTI 64
#define MAX_TRACE_LEN 2500

#define tick2ns 8 // 1 tick = 8 ns

SolReader ** reader;
Hit ** hit;

std::vector<std::vector<int>> idList;

unsigned long totFileSize = 0;
unsigned long processedFileSize = 0;

std::vector<int> activeFileID;
std::vector<int> groupIndex;
std::vector<std::vector<int>> group; // group[i][j], i = group ID, j = group member)

void findEarliestTime(int &fileID, int &groupID){

  unsigned long firstTime = 0;
  for( int i = 0; i < (int) activeFileID.size(); i++){
    int id = activeFileID[i];
    if( i == 0 ) {
      firstTime = hit[id]->timestamp;
      fileID = id;
      groupID = i;
      //printf("%d | %d %lu %d | %d \n", id, reader[id]->GetBlockID(), hit[id]->timestamp, hit[id]->channel, (int) activeFileID.size());
      continue;
    }
    if( hit[id]->timestamp <= firstTime) {
      firstTime = hit[id]->timestamp;
      fileID = id;
      groupID = i;
      //printf("%d | %d %lu %d | %d \n", id, reader[id]->GetBlockID(), hit[id]->timestamp, hit[id]->channel, (int) activeFileID.size());
    }
  }

}

unsigned long long            evID  = 0;
unsigned int                  multi = 0;
unsigned short        bd[MAX_MULTI] = {0}; 
unsigned short        sn[MAX_MULTI] = {0};
unsigned short        ch[MAX_MULTI] = {0};
unsigned short         e[MAX_MULTI] = {0};  
unsigned short        e2[MAX_MULTI] = {0};  //for PSD energy short
unsigned long long   e_t[MAX_MULTI] = {0};
unsigned short       e_f[MAX_MULTI] = {0};
unsigned short   lowFlag[MAX_MULTI] = {0};
unsigned short  highFlag[MAX_MULTI] = {0};
int             traceLen[MAX_MULTI] = {0};
int trace[MAX_MULTI][MAX_TRACE_LEN] = {0};

void fillData(int &fileID, const bool &saveTrace){
  bd[multi]       = idList[fileID][1];
  sn[multi]       = idList[fileID][3];
  ch[multi]       = hit[fileID]->channel;
  e[multi]        = hit[fileID]->energy;
  e2[multi]       = hit[fileID]->energy_short;
  e_t[multi]      = hit[fileID]->timestamp;
  e_f[multi]      = hit[fileID]->fine_timestamp;
  lowFlag[multi]  = hit[fileID]->flags_low_priority;
  highFlag[multi] = hit[fileID]->flags_high_priority;
  
  if( saveTrace ){
    traceLen[multi] = hit[fileID]->traceLenght;
    for( int i = 0; i < TMath::Min(traceLen[multi], MAX_TRACE_LEN); i++){
      trace[multi][i] = hit[fileID]->analog_probes[0][i];
    } 
  }

  multi++;
  reader[fileID]->ReadNextBlock();
}

void printEvent(){
  printf("==================== evID : %llu\n", evID);
  for( int i = 0; i < multi; i++){
    printf("    %2d | %d %d | %llu %d \n", i, bd[i], ch[i], e_t[i], e[i] );
  }
  printf("==========================================\n");
}

//^##################################################################################
int main(int argc, char ** argv){

  printf("=======================================================\n");
  printf("===        SOLARIS Event Builder sol  --> root      ===\n");
  printf("=======================================================\n");  

  if( argc <= 3){
    printf("%s [outfile] [timeWindow] [saveTrace] [sol-1] [sol-2] ... \n", argv[0]);
    printf("      outfile : output root file name\n");
    printf("   timeWindow : number of tick, 1 tick = %d ns.\n", tick2ns); 
    printf("    saveTrace : 1 = save trace, 0 = no trace\n");
    printf("        sol-X : the sol file(s)\n");
    return -1;
  }

  // for( int i = 0; i < argc; i++){
  //   printf("%d | %s\n", i, argv[i]);
  // }

  TString outFileName = argv[1];
  int timeWindow = abs(atoi(argv[2]));
  const bool saveTrace = atoi(argv[3]);

  const int nFile = argc - 4;
  TString inFileName[nFile];
  for( int i = 0 ; i < nFile ; i++){
    inFileName[i] = argv[i+4];
  }

  //*======================================== setup reader
  reader = new SolReader*[nFile];
  hit = new Hit *[nFile];

  for( int i = 0 ; i < nFile ; i++){
    reader[i] = new SolReader(inFileName[i].Data());
    hit[i] = reader[i]->hit;   //TODO check is file open propertly
    reader[i]->ReadNextBlock(); // read the first block
  }

  //*======================================== group files
  idList.clear();
  for( int i = 0; i < nFile; i++){
    TString fn = inFileName[i];

    int pos = fn.Last('/'); //  path
    fn.Remove(0, pos+1);

    pos = fn.First('_'); //  expName;
    fn.Remove(0, pos+1);

    pos = fn.First('_'); //  runNum;
    fn.Remove(0, pos+1);

    pos = fn.First('_'); // digiID
    TString f1 = fn;
    int digiID = f1.Remove(pos).Atoi();
    fn.Remove(0, pos+1);

    pos = fn.Last('_'); // digi serial num
    f1 = fn;
    int digisn = f1.Remove(pos).Atoi();
    fn.Remove(0, pos+1);

    pos = fn.First('.'); // get the file id;
    int indexID = fn.Remove(pos).Atoi();

    int fileID = i;
    std::vector<int> haha = {fileID, digiID, indexID, digisn};
    idList.push_back(haha);
  }

  // sort by digiID
  std::sort(idList.begin(), idList.end(), [](const std::vector<int>& a, const std::vector<int>& b){ 
    if (a[1] == b[1]) {
      return a[2] < b[2];
    }
    return a[1] < b[1];
  });

  group.clear(); // group[i][j], i is the group Index = digiID
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
  printf(" Event building time window : %d tics = %d nsec \n", timeWindow, timeWindow*tick2ns);
  printf(" Save Trace ? %s \n", saveTrace ? "Yes" : "No");
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

  if( saveTrace){
    tree->Branch("tl",        traceLen, "traceLen[multi]/I");
    tree->Branch("trace",        trace, Form("trace[multi][%d]/I", MAX_TRACE_LEN));
  }

  //*=========================================== build event

  //@---- using file from group[i][0] first

  //--- find earlist time among the files
  activeFileID.clear();
  groupIndex.clear();  //the index of each group

  for(int i = 0; i < (int) group.size(); i++) {
    groupIndex.push_back(0);
    activeFileID.push_back(group[i][0]);
  }

  int fileID = 0;
  int groupID = 0;
  findEarliestTime(fileID, groupID);
  fillData(fileID, saveTrace);

  unsigned long firstTimeStamp = hit[fileID]->timestamp;
  unsigned long lastTimeStamp = 0;

  int last_precentage = 0;
  while((activeFileID.size() > 0)){

    findEarliestTime(fileID, groupID);
    if( reader[fileID]->IsEndOfFile() ){
      groupIndex[groupID] ++;
      if( groupIndex[groupID] < (int) group[groupID].size() ){
        activeFileID[groupID] = group[groupID][groupIndex[groupID]];
        fileID = activeFileID[groupID];
      }else{
        activeFileID.erase(activeFileID.begin() + groupID);
      }
    }

    if( hit[fileID]->timestamp - e_t[0] < timeWindow ){
      fillData(fileID, saveTrace);
    }else{
      outRootFile->cd();
      tree->Fill();
      evID ++;

      multi = 0;
      fillData(fileID, saveTrace);
    }
    
    ///========= calculate progress
    processedFileSize = 0;
    for( int p = 0; p < (int) group.size(); p ++){
      for( int q = 0; q <= groupIndex[p]; q++){
        if( groupIndex[p] < (int) group[p].size() ){
          int id = group[p][q];
          processedFileSize += reader[id]->GetFilePos();
        }
      }
    }
    double percentage = processedFileSize * 100/ totFileSize;
    if( percentage >= last_precentage ) {
      printf("Processed : %llu, %.0f%% | %lu/%lu | ", evID, percentage, processedFileSize, totFileSize);
      for( int i = 0; i < (int) activeFileID.size(); i++) printf("%d, ", activeFileID[i]);
      printf(" \n\033[A\r");
      last_precentage = percentage + 1.0;
    }
  }; ///====== end of event building loop

  processedFileSize = 0;
  for( int p = 0; p < (int) group.size(); p ++){
   for( int q = 0; q < (int) group[p].size(); q++){
     int id = group[p][q];
     processedFileSize += reader[id]->GetFilePos();
   }
  }
  double percentage = processedFileSize * 100/ totFileSize;
  printf("Processed : %llu, %.0f%% | %lu/%lu            \n", evID, percentage, processedFileSize, totFileSize);

  lastTimeStamp = hit[fileID]->timestamp;
  //*=========================================== save file
  outRootFile->cd();
  tree->Fill();
  evID ++;
  tree->Write();

  //*=========================================== Save timestamp as TMacro
  TMacro timeStamp;
  TString str;
  str.Form("%lu", firstTimeStamp); timeStamp.AddLine( str.Data() );
  str.Form("%lu", lastTimeStamp); timeStamp.AddLine( str.Data() );
  timeStamp.Write("timeStamp");

  unsigned int numBlock = 0;
  for( int i = 0; i < nFile; i++){
    //printf("%d | %8ld | %10u/%10u\n", i,  reader[i]->GetBlockID() + 1, reader[i]->GetFilePos(), reader[i]->GetFileSize());
    numBlock += reader[i]->GetBlockID() + 1;
  }

  printf("===================================== done. \n");
  printf("Number of Block Scanned : %u\n", numBlock);
  printf("  Number of Event Built : %lld\n", evID);
  printf("  Output Root File Size : %.2f MB\n", outRootFile->GetSize()/1024./1024.);
  printf("        first timestamp : %lu \n", firstTimeStamp);
  printf("         last timestamp : %lu \n", lastTimeStamp);
  unsigned long duration = lastTimeStamp - firstTimeStamp;
  printf("         total duration : %lu = %.2f sec \n", duration, duration * tick2ns * 1.0 / 1e9 );
  printf("===================================== end of summary. \n");


  //^############## delete new
  for( int i = 0; i < nFile; i++) delete reader[i];
  delete [] reader;
  outRootFile->Close();

  return 0;
}