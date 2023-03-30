#!/bin/bash -l

echo "################# end Run Script"

#xterm -T endRunScript -hold -geometry 100x20+0+0  -sb  -sl 1000 -e "Process_Run" "lastRun"
xterm -T endRunScript -geometry 100x20+0+0  -sb  -sl 1000 -e "source ~/Analysis/SOLARIS.sh; Process_Run lastRun 2 0"

echo "################# done"