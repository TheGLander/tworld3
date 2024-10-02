#ifndef LIB_CHIPS_LOGIC_H
#define LIB_CHIPS_LOGIC_H

#include <stdbool.h>
#include <stdint.h>
#include "random.h"

#define MAP_WIDTH (32)
#define MAP_HEIGHT (32)

#define MAX_CREATURES (2 * MAP_WIDTH * MAP_HEIGHT)

typedef enum RulesetID {
  Ruleset_None = 0,
  Ruleset_Lynx = 1,
  Ruleset_MS = 2,
  Ruleset_Count,
  Ruleset_First = Ruleset_Lynx
} RulesetID;

typedef enum TileID {
  Nothing = 0,

  Empty = 0x01,

  Slide_North = 0x02,
  Slide_West = 0x03,
  Slide_South = 0x04,
  Slide_East = 0x05,
  Slide_Random = 0x06,
  Ice = 0x07,
  IceWall_Northwest = 0x08,
  IceWall_Northeast = 0x09,
  IceWall_Southwest = 0x0A,
  IceWall_Southeast = 0x0B,
  Gravel = 0x0C,
  Dirt = 0x0D,
  Water = 0x0E,
  Fire = 0x0F,
  Bomb = 0x10,
  Beartrap = 0x11,
  Burglar = 0x12,
  HintButton = 0x13,

  Button_Blue = 0x14,
  Button_Green = 0x15,
  Button_Red = 0x16,
  Button_Brown = 0x17,
  Teleport = 0x18,

  Wall = 0x19,
  Wall_North = 0x1A,
  Wall_West = 0x1B,
  Wall_South = 0x1C,
  Wall_East = 0x1D,
  Wall_Southeast = 0x1E,
  HiddenWall_Perm = 0x1F,
  HiddenWall_Temp = 0x20,
  BlueWall_Real = 0x21,
  BlueWall_Fake = 0x22,
  SwitchWall_Open = 0x23,
  SwitchWall_Closed = 0x24,
  PopupWall = 0x25,

  CloneMachine = 0x26,

  Door_Red = 0x27,
  Door_Blue = 0x28,
  Door_Yellow = 0x29,
  Door_Green = 0x2A,
  Socket = 0x2B,
  Exit = 0x2C,

  ICChip = 0x2D,
  Key_Red = 0x2E,
  Key_Blue = 0x2F,
  Key_Yellow = 0x30,
  Key_Green = 0x31,
  Boots_Ice = 0x32,
  Boots_Slide = 0x33,
  Boots_Fire = 0x34,
  Boots_Water = 0x35,

  Block_Static = 0x36,

  Drowned_Chip = 0x37,
  Burned_Chip = 0x38,
  Bombed_Chip = 0x39,
  Exited_Chip = 0x3A,
  Exit_Extra_1 = 0x3B,
  Exit_Extra_2 = 0x3C,

  Overlay_Buffer = 0x3D,

  Floor_Reserved2 = 0x3E,
  Floor_Reserved1 = 0x3F,

  Chip = 0x40,

  Block = 0x44,

  Tank = 0x48,
  Ball = 0x4C,
  Glider = 0x50,
  Fireball = 0x54,
  Walker = 0x58,
  Blob = 0x5C,
  Teeth = 0x60,
  Bug = 0x64,
  Paramecium = 0x68,

  Swimming_Chip = 0x6C,
  Pushing_Chip = 0x70,

  Entity_Reserved2 = 0x74,
  Entity_Reserved1 = 0x78,

  Water_Splash = 0x7C,
  Bomb_Explosion = 0x7D,
  Entity_Explosion = 0x7E,
  Animation_Reserved1 = 0x7F
} TileID;

bool TileID_is_slide(TileID id);
bool TileID_is_ice(TileID id);
bool TileID_is_door(TileID id);
bool TileID_is_key(TileID id);
bool TileID_is_boots(TileID id);
bool TileID_is_ms_special(TileID id);
bool TileID_is_terrain(TileID id);
bool TileID_is_actor(TileID id);
bool TileID_is_animation(TileID id);

typedef int16_t Position;
enum { POSITION_NULL = -1 };
enum {
  DIRECTION_NIL = 0,
  DIRECTION_NORTH = 1,
  DIRECTION_WEST = 2,
  DIRECTION_SOUTH = 4,
  DIRECTION_EAST = 8,
};
typedef uint8_t Direction;

uint8_t Direction_to_idx(Direction dir);
Direction Direction_from_idx(uint8_t idx);
Direction Direction_left(Direction dir);
Direction Direction_back(Direction dir);
Direction Direction_right(Direction dir);

TileID TileID_actor_with_dir(TileID id, Direction dir);
Direction TileID_actor_get_dir(TileID id);
Direction TileID_actor_get_id(TileID id);
bool Direction_is_diagonal(Direction dir);

Position Position_neighbor(Position self, Direction dir);

typedef uint16_t GameInput;
enum {  // Mouse moves are a 19x19 square relative to Chip, packing them into 9
        // bits, I don't know where else to put this
  MOUSERANGEMIN = -9,
  MOUSERANGEMAX = +9,
  MOUSERANGE = 19,
};
enum {
  GAME_INPUT_DIR_MOVE_FIRST = DIRECTION_NORTH,
  GAME_INPUT_DIR_MOVE_LAST =
      DIRECTION_NORTH | DIRECTION_EAST | DIRECTION_SOUTH | DIRECTION_WEST,

  GAME_INPUT_MOUSE_MOVE_FIRST,
  GAME_INPUT_MOUSE_MOVE_LAST =
      GAME_INPUT_MOUSE_MOVE_FIRST + MOUSERANGE * MOUSERANGE - 1,
  GAME_INPUT_ABS_MOUSE_MOVE_FIRST = 512,
  GAME_INPUT_ABS_MOUSE_MOVE_LAST =
      GAME_INPUT_ABS_MOUSE_MOVE_FIRST +
      MAP_WIDTH *
          MAP_HEIGHT,  // todo: what the ever loving hell is this, is this used?
};
bool GameInput_is_directional(GameInput self);

typedef struct Actor {
  Position pos;
  TileID id;
  Direction direction;
  int8_t move_cooldown;
  int8_t animation_frame;
  bool hidden;
  // Ruleset-specific state
  uint16_t state;
  Direction move_decision;
} Actor;
Position Actor_get_position(Actor const* actor);
TileID Actor_get_id(Actor const* actor);
Direction Actor_get_direction(Actor const* actor);
int8_t Actor_get_move_cooldown(Actor const* actor);
int8_t Actor_get_animation_frame(Actor const* actor);
bool Actor_get_hidden(Actor const* actor);

typedef struct TileConn {
  Position from;
  Position to;
} TileConn;

typedef struct ConnList {
  uint8_t length;
  TileConn items[256];
} ConnList;

typedef struct MapTile {
  TileID id;
  uint8_t state;
} MapTile;

typedef struct MapCell {
  MapTile top;
  MapTile bottom;
} MapCell;

enum {
  CHIP_OKAY = 0,
  CHIP_DROWNED,
  CHIP_BURNED,
  CHIP_BOMBED,
  CHIP_OUTOFTIME,
  CHIP_COLLIDED,
  CHIP_SQUISHED,
  CHIP_SQUISHED_DEATH,
  CHIP_NOTOKAY
};
typedef uint8_t ChipStatus;

typedef struct MsSlipper {
  Actor* actor;
  Direction direction;
} MsSlipper;

typedef struct MsState {
  uint32_t actor_count;
  uint32_t slip_count;
  MsSlipper slip_list[MAX_CREATURES];
  uint32_t block_list_count;
  Actor* block_list[MAX_CREATURES];
  uint32_t mscc_slippers;
  uint8_t chip_ticks_since_moved;
  ChipStatus chip_status;
  Direction chip_last_slip_dir;
  Position mouse_goal;
  Direction controller_dir;
  uint16_t init_actors_n;
  Position init_actor_list[256];
} MsState;

typedef struct LxState {
  bool pedantic_mode;
  Actor* chip_colliding_actor;
  Actor* last_actor;
  Position chip_predicted_pos;
  Position to_place_wall_pos;
  uint8_t prng1;
  uint8_t prng2;
  uint8_t endgame_timer;
  uint8_t toggle_walls_xor;
  bool chip_stuck;
  bool chip_pushing;
  bool chip_bonked;
  bool map_breached;
} LxState;

typedef struct Level Level;

typedef struct Ruleset {
  RulesetID id;
  bool (*init_level)(Level*);
  void (*tick_level)(Level*);
  void (*uninit_level)(Level*);
} Ruleset;
RulesetID Ruleset_get_id(const Ruleset* self);

enum { TRIRES_DIED = -1, TRIRES_NOTHING = 0, TRIRES_SUCCESS = 1 };
typedef int8_t TriRes;

typedef struct LevelMetadata LevelMetadata;

typedef struct Level {
  LevelMetadata const* metadata; 
  Ruleset const* ruleset;
  // `replay`?
  int8_t timer_offset;
  uint32_t time_limit;
  GameInput game_input;
  uint32_t current_tick;
  uint16_t chips_left;
  Position camera_pos;
  uint8_t player_keys[4];
  uint8_t player_boots[4];
  // `lastmove`?
  uint16_t status_flags;
  Direction rff_dir;
  int8_t init_step_parity;
  uint32_t sfx;
  // `moves`?
  Prng prng;
  Actor* actors;
  ConnList trap_connections;
  ConnList cloner_connections;
  MapCell map[MAP_WIDTH * MAP_HEIGHT];
  bool level_complete;
  TriRes win_state;
  union {
    MsState ms_state;
    LxState lx_state;
  };
} Level;

const Ruleset* Level_get_ruleset(Level const* self);
int8_t Level_get_time_offset(Level const* self);
uint32_t Level_get_time_limit(Level const* self);
uint32_t Level_get_current_tick(Level const* self);
uint32_t Level_get_chips_left(Level const* self);
uint8_t* Level_get_player_keys(Level* self);
uint8_t* Level_get_player_boots(Level* self);
uint16_t Level_get_status_flags(Level const* self);
uint32_t Level_get_sfx(Level const* self);
Prng* Level_get_prng_ptr(Level* self);
TileID Level_get_top_terrain(Level const* self, Position pos);
TileID Level_get_bottom_terrain(Level const* self, Position pos);
Actor* Level_get_actors_ptr(Level const* self);
Actor* Level_get_actor_by_idx(Level const* self, uint32_t idx);
uint8_t* Level_player_item_ptr(Level* self, TileID id);
bool Level_player_has_item(Level const* self, TileID id);
void Level_set_game_input(Level* self, GameInput game_input);
GameInput Level_get_game_input(Level const* self);
TriRes Level_get_win_state(Level const* self);
LevelMetadata const* Level_get_metadata(Level const* self);


typedef enum Sfx {
  SND_CHIP_LOSES = 0,
  SND_CHIP_WINS = 1,
  SND_TIME_OUT = 2,
  SND_TIME_LOW = 3,
  SND_DEREZZ = 4,
  SND_CANT_MOVE = 5,
  SND_IC_COLLECTED = 6,
  SND_ITEM_COLLECTED = 7,
  SND_BOOTS_STOLEN = 8,
  SND_TELEPORTING = 9,
  SND_DOOR_OPENED = 10,
  SND_SOCKET_OPENED = 11,
  SND_BUTTON_PUSHED = 12,
  SND_TILE_EMPTIED = 13,
  SND_WALL_CREATED = 14,
  SND_TRAP_ENTERED = 15,
  SND_BOMB_EXPLODES = 16,
  SND_WATER_SPLASH = 17,
  SND_ONESHOT_COUNT = 18,

  SND_BLOCK_MOVING = 18,
  SND_SKATING_FORWARD = 19,
  SND_SKATING_TURN = 20,
  SND_SLIDING = 21,
  SND_SLIDEWALKING = 22,
  SND_ICEWALKING = 23,
  SND_WATERWALKING = 24,
  SND_FIREWALKING = 25,
  SND_COUNT = 26,
} Sfx;

void Level_add_sfx(Level* self, Sfx sfx);
void Level_stop_sfx(Level* self, Sfx sfx);
void Level_free(Level* self);

void Level_tick(Level* self);

enum StateFlags {
  SF_INVALID = 0x2,
  SF_BAD_TILES = 0x4,
  SF_SHOW_HINT = 0x8,
  SF_NO_ANIMATION = 0x10,
  SF_SHUTTERRED = 0x20,
};

extern Ruleset const lynx_logic;
extern Ruleset const ms_logic;

#endif  // LIB_CHIPS_LOGIC_H
