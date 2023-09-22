#include "SolReader.h"
#include "TH1.h"
#include "TMath.h"
#include "TH2.h"
#include "TStyle.h"
#include "TCanvas.h"
#include "TGraph.h"


void script(std::string fileName){ 

  SolReader * reader = new SolReader(fileName);
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
  printf(" avarge rate : %f Hz\n", reader->GetTotalNumBlock()/duration);
  reader->RewindFile();


  TH1F * hid = new TH1F("hid", "Ch-ID", 64, 0, 64);
  TH1F * h1  = new TH1F("h1", "Rate [Hz]; time [s] ; Hz", ceil(duration)+2, startTime*8/1e9 - 1 , ceil(endTime*8/1e9) + 1);
  TH2F * h2  = new TH2F("h2", "Time vs Entry ; time [s] ; Entry", 1000, startTime*8/1e9, endTime*8/1e9 + 1, 1000, 0, reader->GetTotalNumBlock());

  TGraph * g1 = new TGraph();
  TGraph * g2 = new TGraph(); g2->SetLineColor(2);
  TGraph * ga = new TGraph(); ga->SetLineColor(4);
  TGraph * gb = new TGraph(); gb->SetLineColor(5);
  TGraph * gc = new TGraph(); gc->SetLineColor(6);
  TGraph * gd = new TGraph(); gd->SetLineColor(7);

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
    if( evt->channel == 0 ) h1->Fill(evt->timestamp*8/1e9);
    h2->Fill(evt->timestamp*8/1e9, i);
    
    
    if( i == 0){
      for( int i = 0; i < evt->traceLenght; i++){
        g1->AddPoint(i*8, evt->analog_probes[0][i]);
        g2->AddPoint(i*8, evt->analog_probes[1][i]);
        ga->AddPoint(i*8, 10000+5000*evt->digital_probes[0][i]);
        gb->AddPoint(i*8, 20000+5000*evt->digital_probes[1][i]);
        gc->AddPoint(i*8, 30000+5000*evt->digital_probes[2][i]);
        gd->AddPoint(i*8, 40000+5000*evt->digital_probes[3][i]);
      }
    }
  }

  gStyle->SetOptStat("neiou");

  TCanvas * canvas = new TCanvas("c1", "c1", 1200, 1200);
  canvas->Divide(2,2);
  canvas->cd(1); hid->Draw();
  canvas->cd(2); h1->SetMinimum(0); h1->Draw();
  canvas->cd(3); h2->Draw();
  canvas->cd(4); g1->Draw("APl"); 
  g1->GetYaxis()->SetRangeUser(0, 80000);
  g2->Draw("same"); 
  ga->Draw("same");
  gb->Draw("same");
  gc->Draw("same");
  gd->Draw("same");
  //printf("reader traceLength : %lu \n", evt->traceLenght);

  /*
  for( int i = 0; i < evt->traceLenght; i++){

    printf("%4d| %d\n", i, evt->analog_probes[0][i]);

  }
  */

  evt = NULL;
  delete reader;

}