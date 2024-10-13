#ifndef FORMAT_TWS_H
#define FORMAT_TWS_H

#include <stdint.h>

#include "formats.h"
#include "logic.h"

typedef struct TWSMetadata {
  uint16_t level_num;
  char password[4];
  uint8_t other_flags;
  Direction slide_direction;
  int8_t step_value;
  uint32_t prng_seed;
  uint32_t num_ticks;
  GameInput* inputs;
} TWSMetadata;

uint16_t TWSMetadata_get_level_num(TWSMetadata const* self);
void TWSMetadata_get_password(TWSMetadata const* self, char pass_buf[4]);
uint8_t TWSMetadata_get_flags(TWSMetadata const* self);
Direction TWSMetadata_get_slide_dir(TWSMetadata const* self);
int8_t TWSMetadata_get_step(TWSMetadata const* self);
uint32_t TWSMetadata_get_prng_seed(TWSMetadata const* self);
uint32_t TWSMetadata_get_length(TWSMetadata const* self);
GameInput const* TWSMetadata_get_inputs(TWSMetadata const* self);
GameInput TWSMetadata_get_input(TWSMetadata const* self, uint32_t tick_num);
// void TWSMetadata_set_input(TWSMetadata const* self, uint32_t tick_num, GameInput input);

typedef struct TWSSet {
  RulesetID ruleset;
  char* set_name;
  uint16_t recent_level;
  uint32_t solutions_n;
  uint32_t solutions_allocated;
  TWSMetadata* solutions;
} TWSSet;

RulesetID TWSSet_get_ruleset(TWSSet const* self);
char const* TWSSet_get_set_name(TWSSet const* self);
uint16_t TWSSet_get_recent_level(TWSSet const* self);
uint32_t TWSSet_get_solutions_n(TWSSet const* self);
TWSMetadata const* TWSSet_get_level_solution(TWSSet const* self, uint16_t level_num);
TWSMetadata const* TWSSet_get_solution_by_idx(TWSSet const* self, uint32_t idx);
uint32_t TWSSet_get_level_idx(TWSSet const* self, uint16_t level_num);

typedef TWSSet* TWSSetPtr;
DEFINE_RESULT(TWSSetPtr);

Result_TWSSetPtr parse_tws(uint8_t const* data, size_t data_len);
void TWSSet_free(TWSSet* self);

#endif //FORMAT_TWS_H
