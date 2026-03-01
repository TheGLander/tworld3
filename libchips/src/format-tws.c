#include "format-tws.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint16_t TWSMetadata_get_level_num(TWSMetadata const* self) {
  return self->level_num;
}

void TWSMetadata_get_password(TWSMetadata const* self, char pass_buf[4]) {
  memcpy(pass_buf, self->password, 4);
}

uint8_t TWSMetadata_get_flags(TWSMetadata const* self) {
  return self->other_flags;
}

Direction TWSMetadata_get_slide_dir(TWSMetadata const* self) {
  return self->rff_dir;
}

int8_t TWSMetadata_get_step(TWSMetadata const* self) {
  return self->init_step_parity;
}

uint32_t TWSMetadata_get_prng_seed(TWSMetadata const* self) {
  return self->prng_seed;
}

uint32_t TWSMetadata_get_length(TWSMetadata const* self) {
  return self->num_ticks;
}

Result_GameInputList TWSMetadata_prepare_inputs(TWSMetadata const* self) {
  if (self->compressed_inputs.bytes == NULL) {
    return res_err(GameInputList, "Solution has no inputs");
  }

  static_assert(((GameInput) DIRECTION_NIL) == 0);
  GameInputList input_list = GameInputList_new(self->num_ticks);
  uint32_t tick = 0;
  GameInput const input_lookup[] = {DIRECTION_NORTH, DIRECTION_WEST, DIRECTION_SOUTH, DIRECTION_EAST,
    DIRECTION_NORTH | DIRECTION_WEST, DIRECTION_SOUTH | DIRECTION_WEST, DIRECTION_NORTH | DIRECTION_EAST,
    DIRECTION_SOUTH | DIRECTION_EAST};
  size_t size = self->compressed_inputs.count;
  uint8_t const* data = self->compressed_inputs.bytes;
  while (size) {
    uint32_t time = 0;
    GameInput input;

    uint8_t first_byte = *data;
    data += 1;
    size -= 1;
    if ((first_byte & 0b11) == 0b00) {
      input = input_lookup[(first_byte >> 2) & 0b11];
      GameInput input2 = input_lookup[(first_byte >> 4) & 0b11];
      GameInput input3 = input_lookup[(first_byte >> 6) & 0b11];

      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, input);
      tick += 4;
      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, input2);
      tick += 4;
      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, DIRECTION_NIL);
      GameInputList_append(&input_list, input3);
      tick += 4;
    } else {
      if ((first_byte & 0b11) == 0b01) {
        time = first_byte >> 5;
        input = input_lookup[(first_byte >> 2) & 0b111];
      } else if ((first_byte & 0b11) == 0b10) {
        uint8_t second_byte = *data;
        data += 1;
        size -= 1;
        time = second_byte << 3 | first_byte >> 5;
        input = input_lookup[(first_byte >> 2) & 0b111];
      } else /*if ((first_byte & 0b11) == 0b11)*/ {
        if (!(first_byte & 0b10000)) {
          uint8_t second_byte = data[0];
          uint8_t third_byte = data[1];
          uint8_t fourth_byte = data[2];
          data += 3;
          size -= 3;
          input = input_lookup[(first_byte >> 2) & 0b11];
          time = ((fourth_byte & 0b00001111) << 19 | third_byte << 11 | second_byte << 3 | first_byte >> 5);
        } else {
          uint8_t num_bytes = ((first_byte >> 2) & 0b11) + 1;
          uint8_t bytes[5] = {0, 0, 0, 0, 0};
          bytes[0] = first_byte;
          memcpy(bytes + 1, data, num_bytes);
          data += num_bytes;
          size -= num_bytes;
          num_bytes += 1;
          input = ((bytes[1] & 0b00111111) << 3) | bytes[0] >> 5;
          time = (bytes[4] & 0b00011111) << 18 | bytes[3] << 10 | bytes[2] << 2 | bytes[1] >> 6;
        }
      }
      for (uint32_t i = 0; i < time; i += 1) {
        GameInputList_append(&input_list, DIRECTION_NIL);
      }
      GameInputList_append(&input_list, input);
      tick += time + 1;
    }
  }
  if (input_list.count < TWSMetadata_get_length(self)) {
    GameInputList_resize(&input_list, TWSMetadata_get_length(self));
  } else {
    GameInputList_shrink(&input_list);
  }
  return res_val(GameInputList, input_list);
}

void TWSMetadata_free(TWSMetadata* self) {
  free(self->compressed_inputs.bytes);
}

RulesetID TWSSet_get_ruleset(TWSSet const* self) {
  return self->ruleset;
}

char const* TWSSet_get_set_name(TWSSet const* self) {
  return self->set_name;
}

uint16_t TWSSet_get_recent_level(TWSSet const* self) {
  return self->recent_level;
}

uint32_t TWSSet_get_solutions_n(TWSSet const* self) {
  return self->solutions_n;
}

TWSMetadata const* TWSSet_get_level_solution(TWSSet const* self, uint16_t level_num) {
  for (uint32_t i = 0; i < self->solutions_n; i += 1) {
    if (self->solutions[i].level_num == level_num) {
      return &self->solutions[i];
    }
  }
  return NULL;
}

TWSMetadata const* TWSSet_get_solution_by_idx(TWSSet const* self, uint32_t idx) {
  return &self->solutions[idx];
}

uint32_t TWSSet_get_level_idx(TWSSet const* self, uint16_t level_num) {
  return TWSSet_get_level_solution(self, level_num) - self->solutions;
}

void TWSSet_free(TWSSet* self) {
  if (self == NULL)
    return;
  if (self->set_name != NULL)
    free(self->set_name);
  if (self->solutions) {
    for (uint32_t i = 0; i < self->solutions_n; i += 1) {
      TWSMetadata_free(&self->solutions[i]);
    }
    free(self->solutions);
  }
  free(self);
}

static void TWSSet_add_level(TWSSet* self, TWSMetadata* level) {
  if (self->solutions_n + 1 > self->solutions_allocated) { // todo: get a proper vector style generic thing going and port this to it
    self->solutions_allocated *= 2;
    self->solutions = xrealloc(self->solutions, sizeof(TWSMetadata) * self->solutions_allocated);
  }
  self->solutions[self->solutions_n] = *level;
  self->solutions_n += 1;
}

static Result_TWSSetPtr get_error(TWSSet* self, const char* err_msg) {
  TWSSet_free(self);
  return res_err(TWSSetPtr, err_msg);
}

static int TWSMetadata_cmp(const void* lhs_v, const void* rhs_v) {
  TWSMetadata const* lhs = lhs_v;
  TWSMetadata const* rhs = rhs_v;
  if (lhs->level_num < rhs->level_num) {
    return -1;
  } else if (lhs->level_num > rhs->level_num) {
    return 1;
  } else {
    return 0;
  }
}

// https://www.muppetlabs.com/~breadbox/software/tworld/tworldff.html#3
Result_TWSSetPtr parse_tws(uint8_t const* data, size_t data_len) {
  uint8_t const* const base_data = data;
  TWSSet* set = xcalloc(sizeof(TWSSet), 1);
  #define assert_data_avail(...)                                     \
    if (data - base_data __VA_OPT__(-1 +) __VA_ARGS__ >= data_len) { \
      return get_error(set, "TWS file ends too soon");               \
    }
  if (!data)
    return get_error(set, "TWS data is NULL");

  set->solutions_allocated = 1;
  set->solutions = xmalloc(sizeof(TWSMetadata) * set->solutions_allocated);
  assert_data_avail(4);
  uint32_t file_signature = read_uint32_le(data);
  data += 4;
  if (file_signature != 0x999B3335)
    return get_error(set, "Invalid TWS signature. Are you sure this is a TWS file?");

  assert_data_avail(1);
  set->ruleset = *data;
  data += 1;
  if (set->ruleset != Ruleset_Lynx && set->ruleset != Ruleset_MS)
    return get_error(set, "Invalid TWS ruleset.");

  assert_data_avail(2);
  set->recent_level = read_uint16_le(data);
  data += 2;

  assert_data_avail(1);
  uint8_t bytes_remaining = *data;
  data += 1;
  assert_data_avail(bytes_remaining);
  data += bytes_remaining;

  uint32_t levels_processed = 0;
  bool first_run = true;
  while (true) {
    if (data - base_data >= data_len)
      break;
    TWSMetadata level = {};
    uint32_t size = 0;
    while (size == 0) {
      assert_data_avail(4);
      size = read_uint32_le(data);
      data += 4;
    }
    if (size == 0xFFFFFFFF)
      break;

    assert_data_avail(size);
    if (size < 6)
      break;
    // return get_error(set, "Not enough data for first solution.");
    if (first_run && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0 && data[4] == 0 && data[5] == 0) {
      if (size <= 16)
        return get_error(set, "Not enough data for set name string.");
      data += 16;
      size -= 16;
      set->set_name = xmalloc(size);
      memcpy(set->set_name, data, size);
      set->set_name[size - 1] = '\0';
      data += size;
      first_run = false;
      continue;
    } else {
      assert_data_avail(6);
      level.level_num = read_uint16_le(data);
      data += 2;
      memcpy(&level.password, data, 4);
      data += 4;
      if (size == 6) {
        TWSSet_add_level(set, &level);
      } else {
        size -= 6;
        assert_data_avail(10);
        level.other_flags = *data;
        data += 1;
        uint8_t slide_step = *data;
        level.rff_dir = slide_step & 0b111;
        level.init_step_parity = (slide_step >> 3) & 0b111;
        data += 1;
        level.prng_seed = read_uint32_le(data);
        data += 4;
        level.num_ticks = read_uint32_le(data) + 1; // yeah idk either but it is correct
        data += 4;
        size -= 10;
        assert_data_avail(size);
        level.compressed_inputs.bytes = xmalloc(size);
        level.compressed_inputs.count = size;
        memcpy(level.compressed_inputs.bytes, data, size);
        data += size;
      }
      TWSSet_add_level(set, &level);
    }
    first_run = false;
    levels_processed += 1;
  }

  set->solutions_n = levels_processed;
  set->solutions = xrealloc(set->solutions, sizeof(TWSMetadata) * set->solutions_n);
  set->solutions_allocated = set->solutions_n;
  qsort(set->solutions, set->solutions_n, sizeof(TWSMetadata), TWSMetadata_cmp); // put the levels in order
  return res_val(TWSSetPtr, set);
}

GameInputList GameInputList_new(size_t initial_size) {
  if (initial_size == 0) {
    initial_size = 8;
  }
  GameInputList list = {NULL, 0, initial_size};
  list.inputs = xcalloc(sizeof(GameInput), list.allocated);
  return list;
}

void GameInputList_free(GameInputList* self) {
  free(self->inputs);
  self->count = 0;
  self->allocated = 0;
  self->inputs = NULL;
}

void GameInputList_shrink(GameInputList* self) {
  self->allocated = self->count;
  self->inputs = xrealloc(self->inputs, sizeof(GameInput) * self->allocated);
}

static void GameInputList_grow(GameInputList* self) {
  if (self->allocated == 0) {
    self->allocated = 4;
  }
  self->allocated *= 2;
  self->inputs = xrealloc(self->inputs, sizeof(GameInput) * self->allocated);
}

void GameInputList_resize(GameInputList* self, size_t new_size) {
  if (new_size == 0) {
    free(self->inputs);
    self->inputs = NULL;
    self->count = 0;
    self->allocated = 0;
    return;
  }
  if (new_size <= self->count) {
    self->count = new_size;
  } else {
    while (self->allocated < new_size) {
      GameInputList_grow(self);
    }
    memset(self->inputs + self->count, 0, (new_size - self->count) * sizeof(GameInput));
    self->count = new_size;
  }
}

void GameInputList_append(GameInputList* self, GameInput input) {
  if (self->count + 1 > self->allocated) {
    GameInputList_grow(self);
  }
  self->inputs[self->count] = input;
  self->count += 1;
}

GameInput GameInputList_get_input(GameInputList const* self, size_t tick) {
  if (tick >= self->count) {
    return DIRECTION_NIL;
  }
  return self->inputs[tick];
}
