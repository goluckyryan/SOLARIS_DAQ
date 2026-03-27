CC=g++
CFLAG= -std=c++17 -O2
# CFLAG= -std=c++17 -g -O0
ROOTFLAG=`root-config --cflags --glibs`

OBJS = ClassDigitizer.o ClassInfluxDB.o

all: EventBuilder EventBuilderRaw

EventBuilder: EventBuilder.cpp SolReader.h ../Hit.h
	$(CC) $(CFLAG) EventBuilder.cpp -o EventBuilder ${ROOTFLAG}

EventBuilderRaw: EventBuilderRaw.cpp ../RawDecoder.h ../Hit.h
	$(CC) $(CFLAG) EventBuilderRaw.cpp -o EventBuilderRaw ${ROOTFLAG}

ClassDigitizer.o : ../ClassDigitizer.cpp ../ClassDigitizer.h ../RegisterAddress.h ../macro.h ../ClassData.h
		$(CC) $(COPTS) -c ../ClassDigitizer.cpp

ClassInfluxDB.o : ../ClassInfluxDB.cpp ../ClassInfluxDB.h 
		$(CC) $(COPTS) -c ../ClassInfluxDB.cpp -lcurl

test: test.cpp ../ClassDigitizer2Gen.o ../ClassInfluxDB.o ../RawDecoder.h ../Hit.h
	$(CC) $(CFLAG) test.cpp ../ClassDigitizer2Gen.o ../ClassInfluxDB.o -o test -lcurl -lCAEN_FELib -lX11

clean:
	-rm EventBuilder EventBuilderRaw test
