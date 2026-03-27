CC = g++
CFLAGS = -std=c++17 -O2 -Wall -I../core -I/usr/local/include
LDFLAGS = -lCAEN_FELib -lcurl -lzmq -lpthread

CORE = ../core
DIGI_OBJ = $(CORE)/ClassDigitizer2Gen.o
INFLUX_OBJ = $(CORE)/ClassInfluxDB.o

OUTDIR = ..

all: $(OUTDIR)/solaris-broker $(OUTDIR)/solaris-cli

#--- Core objects (shared, compile without Qt) ---
$(DIGI_OBJ): $(CORE)/ClassDigitizer2Gen.cpp $(CORE)/ClassDigitizer2Gen.h \
             $(CORE)/DigiParameters.h $(CORE)/macro.h $(CORE)/Hit.h \
             $(CORE)/RingBuffer.h $(CORE)/RawDecoder.h
	$(CC) $(CFLAGS) -c $< -o $@

$(INFLUX_OBJ): $(CORE)/ClassInfluxDB.cpp $(CORE)/ClassInfluxDB.h
	$(CC) $(CFLAGS) -c $< -o $@

#--- Broker objects ---
BrokerServer.o: BrokerServer.cpp BrokerServer.h BrokerProtocol.h
	$(CC) $(CFLAGS) -c $<

BrokerClient.o: BrokerClient.cpp BrokerClient.h BrokerProtocol.h
	$(CC) $(CFLAGS) -c $<

#--- Executables (output to root) ---
$(OUTDIR)/solaris-broker: solaris-broker.cpp BrokerServer.o $(DIGI_OBJ) $(INFLUX_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OUTDIR)/solaris-cli: solaris-cli.cpp BrokerClient.o $(DIGI_OBJ) $(INFLUX_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lreadline

clean:
	rm -f *.o $(CORE)/*.o $(OUTDIR)/solaris-broker $(OUTDIR)/solaris-cli

.PHONY: all clean
