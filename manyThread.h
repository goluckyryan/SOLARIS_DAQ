
#ifndef MANYTHREADS_H
#define MANYTHREADS_H

#include <QThread>
#include <QMutex>

#include "ClassDigitizer2Gen.h"

static QMutex digiMTX;

//^#===================================================== ReadData Thread
class ReadDataThread : public QThread {
  Q_OBJECT
public:
  ReadDataThread(Digitizer2Gen * dig, QObject * parent = 0) : QThread(parent){ 
    this->digi = dig;
    isScopeRun = false;
  }
  void SetScopeRun(bool onOff) {this->isScopeRun = onOff;}
  void run(){
    clock_gettime(CLOCK_REALTIME, &ta);
    while(true){
      digiMTX.lock();
      int ret = digi->ReadData();
      digiMTX.unlock();

      if( ret == CAEN_FELib_Success){
        if( !isScopeRun) digi->SaveDataToFile();
      }else if(ret == CAEN_FELib_Stop){
        digi->ErrorMsg("No more data");
        break;
      }else{
        //digi->ErrorMsg("ReadDataLoop()");
        digi->evt->ClearTrace();
      }

      if( !isScopeRun ){
        clock_gettime(CLOCK_REALTIME, &tb);
        if( tb.tv_sec - ta.tv_sec > 2 ) {
          emit sendMsg("FileSize : " +  QString::number(digi->GetFileSize()/1024./1024.) + " MB");
          
          //double duration = tb.tv_nsec-ta.tv_nsec + tb.tv_sec*1e+9 - ta.tv_sec*1e+9;
          //printf("%4d, duration : %10.0f, %6.1f\n", readCount, duration, 1e9/duration);
          ta = tb;
        }
      }
    }
  }
signals:
  void sendMsg(const QString &msg);
private:
  Digitizer2Gen * digi; 
  timespec ta, tb;
  bool isScopeRun;
};

//^#===================================================== UpdateTrace Thread
class UpdateTraceThread : public QThread {
  Q_OBJECT
public:
  UpdateTraceThread(QObject * parent = 0) : QThread(parent){
    waitTime = 2;
    stop = false;
  }
  void Stop() {this->stop = true;}
  void run(){
    unsigned int count = 0;
    stop = false;
    do{
      usleep(100000);
      count ++;
      if( count % waitTime == 0){
        emit updateTrace();
      }
    }while(!stop);
  }
signals:
  void updateTrace();

private:
  bool stop;
  unsigned int waitTime; //100 of milisec
};

//^#======================================================= Scalar Thread
class ScalarThread : public QThread {
  Q_OBJECT
public:
  ScalarThread(QObject * parent = 0 ) : QThread(parent){
    waitTime = 20; // 10 x 100 milisec
    stop = false;
  }
  void Stop() { this->stop = true;}
  unsigned int GetWaitTimeinSec() const {return waitTime/10;}
  void run(){
    unsigned int count  = 0;
    stop = false;
    do{
      usleep(100000);
      count ++;
      if( count % waitTime == 0){
        emit updataScalar();
      }
    }while(!stop);
  }
signals:
  void updataScalar();
private:
  bool stop;
  unsigned int waitTime;
};

#endif