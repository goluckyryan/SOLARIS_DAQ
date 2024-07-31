CC=g++
CFLAG= -O2
#CFLAG= -g -O0
ROOTFLAG=`root-config --cflags --glibs`

OBJS = ClassDigitizer.o ClassInfluxDB.o

all: EventBuilder

EventBuilder: EventBuilder.cpp SolReader.h ../Hit.h
	$(CC) $(CFLAG) EventBuilder.cpp -o EventBuilder ${ROOTFLAG}

ClassDigitizer.o : ../ClassDigitizer.cpp ../ClassDigitizer.h ../RegisterAddress.h ../macro.h ../ClassData.h
		$(CC) $(COPTS) -c ../ClassDigitizer.cpp

ClassInfluxDB.o : ../ClassInfluxDB.cpp ../ClassInfluxDB.h 
		$(CC) $(COPTS) -c ../ClassInfluxDB.cpp -lcurl

test: test.cpp ../ClassDigitizer2Gen.o ../ClassInfluxDB.o
	$(CC) $(CFLAG) test.cpp ../ClassDigitizer2Gen.o ../ClassInfluxDB.o -o test -lcurl -lCAEN_FELib -lX11

clean:
	-rm EventBuilder test
