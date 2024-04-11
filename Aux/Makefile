CC=g++
CFLAG= -g
ROOTFLAG=`root-config --cflags --glibs`
all: EventBuilder

EventBuilder: EventBuilder.cpp SolReader.h ../Hit.h
	$(CC) $(CFLAG) EventBuilder.cpp -o EventBuilder ${ROOTFLAG}

test: test.cpp ../ClassDigitizer2Gen.o
	$(CC) $(CFLAG) test.cpp ../ClassDigitizer2Gen.o -o test -lcurl -lCAEN_FELib -lX11

clean:
	-rm EventBuilder