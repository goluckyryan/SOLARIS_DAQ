#!/bin/bash -l

echo "################# End-Run Script"

#xterm -T endRunScript -hold -geometry 100x20+0+0  -sb  -sl 1000 -e "Process_Run" "lastRun"
#xterm -T endRunScript -geometry 100x20+0+0  -sb  -sl 1000 -e "source ~/Analysis/SOLARIS.sh; Process_Run lastRun 2 0"

#master data path
dataPath=~/ExpData/SOLARISDAQ/Haha/

#load the runID from expName
source $dataPath/data_raw/expName.sh

#format runID to 3 digit
runIDStr=$(printf "run%03d" $runID)

cp $dataPath/ExpSetup.txt  $dataPath/data_raw/$runIDStr/.

echo "################# done"