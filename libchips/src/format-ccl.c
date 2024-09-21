#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "formats.h"

// Sure would be great to have `constexpr` for `TileID_with_dir`
#define north(id) id
#define west(id) ((id) | 1)
#define east(id) ((id) | 2)
#define south(id) ((id) | 3)

char const* LevelMetadata_get_title(LevelMetadata const* self) {
  return self->title;
};
uint16_t LevelMetadata_get_level_number(LevelMetadata const* self) {
  return self->level_number;
};
uint16_t LevelMetadata_get_time_limit(LevelMetadata const* self) {
  return self->time_limit;
};
uint16_t LevelMetadata_get_chips_required(LevelMetadata const* self) {
  return self->chips_required;
};
char const* LevelMetadata_get_password(LevelMetadata const* self) {
  return self->password;
};
char const* LevelMetadata_get_hint(LevelMetadata const* self) {
  return self->hint;
};
char const* LevelMetadata_get_author(LevelMetadata const* self) {
  return self->author;
};

uint16_t LevelSet_get_levels_n(LevelSet const* self) {
  return self->levels_n;
};
LevelMetadata* LevelSet_get_level(LevelSet* self, uint16_t idx) {
  return &self->levels[idx];
};

static TileID const dat_tileid_map[] = {
    // 0x00
    Empty, Wall, ICChip, Water, Fire, HiddenWall_Perm, Wall_North, Wall_West,
    Wall_South, Wall_East, Block_Static, Dirt, Ice, Slide_South,
    // 0x10
    north(Block), west(Block), south(Block), east(Block), Slide_North,
    Slide_East, Slide_West, Exit, Door_Blue, Door_Red, Door_Green, Door_Yellow,
    IceWall_Northwest, IceWall_Northeast, IceWall_Southeast, IceWall_Southwest,
    BlueWall_Fake, BlueWall_Real,
    // 0x20
    1, Burglar, Socket, Button_Green, Button_Red, SwitchWall_Closed,
    SwitchWall_Open, Button_Brown, Button_Blue, Teleport, Bomb, Beartrap,
    HiddenWall_Temp, Gravel, PopupWall, HintButton,
    // 0x30
    Wall_Southeast, CloneMachine, Slide_Random, Drowned_Chip, Burned_Chip,
    Bombed_Chip, 1, 1, 1, Exited_Chip, Exit_Extra_1, Exit_Extra_2,
    north(Swimming_Chip), west(Swimming_Chip), south(Swimming_Chip),
    east(Swimming_Chip),
    // 0x40
    north(Bug), west(Bug), south(Bug), east(Bug), north(Fireball),
    west(Fireball), south(Fireball), east(Fireball), north(Ball), west(Ball),
    south(Ball), east(Ball), north(Tank), west(Tank), south(Tank), east(Tank),
    // 0x50
    north(Glider), west(Glider), south(Glider), east(Glider), north(Teeth),
    west(Teeth), south(Teeth), east(Teeth), north(Walker), west(Walker),
    south(Walker), east(Walker), north(Blob), west(Blob), south(Blob),
    east(Blob),
    // 0x60
    north(Paramecium), west(Paramecium), south(Paramecium), east(Paramecium),
    Key_Blue, Key_Red, Key_Green, Key_Yellow, Boots_Water, Boots_Fire,
    Boots_Ice, Boots_Slide, north(Chip), west(Chip), south(Chip), east(Chip)};

static uint16_t read_uint16_le(uint8_t const* data) {
  return data[0] + (data[1] << 8);
}
static uint32_t read_uint32_le(uint8_t const* data) {
  return data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
}

enum CllChunkTypes {
  CCL_CHUNK_REDUNDANT_TIME = 1,
  CCL_CHUNK_REDUNDANT_CHIPS = 2,
  CCL_CHUNK_TITLE = 3,
  CCL_CHUNK_TRAPS = 4,
  CCL_CHUNK_CLONERS = 5,
  CCL_CHUNK_PASSWORD = 6,
  CCL_CHUNK_REDUNDANT_PASSWORD = 7,
  CCL_CHUNK_HINT = 8,
  CCL_CHUNK_AUTHOR = 9,
  CCL_CHUNK_MONSTER_LIST = 10,
};

Result_LevelSetPtr parse_ccl(uint8_t const* data, size_t data_len) {
  uint8_t const* const base_data = data;
#define assert_data_avail(...)                                     \
  if (data - base_data __VA_OPT__(-1 +) __VA_ARGS__ >= data_len) { \
    LevelSet_free(set);                                            \
    return res_err(LevelSetPtr, "CCL file ends too soon");         \
  }

  LevelSet* set = NULL;

  assert_data_avail(4);
  uint32_t magic_bytes = read_uint32_le(data);
  data += 4;
  if (magic_bytes != 0x0002AAAC)
    return res_err(LevelSetPtr,
                   "Invalid CCL signature. Are you sure this is a CCL file?");
  assert_data_avail(2);
  uint16_t levels_n = read_uint16_le(data);
  data += 2;

  set = xmalloc(sizeof(LevelSet) + levels_n * sizeof(LevelMetadata));
  // We will update `levels_n` as we parse the levels, so that uninit in case of
  // error is easier
  set->levels_n = 0;

  for (uint16_t idx = 0; idx < levels_n; idx += 1) {
    set->levels_n += 1;
    LevelMetadata* meta = &set->levels[idx];
    *meta = (LevelMetadata){};
    assert_data_avail(12);
    uint8_t const* const level_data_ptr = data;
    uint16_t level_data_len = read_uint16_le(data);
    meta->level_number = read_uint16_le(data + 2);
    meta->time_limit = read_uint16_le(data + 4);
    meta->chips_required = read_uint16_le(data + 6);
    // data+8 is unused
    meta->layer_top_size = read_uint16_le(data + 10);
    data += 12;
    assert_data_avail(meta->layer_top_size);
    meta->layer_top = xmalloc(meta->layer_top_size);
    memcpy(meta->layer_top, data, meta->layer_top_size);
    data += meta->layer_top_size;

    meta->layer_bottom_size = read_uint16_le(data);
    data += 2;
    assert_data_avail(meta->layer_bottom_size);
    meta->layer_bottom = xmalloc(meta->layer_bottom_size);
    memcpy(meta->layer_bottom, data, meta->layer_bottom_size);
    data += meta->layer_bottom_size;
    assert_data_avail(2);
    uint16_t level_chunks_size = read_uint16_le(data);
    data += 2;
    assert_data_avail(level_chunks_size);
    while (level_chunks_size > 0) {
      assert_data_avail(2);
      uint8_t chunk_type = data[0];
      uint8_t chunk_len = data[1];
      data += 2;
      assert_data_avail(chunk_len);
      if (chunk_type == CCL_CHUNK_TITLE) {
        meta->title =
            strndup((char const*)data, chunk_len > 64 ? chunk_len : 64);
      } else if (chunk_type == CCL_CHUNK_TRAPS) {
        uint8_t traps_n = chunk_type / 10;
        meta->trap_links = xmalloc(sizeof(ConnList));
        for (uint8_t trap_idx = 0; trap_idx < traps_n; trap_idx += 1) {
          TileConn* conn = &meta->trap_links->items[trap_idx];
          uint16_t from_x = read_uint16_le(&data[trap_idx * 10]);
          uint16_t from_y = read_uint16_le(&data[trap_idx * 10 + 2]);
          uint16_t to_x = read_uint16_le(&data[trap_idx * 10 + 4]);
          uint16_t to_y = read_uint16_le(&data[trap_idx * 10 + 6]);
          // data[trap_id*10+8] is unused
          conn->from = from_x + from_y * MAP_WIDTH;
          conn->to = to_x + to_y * MAP_WIDTH;
        }
        data -= traps_n * 10;
      } else if (chunk_type == CCL_CHUNK_CLONERS) {
        uint8_t cloners_n = chunk_type / 8;
        meta->cloner_links = xmalloc(sizeof(ConnList));
        for (uint8_t cloner_idx = 0; cloner_idx < cloners_n; cloner_idx += 1) {
          TileConn* conn = &meta->cloner_links->items[cloner_idx];
          uint16_t from_x = read_uint16_le(&data[cloner_idx * 8]);
          uint16_t from_y = read_uint16_le(&data[cloner_idx * 8 + 2]);
          uint16_t to_x = read_uint16_le(&data[cloner_idx * 8 + 4]);
          uint16_t to_y = read_uint16_le(&data[cloner_idx * 8 + 6]);
          conn->from = from_x + from_y * MAP_WIDTH;
          conn->to = to_x + to_y * MAP_WIDTH;
        }
      } else if (chunk_type == CCL_CHUNK_PASSWORD) {
        strncpy(meta->password, (char const*)data,
                chunk_len > 10 ? 10 : chunk_len);
        // Decode the password
        for (char* password_char = meta->password; *password_char != 0;
             password_char += 1) {
          *password_char ^= 0x99;
        }
      } else if (chunk_type == CCL_CHUNK_HINT) {
        meta->hint =
            strndup((char const*)data, chunk_len > 128 ? 128 : chunk_len);
      } else if (chunk_type == CCL_CHUNK_MONSTER_LIST) {
        uint8_t monsters_n = chunk_len / 2;
        meta->monster_list = xmalloc(monsters_n * sizeof(Position));
        for (uint8_t monster_idx = 0; monster_idx < monsters_n;
             monster_idx += 1) {
          uint8_t monster_x = data[monster_idx * 2];
          uint8_t monster_y = data[monster_idx * 2 + 1];
          meta->monster_list[monster_idx] = monster_x + monster_y * MAP_WIDTH;
        }
      } else if (chunk_type == CCL_CHUNK_AUTHOR) {
        meta->author =
            strndup((char const*)data, chunk_len > 128 ? 128 : chunk_len);
      } else {
        // Unknown chunk type, ignore
      }
      data += chunk_len;
      level_chunks_size -= 2 + chunk_len;
    }
  }
  if (data != base_data + data_len) {
    LevelSet_free(set);
    return res_err(LevelSetPtr, "CCL larger than needed");
  }
  return res_val(LevelSetPtr, set);
}

void LevelSet_free(LevelSet* self) {
  if (self == NULL)
    return;
  for (uint16_t level_idx = 0; level_idx < self->levels_n; level_idx += 1) {
    LevelMetadata* meta = &self->levels[level_idx];
    free(meta->title);
    free(meta->hint);
    free(meta->author);
    free(meta->trap_links);
    free(meta->cloner_links);
    free(meta->monster_list);
    free(meta->layer_top);
    free(meta->layer_bottom);
  }
  free(self);
}

static bool uncompress_field(uint8_t* to,
                             uint8_t const* from,
                             size_t from_size) {
  size_t from_idx = 0;
  size_t to_idx = 0;
  size_t const to_size = MAP_WIDTH * MAP_HEIGHT;
  while (to_idx != to_size) {
    if (from_size - from_idx < 1)
      return false;
    if (from[from_idx] != 0xFF) {
      to[to_idx] = from[from_idx];
      to_idx += 1;
      from_idx += 1;
    } else {
      if (from_size - from_idx < 3)
        return false;
      size_t rle_count = from[from_idx + 1];
      if (rle_count > to_size - to_idx)
        return false;
      uint8_t rle_val = from[from_idx + 2];
      memset(to + to_idx, rle_val, rle_count);
      to_idx += rle_count;
      from_idx += 3;
    }
  }
  if (from_idx != from_size)
    return false;
  return true;
}

Result_LevelPtr LevelMetadata_make_level(LevelMetadata const* self,
                                         Ruleset const* ruleset) {
  Level* level = xmalloc(sizeof(Level));
  *level = (Level){.ruleset = ruleset,
                   .chips_left = self->chips_required,
                   .time_limit = self->time_limit * 20};
  if (self->trap_links) {
    level->trap_connections = *self->trap_links;
  }
  if (self->cloner_links) {
    level->cloner_connections = *self->cloner_links;
  }
  if (self->monster_list) {
    memcpy(level->ms_state.init_actor_list, self->monster_list,
           self->monsters_n * sizeof(Position));
  }
  uint8_t uncompressed_field[MAP_WIDTH * MAP_HEIGHT];
  if (!uncompress_field(uncompressed_field, self->layer_top,
                        self->layer_top_size)) {
    free(level);
    return res_err(LevelPtr, "Failed to uncompress top field");
  }
  for (Position pos = 0; pos < MAP_WIDTH * MAP_HEIGHT; pos += 1) {
    uint8_t ccl_tile_id = uncompressed_field[pos];
    if (ccl_tile_id >= lengthof(dat_tileid_map)) {
      free(level);
      return res_err(LevelPtr, "Unknown CCL tile id %02X", ccl_tile_id);
    }
    level->map[pos].top.id = dat_tileid_map[ccl_tile_id];
  }
  if (!uncompress_field(uncompressed_field, self->layer_bottom,
                        self->layer_bottom_size)) {
    free(level);
    return res_err(LevelPtr, "Failed to uncompress bottom field");
  }
  for (Position pos = 0; pos < MAP_WIDTH * MAP_HEIGHT; pos += 1) {
    uint8_t ccl_tile_id = uncompressed_field[pos];
    if (ccl_tile_id >= lengthof(dat_tileid_map)) {
      free(level);
      return res_err(LevelPtr, "Unknown CCL tile id %02X", ccl_tile_id);
    }
    level->map[pos].bottom.id = dat_tileid_map[ccl_tile_id];
  }

  level->ruleset = ruleset;
  bool init_success = ruleset->init_level(level);
  return res_val(LevelPtr, level);
}
