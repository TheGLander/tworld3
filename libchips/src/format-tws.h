#ifndef FORMAT_TWS_H
#define FORMAT_TWS_H

#include <stdint.h>

#include "formats.h"
#include "logic.h"

typedef struct GameInputList { // todo: get a proper vector style generic thing going and port this to it
  GameInput* inputs;
  size_t count;
  size_t allocated;
} GameInputList;
DEFINE_RESULT(GameInputList);

typedef struct CompressedInputList {
  uint8_t* bytes;
  size_t count;
} CompressedInputList;

typedef struct TWSMetadata {
  uint16_t level_num;
  char password[4];
  uint8_t other_flags;
  Direction rff_dir;
  int8_t init_step_parity;
  uint32_t prng_seed;
  uint32_t num_ticks;
  CompressedInputList compressed_inputs;
} TWSMetadata;

uint16_t TWSMetadata_get_level_num(TWSMetadata const* self);
void TWSMetadata_get_password(TWSMetadata const* self, char pass_buf[4]);
uint8_t TWSMetadata_get_flags(TWSMetadata const* self);
Direction TWSMetadata_get_slide_dir(TWSMetadata const* self);
int8_t TWSMetadata_get_step(TWSMetadata const* self);
uint32_t TWSMetadata_get_prng_seed(TWSMetadata const* self);
uint32_t TWSMetadata_get_length(TWSMetadata const* self);
Result_GameInputList TWSMetadata_prepare_inputs(TWSMetadata const* self); // caller owns the resulting input list
void TWSMetadata_free(TWSMetadata* self);

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

GameInputList GameInputList_new(size_t initial_size);
void GameInputList_free(GameInputList* self);
void GameInputList_shrink(GameInputList* self);
void GameInputList_resize(GameInputList* self, size_t new_size);
void GameInputList_append(GameInputList* self, GameInput input);
GameInput GameInputList_get_input(GameInputList const* self, size_t tick);

#endif //FORMAT_TWS_H
