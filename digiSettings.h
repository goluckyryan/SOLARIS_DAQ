#ifndef DigiSettings_H
#define DigiSettings_H

#include <QWidget>
#include "ClassDigitizer2Gen.h"

class DigiSettings : public QWidget{
  Q_OBJECT

public:
  DigiSettings(Digitizer2Gen * digi, unsigned short nDigi, QWidget * parent = nullptr);
  ~DigiSettings();

private slots:

signals:

private:
  
  Digitizer2Gen * digi;
  unsigned short nDigi;


};



#endif