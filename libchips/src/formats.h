#ifndef FORMATS_H
#define FORMATS_H

#include <stddef.h>
#include <stdint.h>
#include "logic.h"
#include "misc.h"

uint16_t read_uint16_le(uint8_t const* data);
uint32_t read_uint32_le(uint8_t const* data);
uint64_t read_uint64_le(uint8_t const* data);

typedef struct LevelMetadata {
  char* title;
  uint16_t level_number;
  uint16_t time_limit;
  uint16_t chips_required;
  ConnList* trap_links;
  ConnList* cloner_links;
  uint8_t monsters_n;
  Position* monster_list;
  char password[10];
  char* hint;
  char* author;

  uint16_t layer_top_size;
  uint8_t* layer_top;
  uint16_t layer_bottom_size;
  uint8_t* layer_bottom;
} LevelMetadata;
char const* LevelMetadata_get_title(LevelMetadata const* self);
uint16_t LevelMetadata_get_level_number(LevelMetadata const* self);
uint16_t LevelMetadata_get_time_limit(LevelMetadata const* self);
uint16_t LevelMetadata_get_chips_required(LevelMetadata const* self);
char const* LevelMetadata_get_password(LevelMetadata const* self);
char const* LevelMetadata_get_hint(LevelMetadata const* self);
char const* LevelMetadata_get_author(LevelMetadata const* self);

typedef Level* LevelPtr;
DEFINE_RESULT(LevelPtr);
Result_LevelPtr LevelMetadata_make_level(LevelMetadata const* self,
                                         Ruleset const* ruleset);

typedef struct LevelSet {
  char* name;
  uint16_t levels_n;
  LevelMetadata levels[];
} LevelSet;
void LevelSet_set_name(LevelSet* self, char const* set_name);
char const* LevelSet_get_name(LevelSet const* self);
uint16_t LevelSet_get_levels_n(LevelSet const* self);
LevelMetadata* LevelSet_get_level(LevelSet* self, uint16_t idx);

typedef LevelSet* LevelSetPtr;
DEFINE_RESULT(LevelSetPtr);
Result_LevelSetPtr parse_ccl(uint8_t const* data, size_t data_len);
void LevelSet_free(LevelSet* self);

#endif
