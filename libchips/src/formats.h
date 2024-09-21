#include <stddef.h>
#include <stdint.h>
#include "logic.h"
#include "misc.h"

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
const char* LevelMetadata_get_title(const LevelMetadata* self);
uint16_t LevelMetadata_get_level_number(const LevelMetadata* self);
uint16_t LevelMetadata_get_time_limit(const LevelMetadata* self);
uint16_t LevelMetadata_get_chips_required(const LevelMetadata* self);
const char* LevelMetadata_get_password(const LevelMetadata* self);
const char* LevelMetadata_get_hint(const LevelMetadata* self);
const char* LevelMetadata_get_author(const LevelMetadata* self);

typedef Level* LevelPtr;
DEFINE_RESULT(LevelPtr);
Result_LevelPtr LevelMetadata_make_level(const LevelMetadata* self,
                                         const Ruleset* ruleset);

typedef struct LevelSet {
  uint16_t levels_n;
  LevelMetadata levels[];
} LevelSet;
uint16_t LevelSet_get_levels_n(const LevelSet* self);
LevelMetadata* LevelSet_get_level(LevelSet* self, uint16_t idx);

typedef LevelSet* LevelSetPtr;
DEFINE_RESULT(LevelSetPtr);
Result_LevelSetPtr parse_ccl(const uint8_t* data, size_t data_len);
void LevelSet_free(LevelSet* self);
