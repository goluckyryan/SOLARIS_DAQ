#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <atomic>
#include <cstddef>
#include <cstring>

/// Single-producer / multi-consumer ring buffer.
/// The producer (DAQ thread) is the only writer of writeIndex; it is released on advance()/push()
/// and acquired by index() so that a consumer seeing index N is guaranteed to see the contents of
/// slot N-1. On x86-64 both compile to a plain mov, so the DAQ hot path pays nothing.
/// A consumer must still detect being lapped: read index(), copy the slot with at(), re-read index(),
/// and discard the copy if the producer advanced by N or more in between.
template<typename T, size_t N>
class RingBuffer {
  T buffer[N];
  std::atomic<unsigned long> writeIndex;

public:
  RingBuffer() : writeIndex(0) { memset(buffer, 0, sizeof(buffer)); }

  void push(const T& val) {
    unsigned long w = writeIndex.load(std::memory_order_relaxed);
    buffer[w % N] = val;
    writeIndex.store(w + 1, std::memory_order_release);
  }
  T    at(unsigned long idx) const { return buffer[idx % N]; }
  const T& ref(unsigned long idx) const { return buffer[idx % N]; }
  T& nextSlot() { return buffer[writeIndex.load(std::memory_order_relaxed) % N]; }
  /// plain load+store, NOT fetch_add: there is only one producer, so no read-modify-write is
  /// needed and we avoid a lock-prefixed instruction in the per-event DAQ path.
  void advance() {
    writeIndex.store(writeIndex.load(std::memory_order_relaxed) + 1, std::memory_order_release);
  }
  unsigned long index() const { return writeIndex.load(std::memory_order_acquire); }
  constexpr size_t size() const { return N; }
  void clear() { memset(buffer, 0, sizeof(buffer)); writeIndex.store(0, std::memory_order_release); }
};

#endif
