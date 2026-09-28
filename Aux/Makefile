CC=g++
CFLAG= -std=c++17 -O2
# CFLAG= -std=c++17 -g -O0
ROOTFLAG=`root-config --cflags --glibs`

OBJS = ClassDigitizer.o ClassInfluxDB.o

all: EventBuilder EventBuilderRaw

# Tools that need neither ROOT nor CAEN. Kept out of "all" so they build on any machine.
tools: testOnlineBuilder

testOnlineBuilder: testOnlineBuilder.cpp SolReader.h ../core/OnlineEventBuilder.cpp ../core/EventRing.h ../core/Analysis.h ../analyzers/Coincidence.cpp \
                   ../core/OnlineEventBuilder.h ../core/LeanHit.h ../core/BuiltHit.h ../core/RingBuffer.h ../core/Hit.h
	$(CC) $(CFLAG) -pthread -I../core testOnlineBuilder.cpp ../core/OnlineEventBuilder.cpp ../analyzers/*.cpp -o testOnlineBuilder

EventBuilder: EventBuilder.cpp SolReader.h ../core/Hit.h
	$(CC) $(CFLAG) EventBuilder.cpp -o EventBuilder ${ROOTFLAG}

EventBuilderRaw: EventBuilderRaw.cpp ../core/RawDecoder.h ../core/Hit.h
	$(CC) $(CFLAG) EventBuilderRaw.cpp -o EventBuilderRaw ${ROOTFLAG}

ClassDigitizer.o : ../ClassDigitizer.cpp ../ClassDigitizer.h ../RegisterAddress.h ../macro.h ../ClassData.h
		$(CC) $(COPTS) -c ../ClassDigitizer.cpp

ClassInfluxDB.o : ../core/ClassInfluxDB.cpp ../core/ClassInfluxDB.h 
		$(CC) $(COPTS) -c ../core/ClassInfluxDB.cpp -lcurl

test: test.cpp ../core/ClassDigitizer2Gen.o ../core/ClassInfluxDB.o ../core/RawDecoder.h ../core/Hit.h
	$(CC) $(CFLAG) test.cpp ../core/ClassDigitizer2Gen.o ../core/ClassInfluxDB.o -o test -lcurl -lCAEN_FELib -lX11

clean:
	-rm EventBuilder EventBuilderRaw test testOnlineBuilder
