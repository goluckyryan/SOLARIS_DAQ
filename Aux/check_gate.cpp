#include <unistd.h>
#include "../ClassDigitizer2Gen.h"
int main() {
  Digitizer2Gen * digi = new Digitizer2Gen();
  if(digi->OpenDigitizer("dig2://192.168.0.254/") != 0) return 1;
  printf("GateOffsetT ch0: [%s]\n", digi->ReadValue("/ch/0/par/GateOffsetT", true).c_str());
  printf("GateOffsetS ch0: [%s]\n", digi->ReadValue("/ch/0/par/GateOffsetS", true).c_str());
  printf("GateLongLengthT ch0: [%s]\n", digi->ReadValue("/ch/0/par/GateLongLengthT", true).c_str());
  bool ok = digi->WriteValue("/ch/0/par/GateOffsetT", "12", true);
  printf("Write GateOffsetT=12: %s\n", ok ? "OK" : "FAILED");
  digi->CloseDigitizer();
  delete digi;
  return 0;
}
