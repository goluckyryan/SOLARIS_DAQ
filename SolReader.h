#ifndef SOLREADER_H
#define SOLREADER_H

#include <stdio.h> /// for FILE
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
#include <time.h> // time in nano-sec

#include "Event.h"

class SolReader {
  private:
    FILE * inFile;
    unsigned int inFileSize;
    unsigned int filePos;
    unsigned int totNumBlock;

    unsigned short blockStartIdentifier;
    unsigned int numBlock;
    bool isScanned;

    void init();

    std::vector<unsigned int> blockPos;

  public:
    SolReader();
    SolReader(std::string fileName, unsigned short dataType);
    ~SolReader();

    void OpenFile(std::string fileName);
    int  ReadNextBlock(int opt = 0); // opt = 0, noraml, 1, fast
    int  ReadBlock(unsigned int index);

    void ScanNumBlock();

    unsigned int GetNumBlock()      {return numBlock;}
    unsigned int GetTotalNumBlock() {return totNumBlock;}
    unsigned int GetFilePos()       {return filePos;}
    unsigned int GetFileSize()      {return inFileSize;}
    
    void RewindFile(); 

    Event * evt;

};

void SolReader::init(){
  inFileSize = 0;
  numBlock = 0;
  filePos = 0;
  totNumBlock = 0;
  evt = new Event();

  isScanned = false;

  blockPos.clear();

}

SolReader::SolReader(){
  init();
}

SolReader::SolReader(std::string fileName, unsigned short dataType = 0){
  init();
  OpenFile(fileName);
  evt->SetDataType(dataType);
}

SolReader::~SolReader(){
  if( !inFile ) fclose(inFile);
  delete evt;
}

inline void SolReader::OpenFile(std::string fileName){
  inFile = fopen(fileName.c_str(), "r");
  if( inFile == NULL ){
    printf("Cannot open file : %s \n", fileName.c_str());
  }else{
    fseek(inFile, 0L, SEEK_END);
    inFileSize = ftell(inFile);
    rewind(inFile);
  }
}

inline int SolReader::ReadBlock(unsigned int index){
  if( isScanned == false) return -1;
  if( index >= totNumBlock )return -1;
  fseek(inFile, 0L, SEEK_SET);

  printf("-----------%u, %u\n", index, blockPos[index]);

  fseek(inFile, blockPos[index], SEEK_CUR);

  numBlock = index;

  return ReadNextBlock();
}

inline int SolReader::ReadNextBlock(int opt){
  if( inFile == NULL ) return -1;
  if( feof(inFile) ) return -1;
  if( filePos >= inFileSize) return -1;
  
  fread(&blockStartIdentifier, 2, 1, inFile);

  if( (blockStartIdentifier & 0xAAA0) != 0xAAA0 ) {
    printf("header fail.\n");
    return -2 ;
  } 

  if( ( blockStartIdentifier & 0xF ) == 15 ){
    evt->SetDataType(15);  
  }
  evt->dataType = blockStartIdentifier & 0xF;

  if( evt->dataType == 0){
    if( opt == 0 ){
      fread(&evt->channel, 1, 1, inFile);
      fread(&evt->energy, 2, 1, inFile);
      fread(&evt->timestamp, 6, 1, inFile);
      fread(&evt->fine_timestamp, 2, 1, inFile);
      fread(&evt->flags_high_priority, 1, 1, inFile);
      fread(&evt->flags_low_priority, 2, 1, inFile);
      fread(&evt->downSampling, 1, 1, inFile);
      fread(&evt->board_fail, 1, 1, inFile);
      fread(&evt->flush, 1, 1, inFile);
      fread(&evt->trigger_threashold, 2, 1, inFile);
      fread(&evt->event_size, 8, 1, inFile);
      fread(&evt->aggCounter, 4, 1, inFile);
    }else{
      fseek(inFile, 31, SEEK_CUR);
    }
    fread(&evt->traceLenght, 8, 1, inFile);
    if( opt == 0){
      fread(evt->analog_probes_type, 2, 1, inFile);
      fread(evt->digital_probes_type, 4, 1, inFile);
      fread(evt->analog_probes[0], evt->traceLenght*4, 1, inFile);
      fread(evt->analog_probes[1], evt->traceLenght*4, 1, inFile);
      fread(evt->digital_probes[0], evt->traceLenght, 1, inFile);
      fread(evt->digital_probes[1], evt->traceLenght, 1, inFile);
      fread(evt->digital_probes[2], evt->traceLenght, 1, inFile);
      fread(evt->digital_probes[3], evt->traceLenght, 1, inFile);
    }else{
      fseek(inFile, 6 + evt->traceLenght*(12), SEEK_CUR);
    } 
  }else if( evt->dataType == 1){
    if( opt == 0 ){
      fread(&evt->channel, 1, 1, inFile);
      fread(&evt->energy, 2, 1, inFile);
      fread(&evt->timestamp, 6, 1, inFile);
      fread(&evt->fine_timestamp, 2, 1, inFile);
      fread(&evt->flags_high_priority, 1, 1, inFile);
      fread(&evt->flags_low_priority, 2, 1, inFile);
    }else{
      fseek(inFile, 14, SEEK_CUR);
    }
    fread(&evt->traceLenght, 8, 1, inFile);
    if( opt == 0){
      fread(&evt->analog_probes_type[0], 1, 1, inFile);
      fread(evt->analog_probes[0], evt->traceLenght*4, 1, inFile);
    }else{
      fseek(inFile, 1 + evt->traceLenght*4, SEEK_CUR);
    }
  }else if( evt->dataType == 2){
    if( opt == 0 ){
      fread(&evt->channel, 1, 1, inFile);
      fread(&evt->energy, 2, 1, inFile);
      fread(&evt->timestamp, 6, 1, inFile);
      fread(&evt->fine_timestamp, 2, 1, inFile);
      fread(&evt->flags_high_priority, 1, 1, inFile);
      fread(&evt->flags_low_priority, 2, 1, inFile);
    }else{
      fseek(inFile, 14, SEEK_CUR);
    }
  }else if( evt->dataType == 3){
    if( opt == 0 ){
      fread(&evt->channel, 1, 1, inFile);
      fread(&evt->energy, 2, 1, inFile);
      fread(&evt->timestamp, 6, 1, inFile);
    }else{
      fseek(inFile, 9, SEEK_CUR);
    }
  }else if( evt->dataType == 15){
      fread(&evt->dataSize, 8, 1, inFile);
    if( opt == 0){
      fread(evt->data, evt->dataSize, 1, inFile);
    }else{
      fseek(inFile, evt->dataSize, SEEK_CUR);
    }
  }

  numBlock ++;
  filePos = ftell(inFile);
  return 0;
}

void SolReader::RewindFile(){
  rewind(inFile);
  filePos = 0;
  numBlock = 0;
}

void SolReader::ScanNumBlock(){
  if( inFile == NULL ) return;
  if( feof(inFile) ) return;
  
  numBlock = 0;
  blockPos.clear();

  blockPos.push_back(0);

  while( ReadNextBlock(1) == 0){
    blockPos.push_back(filePos);
    printf("%u, %.2f%% %u/%u\n\033[A\r", numBlock, filePos*100./inFileSize, filePos, inFileSize);
  }

  totNumBlock = numBlock;
  numBlock = 0;
  isScanned = true;
  printf("\nScan complete: number of data Block : %u\n", totNumBlock);
  rewind(inFile);
  filePos = 0;

  //for( int i = 0; i < totNumBlock; i++){
  //  printf("%7d | %u \n", i, blockPos[i]);
  //}
  
}

#endif