#ifndef MACRO_H
#define MACRO_H

#define DebugMode 0 //process check, when 1, print out all function call

#define MaxNumberOfDigitizer 20
#define DAQLockFile "DAQLock.dat"
#define PIDFile  "pid.dat"
#define SingleHistogramFillingTime 50  // ms between histogram fill refresh

#include <QString>

//^=================================
namespace Utility{
  /// either haha is "0xFFF" or "12435", convert to 10-base 
  static unsigned long TenBase(std::string haha){
    QString ans = QString::fromStdString(haha);
    unsigned long  mask = 0 ;
    if( ans.contains("0x")){
      bool ok;
      mask = ans.toULong(&ok, 16);
    }else{
      mask = ans.toULong();
    }
    return mask;
  }
}

//just to get rip of the warning;
const unsigned long ksjaldja = Utility::TenBase("0");

// if DebugMode is 1, define DebugPrint() to be printf(), else, DebugPrint() define nothing
#if DebugMode
#define DebugPrint(fmt, ...) printf(fmt "::%s\n",##__VA_ARGS__, __func__);
#else
#define DebugPrint(fmt, ...)
#endif


#endif