#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <cstddef>
#include <cstring>

template<typename T, size_t N>
class RingBuffer {
  T buffer[N];
  unsigned long writeIndex;

public:
  RingBuffer() : writeIndex(0) { memset(buffer, 0, sizeof(buffer)); }

  void push(const T& val) { buffer[writeIndex % N] = val; writeIndex++; }
  T    at(unsigned long idx) const { return buffer[idx % N]; }
  unsigned long index() const { return writeIndex; }
  constexpr size_t size() const { return N; }
  void clear() { writeIndex = 0; memset(buffer, 0, sizeof(buffer)); }
};

#endif
