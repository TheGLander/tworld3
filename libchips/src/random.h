#include <stdint.h>

typedef struct Prng {
  uint64_t initial_seed;
  uint64_t value;
} Prng;

void Prng_init_random(Prng* self);
void Prng_init_seeded(Prng* self, uint64_t seed);
uint64_t Prng_random(Prng* self);
uint8_t Prng_random2(Prng* self);
uint8_t Prng_random3(Prng* self);
uint8_t Prng_random4(Prng* self);
void Prng_permute3(Prng* self, int64_t arr[3]);
void Prng_permute4(Prng* self, int64_t arr[4]);
