
#ifndef CUSTOMTHREADS_H
#define CUSTOMTHREADS_H

#include <QThread>
#include <QMutex>

#include <atomic>

#include "macro.h"
#include "ClassDigitizer2Gen.h"

extern QMutex digiMTX[MaxNumberOfDigitizer];

/// BOTH threads below override run() and never start an event loop, so QThread::quit() does
/// NOTHING to them -- it only asks an event loop to return. Stop() is the only way to end either
/// one; wait() then joins. Calls to quit() on these two used to be sprinkled around the shutdown
/// paths, reading as if they helped, and have been removed. Do not add them back.
/// (SingleSpectra's workerThread and the Analyzer's build/analysis threads are plain QThreads with
/// real event loops and worker objects moved onto them -- those do need quit().)

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
        
      int ret = digi->ReadData();
      
      if( ret == CAEN_FELib_Stop ){
        /// hit is only allocated by SetDataFormat(), which a board with no CAEN handle never
        /// gets through. Without this guard, "never return Stop" would be a memory-safety
        /// invariant of ReadData() rather than a policy.
        if( digi->hit ) digi->hit->ClearTrace();
      }

      if( isSaveData && ret == CAEN_FELib_Success ){
        digi->SaveDataToFile();
      }

      if( ret == CAEN_FELib_Stop ){
        digi->ErrorMsg("ReadData Thread No more data", ret);
        break;
      }

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
  /// Atomic: Stop() and SetSaveData() are called from the GUI thread while run() reads them in its
  /// loop. As plain bools that is a data race, and nothing stops the compiler hoisting the load of
  /// `stop` out of the loop entirely.
  std::atomic<bool> isSaveData;
  std::atomic<bool> stop;
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
  /// Clamped to at least one tick: waitTime is the modulus in run(), so zero is a division by
  /// zero, i.e. SIGFPE. Anything under 0.1 s rounds to zero here.
  void SetWaitTimeSec(float sec) {
    long ticks = (long)(sec * 10);
    waitTime = (unsigned int)( ticks < 1 ? 1 : ticks );
  }
  void run(){
    unsigned int count  = 0;
    stop = false;
    do{
      usleep(100000); // sleep for 100 ms
      count ++;
      const unsigned int period = waitTime.load(std::memory_order_relaxed);
      if( period > 0 && count % period == 0){
        emit TimeUp();
      }
    }while(!stop);
  }
signals:
  void TimeUp();
private:
  /// Both written from the GUI thread and read here; see the note on ReadDataThread.
  std::atomic<bool> stop;
  std::atomic<unsigned int> waitTime;
};

#endif
