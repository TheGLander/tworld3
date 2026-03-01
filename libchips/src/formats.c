#include "formats.h"

uint16_t read_uint16_le(uint8_t const* data) {
  return data[0] | (data[1] << 8);
}

uint32_t read_uint32_le(uint8_t const* data) {
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

uint64_t read_uint64_le(uint8_t const* data) {
  return (uint64_t) data[0] | ((uint64_t) data[1] << 8) | ((uint64_t) data[2] << 16) | ((uint64_t) data[3] << 24) |
    ((uint64_t) data[4] << 32) | ((uint64_t) data[5] << 40) | ((uint64_t) data[6] << 48) | ((uint64_t) data[7] << 56);
}
