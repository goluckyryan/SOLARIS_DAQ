#!/bin/bash
# Build the headless fill/replot race harness against the app's objects.
# Prereq: `make` has been run in the parent directory (objects + moc up to date).
set -e
cd "$(dirname "$0")/.."
APP=$(pwd)
CXX="g++ -pipe -g -O0 -Wall -Wextra -D_REENTRANT -DQT_NO_DEBUG -DQT_CHARTS_LIB -DQT_OPENGLWIDGETS_LIB -DQT_PRINTSUPPORT_LIB -DQT_WIDGETS_LIB -DQT_OPENGL_LIB -DQT_GUI_LIB -DQT_CORE_LIB"
INC="-I. -I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCharts -I/usr/include/x86_64-linux-gnu/qt6/QtOpenGLWidgets -I/usr/include/x86_64-linux-gnu/qt6/QtPrintSupport -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets -I/usr/include/x86_64-linux-gnu/qt6/QtOpenGL -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/lib/x86_64-linux-gnu/qt6/mkspecs/linux-g++"
LIBS="-lcurl -lCAEN_FELib -lX11 -ldl /usr/lib/x86_64-linux-gnu/libQt6Charts.so /usr/lib/x86_64-linux-gnu/libQt6OpenGLWidgets.so /usr/lib/x86_64-linux-gnu/libQt6PrintSupport.so /usr/lib/x86_64-linux-gnu/libQt6Widgets.so /usr/lib/x86_64-linux-gnu/libQt6OpenGL.so /usr/lib/x86_64-linux-gnu/libQt6Gui.so /usr/lib/x86_64-linux-gnu/libGLX.so /usr/lib/x86_64-linux-gnu/libOpenGL.so /usr/lib/x86_64-linux-gnu/libQt6Core.so -lpthread -lGLX -lOpenGL"

OBJS=$(ls *.o | grep -v "^main.o$" | grep -v "^moc_main.o$")

$CXX $INC -c tests/histRace.cpp -o tests/histRace.o
$CXX -o tests/histRace tests/histRace.o $OBJS $LIBS
$CXX $INC -c tests/cutMenu.cpp -o tests/cutMenu.o
$CXX -o tests/cutMenu tests/cutMenu.o $OBJS $LIBS
# live-board probes (READ-ONLY): built here so they cannot drift from the app's API unseen
$CXX $INC -c tests/probeStat.cpp -o tests/probeStat.o
$CXX -o tests/probeStat tests/probeStat.o $OBJS $LIBS
$CXX $INC -c tests/coincCheck.cpp -o tests/coincCheck.o
$CXX -o tests/coincCheck tests/coincCheck.o $OBJS $LIBS
echo "built tests/histRace tests/cutMenu tests/probeStat tests/coincCheck"
