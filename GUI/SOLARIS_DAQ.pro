######################################################################
# SOLARIS DAQ - GUI Application
######################################################################

TEMPLATE = app
TARGET = SOLARIS_DAQ
DESTDIR = ..
INCLUDEPATH += . ../core

QT += core widgets charts printsupport

LIBS += -lcurl -lCAEN_FELib -lX11

#=========== for GDB debug
QMAKE_CXXFLAGS += -g  # for gdb debug
QMAKE_CXXFLAGS_RELEASE = -O0
QMAKE_CFLAGS_RELEASE = -O0

# Core headers (shared with broker)
HEADERS += ../core/ClassDigitizer2Gen.h \
           ../core/Hit.h \
           ../core/RawDecoder.h \
           ../core/RingBuffer.h \
           ../core/ClassInfluxDB.h \
           ../core/DigiParameters.h \
           ../core/macro.h

# GUI headers
HEADERS += mainwindow.h \
           digiSettingsPanel.h \
           scope.h \
           CustomThreads.h \
           CustomWidgets.h \
           SOLARISpanel.h \
           qcustomplot.h \
           Histogram1D.h \
           Histogram2D.h \
           SingleSpectra.h

# Core sources (shared with broker)
SOURCES += ../core/ClassDigitizer2Gen.cpp \
           ../core/ClassInfluxDB.cpp

# GUI sources
SOURCES += main.cpp \
           mainwindow.cpp \
           digiSettingsPanel.cpp \
           scope.cpp \
           SOLARISpanel.cpp \
           qcustomplot.cpp \
           SingleSpectra.cpp
