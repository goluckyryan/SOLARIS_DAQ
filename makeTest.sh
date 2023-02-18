CC="g++"
COPTS="-fPIC -DLINUX -O2 -std=c++17 -lpthread"
CAENLIBS="-lCAEN_FELib"
CURLLIBS="-lcurl"

OBJS="ClassDigitizer2Gen.o influxdb.o"

#
#ALL = test   
#
################################################################

#
#test : test.cpp ClassDigitizer2Gen.o influxdb.o
#	$(CC) $(COPTS) $(OBJS) -o test test.cpp  $(CAENLIBS) $(CURLLIBS)
#
#ClassDigitizer2Gen.o : ClassDigitizer2Gen.cpp ClassDigitizer2Gen.h Event.h
#	$(CC) $(COPTS) -c ClassDigitizer2Gen.cpp $(CAENLIBS)
#
#influxdb.o : influxdb.cpp influxdb.h
#	$(CC) $(COPTS) -c influxdb.cpp $(CURLLIBS)



echo "------- influxdb.o"
${CC} ${COPTS} -c influxdb.cpp ${CURLLIBS}
echo "------- ClassDigitizer2Gen.o"
${CC} ${COPTS} -c ClassDigitizer2Gen.cpp ${CAENLIBS}
echo "------- test"
${CC} ${COPTS} ${OBJS} -o test test.cpp  ${CAENLIBS} ${CURLLIBS}
echo "------- windowID"
${CC} ${COPTS} -o windowID windowID.cpp  -lX11 -lpng
