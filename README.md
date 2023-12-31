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

# Additional function

## connect to analysis working directory
When the analysis path is set, it will do servera things

- save the expName.sh
- save Settings 
- try to load the Mapping.h in the working directory

## End run bash script

When run stop, it will run the bash script under the directory scripts/endRUnScript.h


# Required / Development enviroment

Ubuntu 22.04

CAEN_FELIB_v1.2.2 + (install first)

CAEN_DIG2_v1.5.3 + 

`sudo apt install qt6-base-dev libcurl4-openssl-dev libqt6charts6-dev`

Digitizer firmware V2745-dpp-pha-2022092903.cup

## Developer is using these at 2023-Oct-13

CAEN_FELIB_v1.2.5

CAEN_DIG2_v1.5.10

with these new API, Digitizer firmwares 

* V2745-dpp-pha-1G-2023091800.cup
* V2745-dpp-psd-1G-2023091901.cup
* V2740-dpp-pha-1G-2023091800.cup
* V2740-dpp-psd-1G-2023091901.cup 

are supported.

# Compile

## if *.pro does not exist
use `qmake6 -project ` to generate the *.pro

in the *.pro, add 

` QT += widgets`

` LIBS += -lcurl -lCAEN_FELib`

## if *.pro exist

run ` qmake6 *.pro` it will generate Makefile

then  ` make`

# Using the CAENDig2.h

The CAENDig2.h is not copied to system include path as the CAEN+FELib.h. But we can copy it from the source. In the caen_dig2-vXXXX folder, go to the include folder, copy the CAENDig2.h to /usr/local/include/.

This enable us to compile code with -lCAEN_Dig2. For example, we can use the following to get the CAEN Dig2 Library version.
```
 char version[16];
 CAENDig2_GetLibVersion(version);
 puts(version);
```

# Known Issues

- The "Trig." Rate in the Scaler does not included the coincident condition. This is related to the ChSavedEventCnt from the firmware.
- LVDSTrgMask cannot acess.
- The CoincidenceLengthT not loaded. 
- Sometime, the digitizer halt after sent the /cmd/armacquisition command. This is CAEN library problem.
- Event/Wave trig. Source cannot set as SWTrigger. 
- After update to CAEN_FELIB_v1.2.5 and CAEN_DIG2_v1.5.10, old firmware version before 202309XXXX is not supported.