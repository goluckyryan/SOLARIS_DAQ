# Architecture

The core digitizer control classes are independent from the UI classes

## Core digitizer class/files

- Event.h
- DigiParameters.h
- ClassDigitizer2Gen.h/cpp

The test.cpp is a demo code to use the ClassDigitizer2Gen.h/cpp.

## Auxillary classes

- influxdb.h/cpp

## UI classes/files

- main.cpp
- mainwindow.h/cpp
- digiSettingsPanel.h/cpp
- CustomWidget.h
- CustomThreads.h
- scope.h/cpp
- SOLARISpanel.h/cpp

## Other files

- makeTest
- test.cpp 
- script.C
- SolReader.h
- windowID.cpp

## Wiki

https://fsunuc.physics.fsu.edu/wiki/index.php/FRIB_SOLARIS_Collaboration

# Required / Development enviroment

Ubuntu 22.04

CAEN_DIG2_v1.5.3

CAEN_FELIB_v1.2.2

`sudo apt install qt6-base-dev libcurl4-openssl-dev libqt6charts6-dev`

Digitizer firmware V2745-dpp-pha-2022092903.cup

# Compile

## if *.pro does not exist
use `qmake6 -project ` to generate the *.pro

in the *.pro, add 

` QT += widgets`

` LIBS += -lcurl -lCAEN_FELib`

## if *.pro exist

run ` qmake6 *.pro` it will generate Makefile

then  ` make`