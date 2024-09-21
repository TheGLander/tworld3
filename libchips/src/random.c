#include "random.h"
#include <stdint.h>
#include <string.h>
#include <time.h>

void Prng_init_seeded(Prng* self, uint64_t seed) {
  self->initial_seed = self->value = seed & 0x7FFFFFFFUL;
}
void Prng_init_random(Prng* self) {
  Prng_init_seeded(self, time(NULL));
  // Generate a few times to remove possible biases from the seeded value.
  Prng_random(self);
  Prng_random(self);
  Prng_random(self);
  Prng_random(self);
  Prng_random(self);
}
uint64_t Prng_random(Prng* self) {
  self->value = ((self->value * 1103515245UL) + 12345UL) & 0x7FFFFFFFUL;
  return self->value;
}
uint8_t Prng_random2(Prng* self) {
  return Prng_random(self) >> 30;
}
/**
 * Crush any number down to a 0, 1, 2
 */
static uint8_t crush_to_3(uint64_t val) {
  return (int)((3.0 * (val & 0x3FFFFFFFUL)) / (double)0x40000000UL);
}
/**
 * Crush any number down to a 0, 1, 2. Same as `crush_to_3`, but uses less bits
 * so that `permute4` can use the other bits for other permutations
 */
static uint8_t crush_to_3_using_different_bits(uint64_t val) {
  return (int)((3.0 * (val & 0x0FFFFFFFUL)) / (double)0x10000000UL);
}
//returns a value between 0 and 2
uint8_t Prng_random3(Prng* self) {
  uint64_t val = Prng_random(self);
  return crush_to_3(val);
}

//returns a value between 0 and 3
uint8_t Prng_random4(Prng* self) {
  return Prng_random(self) >> 29;
}

void Prng_permute3(Prng* self, void* arr, size_t const size) {
  unsigned char* arr_c = arr;
  unsigned char temp[32] = {0}; //Note: this means that this func only works with value sizes up to 32 (which is total overkill)
  uint64_t val = Prng_random(self);
  // Swap array index 1 with either 0 or 1
  uint8_t swap_idx = val >> 30;
  memcpy(&temp, arr_c + size * 1, size);
  memcpy(arr_c + size * 1, arr_c + size * swap_idx, size);
  memcpy(arr_c + size * swap_idx, temp, size);
  // Swap array index 2 with index 0, 1, or 2
  swap_idx = crush_to_3(val);
  memcpy(&temp, arr_c + size * 2, size);
  memcpy(arr_c + size * 2, arr_c + size * swap_idx, size);
  memcpy(arr_c + size * swap_idx, temp, size);
}

void Prng_permute4(Prng* self, void* arr, size_t const size) {
  // NOTE: This uses different bit extraction than `permute3` or the `random*`
  // calls to not reuse the same bits.
  unsigned char* arr_c = arr;
  unsigned char temp[32] = {0}; //Note: this means that this func only works with value sizes up to 32 (which is total overkill)
  uint64_t val = Prng_random(self);
  // Swap array index 1 with either 0 or 1
  uint8_t swap_idx = val >> 30;
  memcpy(&temp, arr_c + size * 1, size);
  memcpy(arr_c + size * 1, arr_c + size * swap_idx, size);
  memcpy(arr_c + size * swap_idx, temp, size);
  // Swap array index 2 with index 0, 1, or 2
  swap_idx = crush_to_3_using_different_bits(val);
  memcpy(&temp, arr_c + size * 2, size);
  memcpy(arr_c + size * 2, arr_c + size * swap_idx, size);
  memcpy(arr_c + size * swap_idx, temp, size);
  // Swap array index 3 with index 0, 1, 2, or 3
  swap_idx = (val >> 28) & 3;
  memcpy(&temp, arr_c + size * 3, size);
  memcpy(arr_c + size * 3, arr_c + size * swap_idx, size);
  memcpy(arr_c + size * swap_idx, temp, size);
}
