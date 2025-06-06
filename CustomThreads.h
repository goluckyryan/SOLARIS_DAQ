
#ifndef CUSTOMTHREADS_H
#define CUSTOMTHREADS_H

#include <QThread>
#include <QMutex>

#include "macro.h"
#include "ClassDigitizer2Gen.h"

static QMutex digiMTX[MaxNumberOfDigitizer];

//^#===================================================== ReadData Thread
class ReadDataThread : public QThread {
  Q_OBJECT
public:
  ReadDataThread(Digitizer2Gen * dig, int digiID, QObject * parent = 0) : QThread(parent){ 
    this->digi = dig;
    this->ID = digiID;
    isSaveData = false;
    stop = false;
    // canSendMsg = true;
  }
  // void SuppressFileSizeMsg() {canSendMsg = false;}
  void Stop(){ this->stop = true;}
  void SetSaveData(bool onOff) {this->isSaveData = onOff;}
  void run(){
    // canSendMsg = true;
    stop = false;
    // clock_gettime(CLOCK_REALTIME, &ta);
    emit sendMsg("Digi-" + QString::number(digi->GetSerialNumber()) + " ReadDataThread started.");

    while(!stop){
      digiMTX[ID].lock();
      int ret = digi->ReadData();
      digiMTX[ID].unlock();

      if( ret == CAEN_FELib_Success){
        if( isSaveData) digi->SaveDataToFile();
      }else if(ret == CAEN_FELib_Stop){
        digi->ErrorMsg("ReadData Thread No more data");
        //emit endOfLastData();
        break;
      }else{
        //digi->ErrorMsg("ReadDataLoop()");
        digi->hit->ClearTrace();
      }
// 
      // if( isSaveData && canSendMsg ){
      //   clock_gettime(CLOCK_REALTIME, &tb);
      //   if( tb.tv_sec - ta.tv_sec > 2 ) {
      //     emit sendMsg("FileSize ("+ QString::number(digi->GetSerialNumber()) +"): " +  QString::number(digi->GetTotalFilesSize()/1024./1024.) + " MB");
      //     //emit checkFileSize();
      //     //double duration = tb.tv_nsec-ta.tv_nsec + tb.tv_sec*1e+9 - ta.tv_sec*1e+9;
      //     //printf("%4d, duration : %10.0f, %6.1f\n", readCount, duration, 1e9/duration);
      //     ta = tb;
      //   }
      // }
    }

    emit sendMsg("Digi-" + QString::number(digi->GetSerialNumber()) + " ReadDataThread stopped.");

  }
signals:
  void sendMsg(const QString &msg);
  //void endOfLastData();
  //void checkFileSize();
private:
  Digitizer2Gen * digi; 
  int ID;
  timespec ta, tb;
  // bool isSaveData, stop, canSendMsg;
  bool isSaveData, stop;
};

//^#======================================================= Timing Thread, for some action need to be done periodically
class TimingThread : public QThread {
  Q_OBJECT
public:
  TimingThread(QObject * parent = 0 ) : QThread(parent){
    waitTime = 20; // 20 x 100 milisec
    stop = false;
  }
  void Stop() { this->stop = true;}
  float GetWaitTimeinSec() const {return waitTime/10.;}
  void SetWaitTimeSec(float sec) {waitTime = sec * 10;}
  void run(){
    unsigned int count  = 0;
    stop = false;
    do{
      usleep(100000); // sleep for 100 ms
      count ++;
      if( count % waitTime == 0){
        emit TimeUp();
      }
    }while(!stop);
  }
signals:
  void TimeUp();
private:
  bool stop;
  unsigned int waitTime;
};

#endif
