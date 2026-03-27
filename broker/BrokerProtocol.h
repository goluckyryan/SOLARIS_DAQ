#ifndef BROKER_PROTOCOL_H
#define BROKER_PROTOCOL_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

//=== Default ZMQ endpoints
constexpr const char* DEFAULT_CMD_ENDPOINT_SERVER = "tcp://*:5555";
constexpr const char* DEFAULT_PUB_ENDPOINT_SERVER = "tcp://*:5556";
constexpr const char* DEFAULT_CMD_ENDPOINT_CLIENT = "tcp://localhost:5555";
constexpr const char* DEFAULT_PUB_ENDPOINT_CLIENT = "tcp://localhost:5556";

constexpr float DEFAULT_SCALAR_INTERVAL_SEC = 2.0f;

//=== Request message types (client → broker, REQ socket)
enum MsgType : uint8_t {
  // Digitizer management
  REQ_OPEN_DIGI        = 0x01,
  REQ_CLOSE_DIGI       = 0x02,
  REQ_LIST_DIGI        = 0x03,

  // Parameter access
  REQ_READ_VALUE       = 0x10,
  REQ_WRITE_VALUE      = 0x12,
  REQ_SEND_COMMAND     = 0x14,

  // Acquisition control
  REQ_START_ACQ        = 0x20,
  REQ_STOP_ACQ         = 0x21,
  REQ_SET_DATA_FORMAT  = 0x22,
  REQ_GET_ACQ_STATUS   = 0x23,

  // File I/O (broker-side saving)
  REQ_OPEN_FILE        = 0x30,
  REQ_CLOSE_FILE       = 0x31,
  REQ_SET_SAVE_DATA    = 0x32,
  REQ_GET_FILE_STATUS  = 0x33,

  // Settings
  REQ_READ_ALL_SETTINGS  = 0x40,
  REQ_SAVE_SETTINGS_FILE = 0x41,
  REQ_LOAD_SETTINGS_FILE = 0x42,

  // Info
  REQ_GET_DIGI_INFO    = 0x50,

  // Lifecycle
  REQ_PING             = 0xF0,
  REQ_SHUTDOWN         = 0xFF,
};

//=== Response message types (broker → client, REP socket)
enum RspType : uint8_t {
  RSP_OK               = 0x80,
  RSP_ERROR            = 0x81,
  RSP_DIGI_LIST        = 0x82,
  RSP_VALUE            = 0x83,
  RSP_DIGI_INFO        = 0x84,
  RSP_ACQ_STATUS       = 0x85,
  RSP_FILE_STATUS      = 0x86,
  RSP_PONG             = 0xF0,
};

//=== Broadcast message types (broker → all clients, PUB socket)
enum PubType : uint8_t {
  PUB_SCALAR           = 0xC0,
  PUB_HIT_SUMMARY      = 0xC1,
  PUB_TRACE            = 0xC2,
  PUB_STATUS_CHANGE    = 0xC3,
  PUB_LOG_MESSAGE      = 0xC4,
};

//=== Status change events (payload of PUB_STATUS_CHANGE)
enum StatusEvent : uint8_t {
  EVT_ACQ_STARTED      = 0x01,
  EVT_ACQ_STOPPED      = 0x02,
  EVT_FILE_OPENED      = 0x03,
  EVT_FILE_CLOSED      = 0x04,
  EVT_DIGI_OPENED      = 0x05,
  EVT_DIGI_CLOSED      = 0x06,
};

//=== Special index meaning "all digitizers"
constexpr uint8_t ALL_DIGITIZERS = 0xFF;

//^======================================================
// Binary serialization helpers (little-endian, inline)
//^======================================================

inline void PackU8(std::vector<uint8_t>& buf, uint8_t val) {
  buf.push_back(val);
}

inline void PackU16(std::vector<uint8_t>& buf, uint16_t val) {
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

inline void PackU32(std::vector<uint8_t>& buf, uint32_t val) {
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

inline void PackU64(std::vector<uint8_t>& buf, uint64_t val) {
  for (int i = 0; i < 8; i++) {
    buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
  }
}

inline void PackFloat(std::vector<uint8_t>& buf, float val) {
  uint32_t u;
  memcpy(&u, &val, 4);
  PackU32(buf, u);
}

inline void PackString(std::vector<uint8_t>& buf, const std::string& s) {
  PackU16(buf, static_cast<uint16_t>(s.size()));
  buf.insert(buf.end(), s.begin(), s.end());
}

// Pack message header: msgType + flags(0)
inline void PackHeader(std::vector<uint8_t>& buf, uint8_t msgType) {
  PackU8(buf, msgType);
  PackU8(buf, 0); // flags reserved
}

//--- Unpack helpers (read from pointer, advance offset) ---

inline uint8_t UnpackU8(const uint8_t* p, size_t& off) {
  return p[off++];
}

inline uint16_t UnpackU16(const uint8_t* p, size_t& off) {
  uint16_t val = static_cast<uint16_t>(p[off]) |
                 (static_cast<uint16_t>(p[off + 1]) << 8);
  off += 2;
  return val;
}

inline uint32_t UnpackU32(const uint8_t* p, size_t& off) {
  uint32_t val = static_cast<uint32_t>(p[off]) |
                 (static_cast<uint32_t>(p[off + 1]) << 8) |
                 (static_cast<uint32_t>(p[off + 2]) << 16) |
                 (static_cast<uint32_t>(p[off + 3]) << 24);
  off += 4;
  return val;
}

inline uint64_t UnpackU64(const uint8_t* p, size_t& off) {
  uint64_t val = 0;
  for (int i = 0; i < 8; i++) {
    val |= static_cast<uint64_t>(p[off + i]) << (i * 8);
  }
  off += 8;
  return val;
}

inline float UnpackFloat(const uint8_t* p, size_t& off) {
  uint32_t u = UnpackU32(p, off);
  float val;
  memcpy(&val, &u, 4);
  return val;
}

inline std::string UnpackString(const uint8_t* p, size_t& off) {
  uint16_t len = UnpackU16(p, off);
  std::string s(reinterpret_cast<const char*>(p + off), len);
  off += len;
  return s;
}

// Unpack header: returns msgType, advances offset past flags
inline uint8_t UnpackHeader(const uint8_t* p, size_t& off) {
  uint8_t msgType = UnpackU8(p, off);
  UnpackU8(p, off); // skip flags
  return msgType;
}

#endif // BROKER_PROTOCOL_H
