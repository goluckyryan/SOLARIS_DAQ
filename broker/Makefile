CC = g++
CFLAGS = -std=c++17 -O2 -Wall -I.. -I/usr/local/include
LDFLAGS = -lCAEN_FELib -lcurl -lzmq -lpthread

PARENT = ..
DIGI_OBJ = $(PARENT)/ClassDigitizer2Gen.o
INFLUX_OBJ = $(PARENT)/ClassInfluxDB.o

all: solaris-broker solaris-cli

#--- Parent objects (compile without Qt) ---
$(DIGI_OBJ): $(PARENT)/ClassDigitizer2Gen.cpp $(PARENT)/ClassDigitizer2Gen.h \
             $(PARENT)/DigiParameters.h $(PARENT)/macro.h $(PARENT)/Hit.h \
             $(PARENT)/RingBuffer.h $(PARENT)/RawDecoder.h
	$(CC) $(CFLAGS) -c $< -o $@

$(INFLUX_OBJ): $(PARENT)/ClassInfluxDB.cpp $(PARENT)/ClassInfluxDB.h
	$(CC) $(CFLAGS) -c $< -o $@

#--- Broker objects ---
BrokerServer.o: BrokerServer.cpp BrokerServer.h BrokerProtocol.h
	$(CC) $(CFLAGS) -c $<

BrokerClient.o: BrokerClient.cpp BrokerClient.h BrokerProtocol.h
	$(CC) $(CFLAGS) -c $<

#--- Executables ---
solaris-broker: solaris-broker.cpp BrokerServer.o $(DIGI_OBJ) $(INFLUX_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

solaris-cli: solaris-cli.cpp BrokerClient.o $(DIGI_OBJ) $(INFLUX_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lreadline

clean:
	rm -f *.o solaris-broker solaris-cli

.PHONY: all clean
