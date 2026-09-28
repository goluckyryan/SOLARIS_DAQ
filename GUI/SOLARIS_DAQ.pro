######################################################################
# SOLARIS DAQ - GUI Application
######################################################################

TEMPLATE = app
TARGET = SOLARIS_DAQ
DESTDIR = ..
INCLUDEPATH += . ../core ../broker

QT += core widgets charts printsupport

# -ldl: AnalysisPlugin dlopen()s the analysis .so files at runtime.
LIBS += -lcurl -lCAEN_FELib -lX11 -lzmq -ldl

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
           ../core/ClassElog.h \
           ../core/ClassElogTemplate.h \
           ../core/DigiParameters.h \
           ../core/DigiManager.h \
           ../core/macro.h \
           ../core/LeanHit.h \
           ../core/BuiltHit.h \
           ../core/EventRing.h \
           ../core/Analysis.h \
           ../core/AnalysisPlugin.h \
           ../core/OnlineEventBuilder.h

# Broker headers
HEADERS += ../broker/BrokerClient.h \
           ../broker/BrokerProtocol.h

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
           SingleSpectra.h \
           Analyzer.h

# Core sources (shared with broker)
SOURCES += ../core/ClassDigitizer2Gen.cpp \
           ../core/ClassInfluxDB.cpp \
           ../core/ClassElog.cpp \
           ../core/ClassElogTemplate.cpp \
           ../core/DigiManager.cpp \
           ../core/OnlineEventBuilder.cpp \
           ../core/AnalysisPlugin.cpp

# Broker sources
SOURCES += ../broker/BrokerClient.cpp

# GUI sources
SOURCES += main.cpp \
           mainwindow.cpp \
           digiSettingsPanel.cpp \
           scope.cpp \
           SOLARISpanel.cpp \
           qcustomplot.cpp \
           SingleSpectra.cpp \
           Analyzer.cpp

# Analyses are NOT compiled into the binary: they are built separately into .so files by
# analyzers/Makefile and dlopen()ed at runtime.
