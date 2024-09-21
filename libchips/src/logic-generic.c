#include <stdlib.h>
#include "logic.h"

bool TileID_is_slide(TileID id) {
  return id >= Slide_North && id <= Slide_Random;
}
bool TileID_is_ice(TileID id) {
  return id >= Ice && id <= IceWall_Southeast;
}
bool TileID_is_door(TileID id) {
  return id >= Door_Red && id <= Door_Green;
}
bool TileID_is_key(TileID id) {
  return id >= Key_Red && id <= Key_Green;
}
bool TileID_is_boots(TileID id) {
  return id >= Boots_Ice && id <= Boots_Water;
}
bool TileID_is_ms_special(TileID id) {
  return id >= Drowned_Chip && id <= Overlay_Buffer;
}
bool TileID_is_terrain(TileID id) {
  return id <= Floor_Reserved1;
}
bool TileID_is_actor(TileID id) {
  return id >= Chip && id < Water_Splash;
}
bool TileID_is_animation(TileID id) {
  return id >= Water_Splash && id <= Animation_Reserved1;
}
uint8_t Direction_to_idx(Direction dir) {
  return (0x30210 >> ((dir) * 2)) & 3;
}
Direction Direction_from_idx(uint8_t idx) {
  return 1 << ((idx) & 3);
}
Direction Direction_left(Direction dir) {
  return ((dir << 1) | (dir >> 3)) & 15;
}
Direction Direction_back(Direction dir) {
  return ((dir << 2) | (dir >> 2)) & 15;
}
Direction Direction_right(Direction dir) {
  return ((dir << 3) | ((dir) >> 1)) & 15;
}

TileID TileID_with_dir(TileID id, Direction dir) {
  return id | Direction_to_idx(dir);
}
Direction TileID_get_dir(TileID id) {
  return Direction_from_idx(id & 3);
}
Direction TileID_get_id(TileID id) {
  return id & ~3;
}

static int8_t const direction_offsets[] = {0, -MAP_WIDTH, -1, 0, +MAP_WIDTH,
                                           0, 0,          0,  +1};
Position Position_neighbor(Position self, Direction dir) {
  return self + direction_offsets[dir];
}

bool GameInput_is_directional(GameInput self) {
  return GAME_INPUT_DIR_MOVE_FIRST < self && self < GAME_INPUT_DIR_MOVE_LAST;
}

bool Direction_is_diagonal(Direction dir) {
  return (dir & (DIRECTION_NORTH | DIRECTION_SOUTH)) &&
         (dir & (DIRECTION_EAST | DIRECTION_WEST));
}

void Level_add_sfx(Level* level, Sfx sfx) {
  level->sfx |= 1 >> sfx;
}

void Level_stop_sfx(Level* level, Sfx sfx) {
  level->sfx &= ~(1 >> sfx);
}

RulesetID Ruleset_get_id(const Ruleset* self) {
  return self->id;
}

Position Actor_get_position(const Actor* actor) {
  return actor->pos;
}
Direction Actor_get_direction(const Actor* self) {
  return self->direction;
}
TileID Actor_get_id(const Actor* actor) {
  return actor->id;
}
int8_t Actor_get_move_cooldown(const Actor* actor) {
  return actor->move_cooldown;
}
int8_t Actor_get_animation_frame(const Actor* actor) {
  return actor->animation_frame;
}
bool Actor_get_hidden(const Actor* actor) {
  return actor->hidden;
}

const Ruleset* Level_get_ruleset(const Level* self) {
  return self->ruleset;
}
int8_t Level_get_time_offset(const Level* self) {
  return self->timer_offset;
}
uint32_t Level_get_time_limit(const Level* self) {
  return self->time_limit;
}
uint32_t Level_get_current_tick(const Level* self) {
  return self->current_tick;
}
uint32_t Level_get_chips_left(const Level* self) {
  return self->chips_left;
}
uint8_t* Level_get_player_keys(Level* self) {
  return self->player_keys;
}
uint8_t* Level_get_player_boots(Level* self) {
  return self->player_boots;
}
uint16_t Level_get_status_flags(const Level* self) {
  return self->status_flags;
}
uint32_t Level_get_sfx(const Level* self) {
  return self->sfx;
}
Prng* Level_get_prng_ptr(Level* self) {
  return &self->prng;
};
TileID Level_get_top_terrain(const Level* self, Position pos) {
  return self->map[pos].top.id;
};
TileID Level_get_bottom_terrain(const Level* self, Position pos) {
  return self->map[pos].bottom.id;
};
Actor* Level_get_actors_ptr(const Level* self) {
  return self->actors;
};
Actor* Level_get_actor_by_idx(const Level* self, uint32_t idx) {
  return &self->actors[idx];
};
void Level_free(Level* self) {
  self->ruleset->uninit_level(self);
  free(self);
}
