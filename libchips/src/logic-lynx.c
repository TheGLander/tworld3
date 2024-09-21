#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "logic.h"

#define PEDANTIC_MAX_CREATURES 128

enum ActorState {
  CS_FDIRMASK = 0xf,
  CS_SLIDETOKEN = 0x10,
  CS_REVERSE = 0x20,
  CS_PUSHED = 0x40,
  CS_TELEPORTED = 0x80
};

enum CollisionCheckFlags {
  CMM_RELEASING = 0x0001,
  CMM_CLEARANIMATIONS = 0x0002,
  CMM_STARTMOVEMENT = 0x0004,
  CMM_PUSHBLOCKS = 0x0008,
  CMM_PUSHBLOCKSNOW = 0x0010,
};

static inline TileID Level_cell_get_top_terrain(const Level* self, Position pos) {
  return self->map[pos].top.id;
}
static inline void Level_cell_set_top_terrain(Level* self, Position pos, TileID tile) {
  self->map[pos].top.id = tile;
}

enum TerrainState {
  /**
   * Is there a non-Chip, non-animation actor on this cell?
   */
  FS_CLAIMED = 0x40,
  /**
   * Is there an animation on this cell?
   */
  FS_ANIMATED = 0x20,
  /**
   * Was there ever a trap on this cell?
   * Not equivalent to checking if TileID is a trap since pedantic recessed
   * walls can overwrite terrain
   */
  FS_HAD_TRAP = 0x01,
  /**
   * Was there ever a teleport on this cell?
   * Not equivalent to checking if TileID is a teleport since pedantic recessed
   * walls can overwrite terrain
   */
  FS_HAD_TELEPORT = 0x02,
};

static inline void Level_cell_add_claim(Level* self, Position pos) {
  self->map[pos].top.state |= FS_CLAIMED;
}
static inline void Level_cell_remove_claim(Level* self, Position pos) {
  self->map[pos].top.state &= ~FS_CLAIMED;
}
static inline bool Level_cell_has_claim(const Level* self, Position pos) {
  return self->map[pos].top.state & FS_CLAIMED;
}
static inline void Level_cell_add_animation(Level* self, Position pos) {
  self->map[pos].top.state |= FS_ANIMATED;
}
static inline void Level_cell_remove_animation(Level* self, Position pos) {
  self->map[pos].top.state &= ~FS_ANIMATED;
}
static inline bool Level_cell_has_animation(const Level* self, Position pos) {
  return self->map[pos].top.state & FS_ANIMATED;
}
static inline void Level_cell_add_trap_presence(Level* self, Position pos) {
  self->map[pos].top.state |= FS_HAD_TRAP;
}
static inline bool Level_cell_ever_had_trap(const Level* self, Position pos) {
  return self->map[pos].top.state & FS_HAD_TRAP;
}
static inline void Level_cell_add_teleport_presence(Level* self, Position pos) {
  self->map[pos].top.state |= FS_HAD_TELEPORT;
}
static inline bool Level_cell_ever_had_teleport(const Level* self,
                                                Position pos) {
  return self->map[pos].top.state & FS_HAD_TELEPORT;
}
static inline Actor* Level_get_chip(Level* self) {
  return &self->actors[0];
}
static inline bool Level_in_endgame(const Level* self) {
  return self->lx_state.endgame_timer > 0;
}
static void Level_start_endgame(Level* self) {
  self->lx_state.endgame_timer = 13;
  self->timer_offset = 1;
}
static inline bool Actor_is_moving(const Actor* self) {
  return self->move_cooldown > 0;
}
static uint8_t Level_lynx_rng(Level* self) {
  uint8_t n = (self->lx_state.prng1 >> 2) - self->lx_state.prng1;
  if (!(self->lx_state.prng1 & 0x02))
    n -= 1;
  self->lx_state.prng1 =
      (self->lx_state.prng1 >> 1) | (self->lx_state.prng2 & 0x80);
  self->lx_state.prng2 = (self->lx_state.prng2 << 1) | (n & 0x01);
  return self->lx_state.prng1 ^ self->lx_state.prng2;
}

static void Level_stop_terrain_sfx(Level* level) {
  Level_stop_sfx(level, SND_SKATING_FORWARD);
  Level_stop_sfx(level, SND_SKATING_TURN);
  Level_stop_sfx(level, SND_FIREWALKING);
  Level_stop_sfx(level, SND_WATERWALKING);
  Level_stop_sfx(level, SND_ICEWALKING);
  Level_stop_sfx(level, SND_SLIDEWALKING);
  Level_stop_sfx(level, SND_SLIDING);
}

static bool lynx_init_level(Level* self) {
  Actor* actors = calloc(MAX_CREATURES + 1, sizeof(Actor));
  assert(actors != NULL);
  // TODO: Do we actually need to skip the first actor?
  actors += 1;
  self->actors = actors;
  uint16_t actors_n = 0;
  Actor* chip = NULL;
  if (self->lx_state.pedantic_mode && self->status_flags & SF_BAD_TILES) {
    self->status_flags |= SF_INVALID;
  }
  for (Position pos = 0; pos < MAP_WIDTH * MAP_HEIGHT; pos += 1) {
    MapCell* cell = &self->map[pos];
    // Convert MS tiles into Lynx-comptabile subtitutes
    if (cell->top.id == Block_Static) {
      cell->top.id = TileID_actor_with_dir(Block_Static, DIRECTION_NORTH);
    }
    if (cell->bottom.id == Block_Static) {
      cell->bottom.id = TileID_actor_with_dir(Block_Static, DIRECTION_NORTH);
    }
    if (TileID_is_ms_special(cell->top.id)) {
      cell->top.id = Wall;
      if (self->lx_state.pedantic_mode) {
        self->status_flags |= SF_INVALID;
      }
    }
    if (TileID_is_ms_special(cell->bottom.id)) {
      cell->bottom.id = Wall;
      if (self->lx_state.pedantic_mode) {
        self->status_flags |= SF_INVALID;
      }
    }
    // Detect MS-style buried tiles
    if (cell->bottom.id != Empty && (!TileID_is_terrain(cell->bottom.id) ||
                                     TileID_is_terrain(cell->top.id))) {
      // warn("level %d: invalid \"buried\" tile at (%d %d)",
      //      num, pos % CXGRID, pos / CXGRID);
      self->status_flags |= SF_INVALID;
    }
    // Create actors
    if (TileID_is_actor(cell->top.id)) {
      Actor* actor = &actors[actors_n];
      actors_n += 1;
      actor->pos = pos;
      actor->id = TileID_actor_get_id(cell->top.id);
      actor->direction = TileID_actor_get_dir(cell->top.id);
      if (self->lx_state.pedantic_mode && actor->id == Block &&
          TileID_is_ice(cell->bottom.id)) {
        actor->direction = DIRECTION_NIL;
      }
      if (actor->id == Chip) {
        if (chip) {
          // warn("level %d: multiple Chips on the map!", num);
          self->status_flags |= SF_INVALID;
        }
        chip = actor;
        actor->direction = DIRECTION_SOUTH;
      } else {
        Level_cell_add_claim(self, actor->pos);
      }
      cell->top.id = cell->bottom.id;
      cell->bottom.id = Empty;
    }
    // These tiles don't exist in Lynx Lynx, so they are technically invalid
    if (self->lx_state.pedantic_mode &&
        (cell->top.id == Wall_North || cell->top.id == Wall_West)) {
      self->status_flags |= SF_INVALID;
    }
    if (cell->top.id == Beartrap) {
      Level_cell_add_trap_presence(self, pos);
    }
    if (cell->top.id == Teleport) {
      Level_cell_add_teleport_presence(self, pos);
    }
  }
  if (!chip) {
    // warn("level %d: Chip isn't on the map!", num);
    self->status_flags |= SF_INVALID;
    chip = &actors[actors_n];
    actors_n += 1;
    chip->pos = 0;
    chip->hidden = true;
  }
  self->lx_state.last_actor = &actors[actors_n - 1];
  // TODO: Is this actually needed?
  Actor* final_actor = &actors[actors_n];
  final_actor->pos = POSITION_NULL;
  final_actor->id = Nothing;
  final_actor->direction = DIRECTION_NIL;
  // Swap Chip to be the first actor
  if (chip) {
    Actor* first_actor = &self->actors[0];
    Actor temp;
    temp = *first_actor;
    *first_actor = *chip;
    *chip = temp;
    chip = first_actor;
  }
  // TODO: Validate trap and cloner lists
  memset(self->player_boots, 0, sizeof(self->player_boots));
  memset(self->player_keys, 0, sizeof(self->player_keys));
  self->lx_state = (LxState){
      .pedantic_mode = self->lx_state.pedantic_mode,
      .chip_stuck = self->lx_state.pedantic_mode &&
                    chip->pos != POSITION_NULL &&
                    TileID_is_ice(Level_cell_get_top_terrain(self, chip->pos)),
      .chip_predicted_pos = POSITION_NULL,
  };

  return !(self->status_flags & SF_INVALID);
}

static void Actor_remove(Actor* self, Level* level, TileID animation_type) {
  if (self->id != Chip) {
    Level_cell_remove_claim(level, self->pos);
  }
  if (self->state & CS_PUSHED) {
    Level_stop_sfx(level, SND_BLOCK_MOVING);
  }
  self->id = animation_type;
  self->animation_frame =
      ((level->current_tick + level->init_step_parity) & 1) ? 12 : 11;
  self->animation_frame -= 1;
  self->hidden = false;
  self->state = 0;
  self->move_decision = DIRECTION_NIL;
  // If this actor just started moving, put it back in the cell it came from
  if (self->move_cooldown == 8) {
    self->pos = Position_neighbor(self->pos, Direction_back(self->direction));
    self->move_cooldown = 0;
  }
  Level_cell_add_animation(level, self->pos);
}

static void Level_remove_chip(Level* self, ChipStatus reason, Actor* also) {
  Actor* chip = Level_get_chip(self);
  switch (reason) {
    case CHIP_DROWNED:
      Level_add_sfx(self, SND_WATER_SPLASH);
      Actor_remove(chip, self, Water_Splash);
      break;
    case CHIP_BOMBED:
      Level_add_sfx(self, SND_BOMB_EXPLODES);
      Actor_remove(chip, self, Bomb_Explosion);
      break;
    case CHIP_OUTOFTIME:
      Actor_remove(chip, self, Entity_Explosion);
      break;
    case CHIP_BURNED:
      Level_add_sfx(self, SND_CHIP_LOSES);
      Actor_remove(chip, self, Entity_Explosion);
      break;
    case CHIP_COLLIDED:
      Level_add_sfx(self, SND_CHIP_LOSES);
      Actor_remove(chip, self, Entity_Explosion);
      if (also && also != chip) {
        Actor_remove(also, self, Entity_Explosion);
      }
      break;
  }
  Level_stop_terrain_sfx(self);
  Level_start_endgame(self);
}

static void Actor_erase_animation(Actor* self, Level* level) {
  self->hidden = true;
  Level_cell_remove_animation(level, self->pos);
  if (self == level->lx_state.last_actor) {
    self->id = Nothing;
    level->lx_state.last_actor -= 1;
  }
}

static void Actor_set_forced_move(Actor* self, Direction dir) {
  self->state &= ~CS_FDIRMASK;
  self->state |= dir;
}

static Direction Actor_get_forced_move(const Actor* self) {
  return self->state & CS_FDIRMASK;
}

static Direction Slide_get_forced_direction(TileID self,
                                            Level* level,
                                            bool advance_rff) {
  switch (self) {
    case Slide_North:
      return DIRECTION_NORTH;
    case Slide_West:
      return DIRECTION_WEST;
    case Slide_South:
      return DIRECTION_SOUTH;
    case Slide_East:
      return DIRECTION_EAST;
    case Slide_Random:
      if (advance_rff) {
        level->rff_dir = Direction_right(level->rff_dir);
      }
      return level->rff_dir;
    default:
      return DIRECTION_NIL;
  }
}

static Direction Ice_get_turned_dir(TileID self, Direction dir) {
  if (self == Ice)
    return Direction_back(dir);
  Direction vert_dir = self == IceWall_Southwest || self == IceWall_Southeast
                           ? DIRECTION_SOUTH
                           : DIRECTION_NORTH;
  Direction horiz_dir = self == IceWall_Southwest || self == IceWall_Northwest
                            ? DIRECTION_WEST
                            : DIRECTION_EAST;
  if (dir == vert_dir)
    return Direction_back(horiz_dir);
  if (dir == horiz_dir)
    return Direction_back(vert_dir);
  return dir;
}

static uint8_t* Level_player_item_ptr(Level* level, TileID id) {
  switch (id) {
    case Key_Red:
    case Door_Red:
      return &level->player_keys[0];
    case Key_Blue:
    case Door_Blue:
      return &level->player_keys[1];
    case Key_Yellow:
    case Door_Yellow:
      return &level->player_keys[2];
    case Key_Green:
    case Door_Green:
      return &level->player_keys[3];
    case Boots_Ice:
    case Ice:
    case IceWall_Northwest:
    case IceWall_Northeast:
    case IceWall_Southwest:
    case IceWall_Southeast:
      return &level->player_boots[0];
    case Boots_Slide:
    case Slide_North:
    case Slide_West:
    case Slide_South:
    case Slide_East:
    case Slide_Random:
      return &level->player_boots[1];
    case Boots_Fire:
    case Fire:
      return &level->player_boots[2];
    case Boots_Water:
    case Water:
      return &level->player_boots[3];
    default:
      return NULL;
  }
}
static bool Level_player_has_item(const Level* level, TileID id) {
  // const-discarding pointer cast: it's okay, we don't ever write to the return
  // pointer, which is the only reason why this function isn't const-pointer'd
  return *Level_player_item_ptr((Level*)level, id) > 0;
}

static Direction Actor_calculate_forced_move(Actor* self, Level* level) {
  if (level->current_tick == 0)
    return DIRECTION_NIL;
  TileID terrain = Level_cell_get_top_terrain(level, self->pos);
  if (TileID_is_ice(terrain)) {
    if (self->id == Chip &&
        (Level_player_has_item(level, Boots_Ice) || level->lx_state.chip_stuck))
      return DIRECTION_NIL;
    if (self->direction == DIRECTION_NIL)
      return DIRECTION_NIL;
    return self->direction;
  } else if (TileID_is_slide(terrain)) {
    if (self->id == Chip && Level_player_has_item(level, Boots_Slide))
      return DIRECTION_NIL;
    // FF overrides are now handled separately
    return Slide_get_forced_direction(terrain, level, true);
  } else if (self->state & CS_TELEPORTED) {
    self->state &= ~CS_TELEPORTED;
    return self->direction;
  }
  return DIRECTION_NIL;
}

static Direction TileID_get_exit_impeding_directions(TileID self) {
  switch (self) {
    case Wall_North:
      return DIRECTION_NORTH;
    case Wall_West:
      return DIRECTION_WEST;
    case Wall_South:
      return DIRECTION_SOUTH;
    case Wall_East:
      return DIRECTION_EAST;
    case Wall_Southeast:
      return DIRECTION_SOUTH | DIRECTION_EAST;
    case IceWall_Northwest:
      return DIRECTION_SOUTH | DIRECTION_EAST;
    case IceWall_Northeast:
      return DIRECTION_SOUTH | DIRECTION_WEST;
    case IceWall_Southwest:
      return DIRECTION_NORTH | DIRECTION_EAST;
    case IceWall_Southeast:
      return DIRECTION_NORTH | DIRECTION_WEST;
    default:
      return DIRECTION_NIL;
  };
}

static bool TileID_impedes_move_into(TileID self,
                                 const Level* level,
                                 const Actor* actor,
                                 Direction dir) {
  switch (self) {
    case Wall:
    case HiddenWall_Perm:
    case SwitchWall_Closed:
    case CloneMachine:
    case Block_Static:
    case Drowned_Chip:
    case Burned_Chip:
    case Exited_Chip:
    case Exit_Extra_1:
    case Exit_Extra_2:
    case Overlay_Buffer:
    case Floor_Reserved2:
    case Floor_Reserved1:
      return true;
    case Gravel:
      return actor->id != Chip && actor->id != Block;
    case Dirt:
    case Burglar:
    case HintButton:
    case HiddenWall_Temp:
    case BlueWall_Fake:
    case BlueWall_Real:
    case PopupWall:
    case Exit:
    case ICChip:
    case Key_Yellow:
    case Key_Green:
    case Boots_Slide:
    case Boots_Ice:
    case Boots_Water:
    case Boots_Fire:
      return actor->id != Chip;
    case Socket:
      return actor->id != Chip || level->chips_left > 0;
    case Door_Red:
    case Door_Blue:
    case Door_Green:
    case Door_Yellow:
      return actor->id != Chip || !Level_player_has_item(level, self);
    case Fire:
      return actor->id != Chip && actor->id != Block && actor->id != Fireball;
    case IceWall_Northwest:
      return dir & (DIRECTION_SOUTH | DIRECTION_EAST);
    case IceWall_Northeast:
      return dir & (DIRECTION_SOUTH | DIRECTION_WEST);
    case IceWall_Southwest:
      return dir & (DIRECTION_NORTH | DIRECTION_EAST);
    case IceWall_Southeast:
    case Wall_Southeast:
      return dir & (DIRECTION_NORTH | DIRECTION_WEST);
    case Wall_North:
      return dir == DIRECTION_SOUTH;
    case Wall_East:
      return dir == DIRECTION_EAST;
    case Wall_South:
      return dir == DIRECTION_NORTH;
    case Wall_West:
      return dir == DIRECTION_EAST;
    default:
      return false;
  }
}

enum FindActorFlags {
  FA_NO_CHIP = 0x01,
  FA_ANIMS = 0x02,
};

static Actor* Level_find_actor(const Level* self, Position pos, uint8_t flags) {
  Actor* actors = self->actors;
  if (flags & FA_NO_CHIP) {
    actors += 1;
  }
  for (Actor* actor = actors; actor->id != Nothing; actor += 1) {
    if (actor->pos == pos && !actor->hidden &&
        ((flags & FA_ANIMS) == TileID_is_animation(actor->id)))
      return actor;
  }
  return NULL;
}

static bool Actor_can_be_pushed(Actor* self,
                                Level* level,
                                Direction dir,
                                uint8_t flags);

static bool Actor_check_collision(const Actor* self,
                                  Level* level,
                                  Direction dir,
                                  uint8_t flags) {
  assert(self != NULL);
  assert(dir != DIRECTION_NIL);
  if (self->move_cooldown)
    return false;
  // Exit collision check
  TileID this_terrain = Level_cell_get_top_terrain(level, self->pos);
  Direction exit_dirs_blocked =
      TileID_get_exit_impeding_directions(this_terrain);
  if (exit_dirs_blocked & dir)
    return false;
  if ((this_terrain == Beartrap || this_terrain == CloneMachine) &&
      !(flags & CMM_RELEASING))
    return false;
  // Can't go backwards on force floors
  if (TileID_is_slide(this_terrain) &&
      !(self->id == Chip && Level_player_has_item(level, Boots_Slide)) &&
      Slide_get_forced_direction(this_terrain, level, false) ==
          Direction_back(dir))
    return false;
  int8_t x = self->pos % MAP_WIDTH;
  int8_t y = self->pos / MAP_HEIGHT;
  // Can't just use Position_neighbor since that'd wrap when x is 31 and we're
  // going right
  y += dir == DIRECTION_NORTH ? -1 : dir == DIRECTION_SOUTH ? +1 : 0;
  x += dir == DIRECTION_WEST ? -1 : dir == DIRECTION_EAST ? +1 : 0;
  if (x < 0 || x >= MAP_WIDTH)
    return false;
  if (y < 0 || y >= MAP_HEIGHT) {
    if (level->lx_state.pedantic_mode && (flags & CMM_STARTMOVEMENT)) {
      level->lx_state.map_breached = true;
      // warn("map breach in pedantic mode at (%d %d)", x, y);
    }
    return false;
  }
  Position target_pos = x + y * MAP_WIDTH;
  // Check terrain
  TileID new_terrain = Level_cell_get_top_terrain(level, target_pos);
  if (new_terrain == SwitchWall_Closed || new_terrain == SwitchWall_Open) {
    new_terrain ^= level->lx_state.toggle_walls_xor;
  }
  if (TileID_impedes_move_into(new_terrain, level, self, dir))
    return false;
  // Check actor
  if (Level_cell_has_animation(level, target_pos)) {
    if (self->id == Chip)
      return false;
    if (flags & CMM_CLEARANIMATIONS) {
      Actor* anim = Level_find_actor(level, target_pos, FA_ANIMS);
      Actor_erase_animation(anim, level);
    }
  }
  if (Level_cell_has_claim(level, target_pos)) {
    if (self->id != Chip)
      return false;
    Actor* other = Level_find_actor(level, target_pos, FA_NO_CHIP);
    if (other && other->id == Block) {
      if (!Actor_can_be_pushed(other, level, dir, flags & ~CMM_RELEASING))
        return false;
    }
  }
  // These walls turn into real walls, but I guess we have to do this after the
  // actor check?
  // TODO: Can we not just put this in `TileID_impedes_actor`?
  if (self->id == Chip &&
      (new_terrain == HiddenWall_Temp || new_terrain == BlueWall_Real)) {
    level->map[target_pos].top.id = Wall;
    return false;
  }
  return true;
}

enum {
  TRIRES_DIED = -1,
  TRIRES_FAILED = 0,
  TRIRES_SUCCESS = +1,
};
typedef int8_t TriRes;

static TriRes Actor_start_moving_to(Actor* self, Level* level, bool releasing) {
  assert(!Actor_is_moving(self));
  Direction move_dir;
  if (self->move_decision) {
    move_dir = self->move_decision;
  } else if (Actor_get_forced_move(self)) {
    move_dir = Actor_get_forced_move(self);
  } else {
    return TRIRES_FAILED;
  }
  assert(!Direction_is_diagonal(move_dir));
  self->direction = move_dir;

  TileID from_terrain = Level_cell_get_top_terrain(level, self->pos);

  if (self->id == Chip && !Level_player_has_item(level, Boots_Slide)) {
    if (TileID_is_slide(from_terrain) && self->move_decision == DIRECTION_NIL) {
      self->state |= CS_SLIDETOKEN;
    } else if (!TileID_is_ice(from_terrain) ||
               Level_player_has_item(level, Boots_Ice)) {
      self->state &= ~CS_SLIDETOKEN;
    }
  }
  if (!Actor_check_collision(self, level, move_dir,
                             CMM_PUSHBLOCKSNOW | CMM_CLEARANIMATIONS |
                                 CMM_STARTMOVEMENT |
                                 (releasing ? CMM_RELEASING : 0))) {
    // Show player bonks and play the SFX if we haven't bonk already
    if (self->id == Chip) {
      if (!level->lx_state.chip_bonked) {
        level->lx_state.chip_bonked = true;
        Level_add_sfx(level, SND_CANT_MOVE);
      }
      level->lx_state.chip_pushing = true;
    }
    // If we bonked while on ice, turn around
    if (TileID_is_ice(from_terrain) &&
        !(self->id == Chip && Level_player_has_item(level, Boots_Ice))) {
      self->direction = Ice_get_turned_dir(from_terrain, self->direction);
    }
    return TRIRES_FAILED;
  }

  if (level->lx_state.map_breached && (Level_get_chip(level)->id == Chip)) {
    Level_remove_chip(level, CHIP_COLLIDED, self);
    return TRIRES_DIED;
  }
  assert(releasing ||
         !(from_terrain == CloneMachine || from_terrain == Beartrap));

  if (self->id != Chip) {
    // Remove the claim on the location we're about to leav
    Level_cell_remove_claim(level, self->pos);
    // NOTE: If it looks like Chip will *try* to move into out cell (and we're
    // about to leave), mark ourselves as the actor Chip is colliding with,
    // which we will use when Chip will eventually try to move, see a few lines
    // down
    if (self->id != Block && self->pos == level->lx_state.chip_predicted_pos) {
      level->lx_state.chip_colliding_actor = self;
    }
  }
  // NOTE: When there's apparently a monster that just left the cell we're
  // trying to enter, kill outselves as if we collided into them
  if (self->id == Chip && level->lx_state.chip_colliding_actor &&
      !level->lx_state.chip_colliding_actor->hidden) {
    // ???
    level->lx_state.chip_colliding_actor->move_cooldown = 8;
    Level_remove_chip(level, CHIP_COLLIDED,
                      level->lx_state.chip_colliding_actor);
    return TRIRES_DIED;
  }
  self->pos = Position_neighbor(self->pos, move_dir);
  assert(0 <= self->pos && self->pos < MAP_WIDTH * MAP_HEIGHT);
  // Why `+=` instead of `=`? Can cooldown be negative?
  self->move_cooldown += 8;

  if (self->id != Chip) {
    // Add the claim for the new location we're in
    Level_cell_add_claim(level, self->pos);
    // If we're now at Chip's cell, kill 'em
    Actor* chip = Level_get_chip(level);
    if (self->pos == chip->pos && !chip->hidden) {
      Level_remove_chip(level, CHIP_COLLIDED, self);
      return TRIRES_DIED;
    }
  } else {
    level->lx_state.chip_bonked = false;
    // If we entered an actor's cell, kill ourselves
    Actor* monster = Level_find_actor(level, self->pos, FA_NO_CHIP);
    if (monster) {
      Level_remove_chip(level, CHIP_COLLIDED, monster);
      return TRIRES_DIED;
    }
  }

  // If *any* block is pushed, make Chip show the pushing animation
  if (self->state & CS_PUSHED) {
    level->lx_state.chip_pushing = true;
    Level_add_sfx(level, SND_BLOCK_MOVING);
  }
  return TRIRES_SUCCESS;
}

static TriRes Actor_enter_tile(Actor* self, Level* level, bool pedantic_idle) {
  // TODO: INCOMPLETE!!!
}

/**
 * Returns `true` if the actor has still cooldown to go
 */
static TriRes Actor_reduce_cooldown(Actor* self, const Level* level) {
  if (TileID_is_animation(self->id))
    return TRIRES_SUCCESS;
  assert(self->move_cooldown > 0);

  if (self->id == Chip && level->lx_state.chip_stuck)
    return TRIRES_SUCCESS;

  uint8_t speed = 2;
  if (self->id == Blob) {
    speed /= 2;
  }
  TileID terrain = Level_cell_get_top_terrain(level, self->pos);
  if (TileID_is_slide(terrain) &&
      !(self->id == Chip && Level_player_has_item(level, Boots_Slide))) {
    speed *= 2;
  }
  if (TileID_is_ice(terrain) &&
      !(self->id == Chip && Level_player_has_item(level, Boots_Ice))) {
    speed *= 2;
  }
  self->move_cooldown -= speed;
  self->animation_frame = self->move_cooldown / 2;
  return Actor_is_moving(self);
}

static TriRes Actor_advance_movement(Actor* self,
                                     Level* level,
                                     bool releasing) {
  if (TileID_is_animation(self->id))
    return TRIRES_SUCCESS;
  Direction previous_releasing_dir = DIRECTION_NIL;

  // If we aren't currently moving, start right now!
  if (!Actor_is_moving(self)) {
    if (releasing) {
      assert(self->direction != DIRECTION_NIL);
      previous_releasing_dir = self->move_decision;
      self->move_decision = self->direction;
    }
    // If we don't have any direction we want to go on, don't do anything
    // (except for idling on the tile when in pedantic mode)
    if (self->move_decision == DIRECTION_NIL &&
        Actor_get_forced_move(self) == DIRECTION_NIL) {
      if (level->lx_state.pedantic_mode &&
          Actor_enter_tile(self, level, true) == TRIRES_DIED) {
        return TRIRES_DIED;
      }
      return TRIRES_SUCCESS;
    }
    TriRes start_res = Actor_start_moving_to(self, level, releasing);
    if (start_res != TRIRES_DIED) {
      self->hidden = false;
    }
    if (level->lx_state.pedantic_mode && start_res == TRIRES_FAILED &&
        Actor_enter_tile(self, level, true) != TRIRES_DIED)
      return TRIRES_DIED;
  }
  if (Actor_reduce_cooldown(self, level) == TRIRES_SUCCESS)
    return TRIRES_SUCCESS;
  return Actor_enter_tile(self, level, false);
}

static bool Actor_can_be_pushed(Actor* self,
                                Level* level,
                                Direction dir,
                                uint8_t flags) {
  assert(self && self->id == Block);
  assert(Level_cell_get_top_terrain(level, self->pos) != CloneMachine);
  assert(dir != DIRECTION_NIL);
  if (!Actor_check_collision(self, level, dir, flags)) {
    if (!Actor_is_moving(self) &&
        (flags & (CMM_PUSHBLOCKS | CMM_PUSHBLOCKSNOW))) {
      self->direction = dir;
      if (level->lx_state.pedantic_mode) {
        self->move_decision = dir;
      }
    }
    return false;
  }
  if (flags & (CMM_PUSHBLOCKS | CMM_PUSHBLOCKSNOW)) {
    self->direction = dir;
    self->move_decision = dir;
    self->state |= CS_PUSHED;
    if (flags & CMM_PUSHBLOCKSNOW) {
      Actor_advance_movement(self, level, false);
    }
  }
  return true;
}

static const int clockwise_directions[4] = {DIRECTION_NORTH, DIRECTION_EAST,
                                            DIRECTION_SOUTH, DIRECTION_WEST};

static void Actor_get_checked_decision_dirs(Actor* self,
                                            Level* level,
                                            Direction choices[4]) {
  switch (self->id) {
    case Tank:
      choices[0] = self->direction;
      break;
    case Ball:
      choices[0] = self->direction;
      choices[1] = Direction_back(self->direction);
      break;
    case Glider:
      choices[0] = self->direction;
      choices[1] = Direction_left(self->direction);
      choices[2] = Direction_right(self->direction);
      choices[3] = Direction_back(self->direction);
      break;
    case Fireball:
      choices[0] = self->direction;
      choices[1] = Direction_right(self->direction);
      choices[2] = Direction_left(self->direction);
      choices[3] = Direction_back(self->direction);
      break;
    case Bug:
      choices[0] = Direction_left(self->direction);
      choices[1] = self->direction;
      choices[2] = Direction_right(self->direction);
      choices[3] = Direction_back(self->direction);
      break;
    case Paramecium:
      choices[0] = Direction_right(self->direction);
      choices[1] = self->direction;
      choices[2] = Direction_left(self->direction);
      choices[3] = Direction_back(self->direction);
      break;
    case Walker:
      if (Actor_check_collision(self, level, self->direction,
                                CMM_CLEARANIMATIONS)) {
        self->move_decision = self->direction;
        return;
      }
      Direction checked_dir = self->direction;
      uint8_t rotate_n = Level_lynx_rng(level) & 3;
      while (rotate_n > 0) {
        checked_dir = Direction_right(checked_dir);
        rotate_n -= 1;
      }

      choices[0] = checked_dir;
      break;
    case Blob:
      choices[0] = clockwise_directions[Prng_random4(&level->prng)];
      break;
    case Teeth:
      if ((level->current_tick + level->init_step_parity) & 4)
        return;
      Position chip_pos = Level_get_chip(level)->pos;
      int8_t dx = chip_pos % MAP_WIDTH - self->pos % MAP_WIDTH;
      int8_t dy = chip_pos / MAP_WIDTH - self->pos / MAP_WIDTH;
      Direction horiz_dir = dx < 0   ? DIRECTION_WEST
                            : dx > 0 ? DIRECTION_EAST
                                     : DIRECTION_NIL;
      if (dx < 0) {
        dx = -dx;
      }
      Direction vert_dir = dy < 0   ? DIRECTION_NORTH
                           : dy > 0 ? DIRECTION_SOUTH
                                    : DIRECTION_NIL;
      if (dy < 0) {
        dy = -dy;
      }
      if (dx > dy) {
        choices[0] = horiz_dir;
        choices[1] = vert_dir;
        choices[2] = horiz_dir;
      } else {
        choices[0] = vert_dir;
        choices[1] = horiz_dir;
        choices[2] = vert_dir;
      }
      break;
    default:
      break;
  }
}

static void Chip_do_decision(Actor* self, Level* level) {
  level->lx_state.chip_pushing = false;
  self->move_decision = DIRECTION_NIL;

  bool can_move = true;

  // If the current input is non-directoinal, eg. a mouse move, OR we're
  // "stuck", don't move
  Direction move_dir = GameInput_is_directional(level->game_input)
                           ? (Direction)level->game_input
                           : DIRECTION_NIL;
  if (move_dir == DIRECTION_NIL || level->lx_state.chip_stuck)
    can_move = false;

  // Can we override the current forced move?
  TileID terrain = Level_cell_get_top_terrain(level, self->pos);
  bool can_override = TileID_is_slide(terrain) && (self->state & CS_SLIDETOKEN);
  Direction forced_move = Actor_get_forced_move(self);
  if (forced_move != DIRECTION_NIL && !can_override)
    can_move = false;

  if (!can_move) {
    // Do nothing!
  } else if (!Direction_is_diagonal(move_dir)) {
    // If we're holding an orthogonal direction, just make a collision check
    // there and use that as our decision regardless of if it succeeds or not
    Actor_check_collision(self, level, move_dir, CMM_PUSHBLOCKS);
    self->move_decision = move_dir;
  } else {
    if (!(self->direction & move_dir)) {
      // If we're trying to move in a diagonal neither of which is our current
      // direction, pick the horizonal direction unless it's blocked
      Direction horiz_dir = move_dir & (DIRECTION_WEST | DIRECTION_EAST);
      Direction vert_dir = move_dir & (DIRECTION_NORTH | DIRECTION_SOUTH);
      bool can_go_horiz =
          Actor_check_collision(self, level, horiz_dir, CMM_PUSHBLOCKS);
      self->move_decision = can_go_horiz ? horiz_dir : vert_dir;
    } else {
      // If one of the dirs is out current one, prefer that one, and pick the
      // other if and only if it's available and our current dir is not
      Direction current_dir = self->direction;
      // A diagonal move is two bits in the directions bitfield set, if we XOR
      // (or AND NOT) the current direction out of it, we are only left with the
      // other direction
      Direction other_dir = move_dir ^ self->direction;
      bool can_go_current =
          Actor_check_collision(self, level, current_dir, CMM_PUSHBLOCKS);
      bool can_go_other =
          Actor_check_collision(self, level, other_dir, CMM_PUSHBLOCKS);
      self->move_decision =
          !can_go_current && can_go_other ? other_dir : current_dir;
    }
  }
  if (self->move_decision == DIRECTION_NIL && forced_move == DIRECTION_NIL) {
    Level_stop_terrain_sfx(level);
  }
  // Predict our next position (with flaws!), for the `Actor_start_moving_to`
  // tried-to-enter-just-vacated-cell nonsense
  if (self->move_decision != DIRECTION_NIL) {
    level->lx_state.chip_predicted_pos =
        Position_neighbor(self->pos, self->move_decision);
  }
}

static void Actor_do_decision(Actor* self, Level* level) {
  if (TileID_is_animation(self->id)) {
    self->animation_frame -= 1;
    if (self->animation_frame < 0) {
      Actor_erase_animation(self, level);
    }
    return;
  }
  Direction forced_move = Actor_calculate_forced_move(self, level);
  Actor_set_forced_move(self, forced_move);
  if (self == Level_get_chip(level)) {
    Chip_do_decision(self, level);
    return;
  }
  if (self->id == Block)
    return;
  self->move_decision = DIRECTION_NIL;
  if (forced_move)
    return;

  TileID terrain = Level_cell_get_top_terrain(level, self->pos);
  if (terrain == CloneMachine || terrain == Beartrap) {
    self->move_decision = self->direction;
    return;
  }
  Direction directions[4] = {DIRECTION_NIL, DIRECTION_NIL, DIRECTION_NIL,
                             DIRECTION_NIL};
  Actor_get_checked_decision_dirs(self, level, directions);
  for (uint8_t idx = 0; idx < 4; idx += 1) {
    Direction checked_dir = directions[idx];
    if (checked_dir == DIRECTION_NIL)
      return;
    self->move_decision = checked_dir;
    if (Actor_check_collision(self, level, checked_dir, CMM_CLEARANIMATIONS))
      return;
  }
}

static Position Level_find_connected_cell(const Level* self,
                                          Position from_pos,
                                          TileID target_id,
                                          ConnList* list) {
  // In pedantic mode, connections can only be in reading order, so search
  // manually instead of relying on the list
  if (self->lx_state.pedantic_mode) {
    for (uint16_t offset = 1; offset < MAP_WIDTH * MAP_HEIGHT; offset += 1) {
      Position searched_pos = (from_pos + offset) % (MAP_WIDTH * MAP_HEIGHT);
      TileID terrain = Level_cell_get_top_terrain(self, searched_pos);
      if (terrain == target_id) {
        return searched_pos;
      }
    }
    return POSITION_NULL;
  }
  // In the usual mode, scan the conn list
  for (size_t idx = 0; idx < list->length; idx += 1) {
    TileConn conn = list->items[idx];
    if (conn.from == from_pos)
      return conn.to;
  }
  return POSITION_NULL;
}

static void Level_activate_trap(Level* self, Position pos) {
  assert(pos != POSITION_NULL);
  if (Level_cell_get_top_terrain(self, pos) != Beartrap) {
    assert(false && "Can't activate a cell with no trap!");
    return;
  }
  Actor* actor = Level_find_actor(self, pos, 0);
  if (actor && actor->direction != DIRECTION_NIL) {
    Actor_advance_movement(actor, self, true);
  }
}

/*
 * Teleport ourselves to the next valid teleport in reverse reading order
 */
static void Actor_teleport(Actor* self, Level* level) {
  Position start_pos = self->pos;
  Position checked_pos = start_pos;
  Actor* chip = Level_get_chip(level);
  while (true) {
    if (checked_pos == 0) {
      checked_pos = MAP_WIDTH * MAP_HEIGHT;
    }
    checked_pos -= 1;
    TileID terrain = Level_cell_get_top_terrain(level, checked_pos);
    if (terrain == Teleport) {
      // NOTE: Intentional bug: if a non-Chip actor fails a teleport check due
      // to that cell already being occupied by an actor, the occupier's claim
      // on the cell is ***removed, without the actor itself being removed***
      // due to the teleportee's position still being set to the position of the
      // occupier,
      if (self->id != Chip) {
        Level_cell_remove_claim(level, self->pos);
      }
      self->pos = checked_pos;
    } else if (Level_cell_ever_had_teleport(level, checked_pos)) {
      // Pedantic Lynx only: if there was a teleport on this cell, but due to a
      // monster standing on a recessed wall, it was overwritten
      Level_cell_set_top_terrain(level, checked_pos, Teleport);
      if (checked_pos == chip->pos) {
        chip->hidden = true;
      }
    }
  }
  // TODO: INCOMPLETE!!! (POSSIBLY, PLEASE CHECK)
}

static void lynx_tick_level(Level* self) {
  Actor* chip = Level_get_chip(self);
  if (chip->id == Pushing_Chip) {
    chip->id = Chip;
  }
  if (!Level_in_endgame(self)) {
    if (self->level_complete) {
      Level_start_endgame(self);
    } else if (self->time_limit && self->current_tick >= self->time_limit) {
      Level_remove_chip(self, CHIP_OUTOFTIME, NULL);
    }
  }
  for (Actor* actor = self->actors; actor->id != Nothing; actor += 1) {
    if (actor->hidden || !(actor->state & CS_REVERSE))
      continue;
    actor->state &= ~CS_REVERSE;
    if (!Actor_is_moving(actor)) {
      actor->direction = Direction_back(actor->direction);
    }
  }
  for (Actor* actor = self->actors; actor->id != Nothing; actor += 1) {
    if (!(actor->state & CS_PUSHED))
      continue;
    if (actor->hidden || !Actor_is_moving(actor)) {
      Level_stop_sfx(self, SND_BLOCK_MOVING);
      actor->state &= ~CS_PUSHED;
    }
  }
  if (self->lx_state.toggle_walls_xor) {
    for (Position pos = 0; pos < MAP_WIDTH * MAP_HEIGHT; pos += 1) {
      MapCell* cell = &self->map[pos];
      if (cell->top.id == SwitchWall_Open ||
          cell->top.id == SwitchWall_Closed) {
        cell->top.id ^= self->lx_state.toggle_walls_xor;
      }
    }
    self->lx_state.toggle_walls_xor = 0;
  }
  self->lx_state.chip_predicted_pos = POSITION_NULL;
  self->lx_state.chip_colliding_actor = NULL;
  // Decision/intent phase: all actors decide which direction to go in
  for (Actor* actor = self->lx_state.last_actor; actor >= self->actors;
       actor -= 1) {
    if (actor != chip && actor->hidden)
      continue;
    if (Actor_is_moving(actor))
      continue;
    Actor_do_decision(actor, self);
  }
  // Movement phase: all actors try to move in their predetermined directions
  for (Actor* actor = self->lx_state.last_actor; actor >= self->actors;
       actor -= 1) {
    if (actor == chip && self->level_complete)
      continue;
    if (actor != chip && actor->hidden)
      continue;
    TriRes move_res = Actor_advance_movement(actor, self, false);
    if (move_res == TRIRES_DIED)
      continue;
    actor->move_decision = DIRECTION_NIL;
    Actor_set_forced_move(actor, DIRECTION_NIL);
    TileID terrain = Level_cell_get_top_terrain(self, actor->pos);
    // In pedantic Lynx, the last actor on a popup wall decides which popup wall
    // is actually, well, popped
    if (actor != chip && self->lx_state.pedantic_mode && terrain == PopupWall) {
      self->lx_state.to_place_wall_pos = actor->pos;
    }
    // We also activate traps at this point
    if (terrain == Button_Brown && !Actor_is_moving(actor)) {
      Position linked_pos = Level_find_connected_cell(
          self, actor->pos, Beartrap, &self->trap_connections);
      if (linked_pos != POSITION_NULL) {
        Level_activate_trap(self, linked_pos);
      }
    }
  }
  // Teleport phase: teleport actors on teleports
  for (Actor* actor = self->lx_state.last_actor; actor >= self->actors;
       actor -= 1) {
    if (actor->hidden || Actor_is_moving(actor))
      continue;
    TileID terrain = Level_cell_get_top_terrain(self, actor->pos);
    if (terrain != Teleport)
      continue;
    Actor_teleport(actor, self);
  }
  // TODO: INCOMPLETE!!!
}
static void lynx_uninit_level(Level* level) {
  free(level->actors);
}

const Ruleset lynx_logic = {.id = Ruleset_Lynx,
                            .init_level = lynx_init_level,
                            .tick_level = lynx_tick_level,
                            .uninit_level = lynx_uninit_level};
