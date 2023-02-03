# Required / Development enviroment

Ubuntu 22.04

CAEN_DIG2_v1.5.3

CAEN_FELIB_v1.2.2

`sudo apt install qt6-base-dev libcurl4-openssl-dev libqt6charts6-dev`

Digitizer firmware V2745-dpp-pha-2022092903.cup

# Compile

use `qmake6 -project ` to generate the *.pro

in the *.pro, add 

` QT += widgets`

` LIBS += -lcurl -lCAEN_FELib`

then run ` qmake6 *.pro` it will generate Makefile

then  ` make`