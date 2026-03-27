#ifndef MACRO_H
#define MACRO_H

#define DebugMode 0 //process check, when 1, print out all function call

#define MaxNumberOfDigitizer 20
#define DAQLockFile "DAQLock.dat"
#define PIDFile  "pid.dat"
#define SingleHistogramFillingTime 50  // ms between histogram fill refresh

#include <string>

//^=================================
namespace Utility{
  /// either haha is "0xFFF" or "12435", convert to 10-base
  static unsigned long TenBase(std::string haha){
    if( haha.find("0x") != std::string::npos || haha.find("0X") != std::string::npos ){
      return std::stoul(haha, nullptr, 16);
    }
    return std::stoul(haha);
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