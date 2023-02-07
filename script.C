#include "SolReader.h"
#include "TH1.h"
#include "TMath.h"
#include "TH2.h"
#include "TStyle.h"
#include "TCanvas.h"
#include "TGraph.h"


void script(){ 

  SolReader * reader = new SolReader("haha_000.sol");
  Event * evt = reader->evt;

  printf("----------file size: %u Byte\n", reader->GetFileSize());

  reader->ScanNumBlock();

  if( reader->GetTotalNumBlock() == 0 ) return;
  
  unsigned long startTime, endTime;
  reader->ReadBlock(0);
  startTime = evt->timestamp;
  reader->ReadBlock(reader->GetTotalNumBlock() - 1);
  endTime = evt->timestamp;

  double duration = double(endTime - startTime)*8./1e9;
  printf("============== %lu ns = %.4f sec.\n", (endTime - startTime)*8, duration);
  printf(" avarge rate (16ch): %f Hz\n", reader->GetTotalNumBlock()/duration/16);
  reader->RewindFile();


  TH1F * hid = new TH1F("hid", "hid", 64, 0, 64);
  TH1F * h1  = new TH1F("h1", "h1", duration, startTime, endTime);
  TH2F * h2  = new TH2F("h2", "h2", 1000, startTime, endTime, 1000, 0, reader->GetTotalNumBlock());
  TH1F * hTdiff  = new TH1F("hTdiff", "hTdiff", 400, 0, 200000);

  TGraph * g1 = new TGraph();

  uint64_t tOld = startTime;

  for( int i = 0; i < reader->GetTotalNumBlock() ; i++){
  //for( int i = 0; i < 8 ; i++){
    
    reader->ReadNextBlock();
    
    if( i < 8 ){
      printf("########################## nBlock : %u, %u/%u\n", reader->GetNumBlock(), 
                                                                reader->GetFilePos(), 
                                                                reader->GetFileSize());
      evt->PrintAll();
      //evt->PrintAllTrace();
    }

    hid->Fill(evt->channel);
    if( evt->channel == 0 ) h1->Fill(evt->timestamp);
    h2->Fill(evt->timestamp, i);
    
    if( i > 0 ){
      hTdiff->Fill(evt->timestamp - tOld);
      if( evt->timestamp < tOld) printf("-------- time not sorted.");
      tOld = evt->timestamp;
    }
    
    if( i == 0){
      for( int i = 0; i < evt->traceLenght; i++){
        g1->AddPoint(i*8, evt->analog_probes[0][i]);
      }
    }
  }

  gStyle->SetOptStat("neiou");

  TCanvas * canvas = new TCanvas("c1", "c1", 1200, 1200);
  canvas->Divide(2,2);
  canvas->cd(1); hid->Draw();
  canvas->cd(2); h1->SetMinimum(0); h1->Draw();
  canvas->cd(3); hTdiff->Draw();
  canvas->cd(4); g1->Draw("APl");
  //printf("reader traceLength : %lu \n", evt->traceLenght);

  /*
  for( int i = 0; i < evt->traceLenght; i++){

    printf("%4d| %d\n", i, evt->analog_probes[0][i]);

  }
  */

  evt = NULL;
  delete reader;

}