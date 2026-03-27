#include <unistd.h>
#include <ctime>
#include <CAEN_FELib.h>
#include "../ClassDigitizer2Gen.h"
#include "../RawDecoder.h"

int main() {
  Digitizer2Gen * digi = new Digitizer2Gen();
  if(digi->OpenDigitizer("dig2://192.168.0.254/") != 0) return 1;
  uint64_t h = digi->GetHandle();

  // Setup raw endpoint directly
  uint64_t ep_handle, ep_folder;
  CAEN_FELib_GetHandle(h, "/endpoint/raw", &ep_handle);
  CAEN_FELib_GetParentHandle(ep_handle, NULL, &ep_folder);
  CAEN_FELib_SetValue(ep_folder, "/par/activeendpoint", "raw");
  CAEN_FELib_SetReadDataFormat(ep_handle,
    "[{\"name\":\"DATA\",\"type\":\"U8\",\"dim\":1},"
     "{\"name\":\"SIZE\",\"type\":\"SIZE_T\"},"
     "{\"name\":\"N_EVENTS\",\"type\":\"U32\"}]");

  uint8_t * data = new uint8_t[20*1024*1024];
  size_t dataSize;
  uint32_t n_events;

  CAEN_FELib_SendCommand(h, "/cmd/armacquisition");
  CAEN_FELib_SendCommand(h, "/cmd/swstartacquisition");

  // Read first 5 blobs
  for(int i = 0; i < 5; i++){
    int r = CAEN_FELib_ReadData(ep_handle, 1000, data, &dataSize, &n_events);
    if(r != 0){ printf("Read %d: timeout\n", i); continue; }

    printf("=== Blob %d: size=%zu bytes (%zu words), n_events=%u ===\n", i, dataSize, dataSize/8, n_events);

    // Print first few words
    size_t nWords = dataSize / 8;
    for(size_t w = 0; w < nWords && w < 10; w++){
      uint64_t word;
      memcpy(&word, data + w*8, 8);
      word = __builtin_bswap64(word);
      printf("  word[%zu] = 0x%016lX  bits[63:60]=0x%lX\n", w, word, (word>>60)&0xF);
    }

    // Decode
    RawDecoder dec;
    dec.LoadBlob(data, dataSize, digi->GetFPGAType());
    printf("  Decoded: %u physics hits, %zu stat updates\n",
           dec.GetDecodedCount(), dec.GetStatUpdates().size());
    RawDecoder::DecodedHit hit;
    int hitN = 0;
    while(dec.Next(hit) && hitN < 5){
      printf("  hit %d: ch=%d e=%u e_s=%u ts=%lu ft=%u\n", hitN, hit.channel, hit.energy, hit.energy_short, hit.timestamp, hit.fine_timestamp);
      hitN++;
    }
  }

  CAEN_FELib_SendCommand(h, "/cmd/SwStopAcquisition");
  CAEN_FELib_SendCommand(h, "/cmd/disarmacquisition");
  digi->CloseDigitizer();
  delete digi;
  return 0;
}
