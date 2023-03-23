#ifndef MACRO_H
#define MACRO_H

#define MaxNumberOfDigitizer 20
#define DAQLockFile "DAQLock.dat"
#define PIDFile  "pid.dat"

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

#endif