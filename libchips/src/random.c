#include "random.h"
#include <stdint.h>
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
uint8_t Prng_random3(Prng* self) {
  uint64_t val = Prng_random(self);
  return crush_to_3(val);
}
uint8_t Prng_random4(Prng* self) {
  return Prng_random(self) >> 29;
}
void Prng_permute3(Prng* self, int64_t arr[3]) {
  int64_t temp;
  uint64_t val = Prng_random(self);
  // Swap array index 1 with either 0 or 1
  uint8_t swap_idx = val >> 30;
  temp = arr[1];
  arr[1] = arr[swap_idx];
  arr[swap_idx] = temp;
  // Swap array index 2 with index 0, 1, or 2
  swap_idx = crush_to_3(val);
  temp = arr[2];
  arr[2] = arr[swap_idx];
  arr[swap_idx] = temp;
}

void Prng_permute4(Prng* self, int64_t arr[4]) {
  // NOTE: This uses different bit extraction than `permute3` or the `random*`
  // calls to not reuse the same bits.
  int64_t temp;
  uint64_t val = Prng_random(self);
  // Swap array index 1 with either 0 or 1
  uint8_t swap_idx = val >> 30;
  temp = arr[1];
  arr[1] = arr[swap_idx];
  arr[swap_idx] = temp;
  // Swap array index 2 with index 0, 1, or 2
  swap_idx = crush_to_3_using_different_bits(val);
  temp = arr[2];
  arr[2] = arr[swap_idx];
  arr[swap_idx] = temp;
  // Swap array index 3 with index 0, 1, 2, or 3
  swap_idx = (val >> 28) & 3;
  temp = arr[2];
  arr[2] = arr[swap_idx];
  arr[swap_idx] = temp;
}
