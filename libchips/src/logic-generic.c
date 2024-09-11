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
