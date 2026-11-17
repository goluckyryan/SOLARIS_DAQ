#!/bin/bash -l
# Run by MainWindow::StopACQ after every saving run:
#   endRunScript.sh <expDataPath> <rawDataPath> <runID>

echo "################# End-Run Script"

#xterm -T endRunScript -hold -geometry 100x20+0+0  -sb  -sl 1000 -e "Process_Run" "lastRun"
#xterm -T endRunScript -geometry 100x20+0+0  -sb  -sl 1000 -e "source ~/Analysis/SOLARIS.sh; Process_Run lastRun 2 0"

if [ $# -lt 3 ]; then
  echo "usage: $0 <expDataPath> <rawDataPath> <runID>"
  exit 1
fi

expDataPath=$1
rawDataPath=$2
runIDStr=$(printf "run%03d" "$3")

# the run's own folder when runs are saved in sub-folders, else the raw-data folder itself
runPath=$rawDataPath/$runIDStr
[ -d "$runPath" ] || runPath=$rawDataPath

if [ -f "$expDataPath/ExpSetup.txt" ]; then
  cp "$expDataPath/ExpSetup.txt" "$runPath/."
fi

echo "################# done"
