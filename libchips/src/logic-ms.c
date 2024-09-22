#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logic.h"
#include "misc.h"

static bool Actor_advance_movement(Actor* self, Level* level, Direction dir);

/* Creature state flags.
 */
enum ActorState {
  CS_RELEASED = 0x0001,  /* can leave a beartrap */
  CS_CLONING = 0x0002,   /* cannot move this tick */
  CS_HASMOVED = 0x0004,  /* already used current move */
  CS_TURNING = 0x0008,   /* is turning around */
  CS_SLIP = 0x0010,      /* is on the slip list */
  CS_SLIDE = 0x0020,     /* is on the slip list but can move */
  CS_DEFERPUSH = 0x0040, /* button pushes will be delayed */
  CS_MUTANT = 0x0080,    /* block is mutant, looks like Chip */
  CS_SDIRMASK = 0x0F00,  /* creature needs to store another direction (currently
                            used for tank top glitch) */
  CS_SPONTANEOUS =
      0x1000, /* creature has potential to spontaneously generate */
};

enum {
  CS_SDIRSHIFT = 8,
};

static void Actor_set_spare_direction(Actor* self, Direction dir) {
  self->state &= ~CS_SDIRMASK;
  self->state |= dir << CS_SDIRSHIFT;
}

static Direction Actor_get_spare_direction(Actor const* self) {
  return self->state & CS_SDIRMASK >> CS_SDIRSHIFT;
}

/* Including the flag CMM_NOLEAVECHECK in a call to canmakemove()
 * indicates that the tile the creature is moving out of is
 * automatically presumed to permit such movement. CMM_NOEXPOSEWALLS
 * causes blue and hidden walls to remain unexposed.
 * CMM_CLONECANTBLOCK means that the creature will not be prevented
 * from moving by an identical creature standing in the way.
 * CMM_NOPUSHING prevents Chip from pushing blocks inside this
 * function. CMM_TELEPORTPUSH indicates to the block-pushing logic
 * that Chip is teleporting. This prevents a stack of two blocks from
 * being treated as a single block, and allows Chip to push a slipping
 * block away from him. CMM_NOFIRECHECK causes bugs and walkers to not
 * avoid fire. Finally, CMM_NODEFERBUTTONS causes buttons pressed by
 * pushed blocks to take effect immediately.
 */
enum CollisionCheckFlags {
  CMM_NOLEAVECHECK = 0x0001,
  CMM_NOEXPOSEWALLS = 0x0002,
  CMM_CLONECANTBLOCK = 0x0004,
  CMM_NOPUSHING = 0x0008,
  CMM_TELEPORTPUSH = 0x0010,
  CMM_NOFIRECHECK = 0x0020,
  CMM_NODEFERBUTTONS = 0x0040,

  CMM_ALL = CMM_NOLEAVECHECK | CMM_NOEXPOSEWALLS | CMM_CLONECANTBLOCK |
            CMM_NOPUSHING | CMM_TELEPORTPUSH | CMM_NOFIRECHECK |
            CMM_NODEFERBUTTONS,
};

/* Floor state flags.
 */
enum TerrainState {
  FS_BUTTONDOWN = 0x01, /* button press is deferred */
  FS_CLONING = 0x02,    /* clone machine is activated */
  FS_BROKEN = 0x04,     /* teleport/toggle wall doesn't work */
  FS_HASMUTANT = 0x08,  /* beartrap contains mutant block */
  FS_MARKER = 0x10,     /* marker used during initialization */
};

static inline MapCell* Level_get_map_cell(Level* self, Position pos) {
  return &self->map[pos];
}

static inline MapTile* MapCell_get_top_tile(MapCell* self) {
  return &self->top;
}

static inline MapTile* MapCell_get_bottom_tile(MapCell* self) {
  return &self->bottom;
}

static inline TileID MapCell_get_top_floor(MapCell const* self) {
  return self->top.id;
}

static inline TileID MapCell_get_bottom_floor(MapCell const* self) {
  return self->bottom.id;
}

static inline TileID MapCell_set_top_floor(MapCell* self, TileID tile) {
  return self->top.id = tile;
}

static inline TileID MapCell_set_bottom_floor(MapCell* self, TileID tile) {
  return self->bottom.id = tile;
}

static inline MapTile MapCell_pop_tile(MapCell* self) {
  MapTile tile = self->top;
  self->top = self->bottom;
  self->bottom.id = Empty;
  self->bottom.state = 0;
  return tile;
}

static inline void MapCell_push_tile(MapCell* self, MapTile tile) {
  self->bottom = self->top;
  self->top = tile;
}

static inline TileID MapTile_get_floor(MapTile const* self) {
  return self->id;
}

static inline TileID MapTile_set_floor(MapTile* self, TileID tile) {
  return self->id = tile;
}

static inline TileID MapTile_get_state(MapTile const* self) {
  return self->state;
}

static inline void MapTile_clear_state(MapTile* self) {
  self->state = 0;
}

static inline void MapTile_add_button_down_state(MapTile* self) {
  self->state |= FS_BUTTONDOWN;
}

static inline void MapTile_remove_button_down_state(MapTile* self) {
  self->state &= ~FS_BUTTONDOWN;
}

static inline void MapTile_add_cloning_state(MapTile* self) {
  self->state |= FS_CLONING;
}

static inline void MapTile_remove_cloning_state(MapTile* self) {
  self->state &= ~FS_CLONING;
}

static inline void MapTile_add_broken_state(MapTile* self) {
  self->state |= FS_BROKEN;
}

static inline void MapTile_remove_broken_state(MapTile* self) {
  self->state &= ~FS_BROKEN;
}

static inline void MapTile_add_mutant_state(MapTile* self) {
  self->state |= FS_HASMUTANT;
}

static inline void MapTile_remove_mutant_state(MapTile* self) {
  self->state &= ~FS_HASMUTANT;
}

static inline void MapTile_add_marker_state(MapTile* self) {
  self->state |= FS_MARKER;
}

static inline void MapTile_remove_marker_state(MapTile* self) {
  self->state &= ~FS_MARKER;
}

static inline TileID Level_cell_get_top_floor(Level const* self, Position pos) {
  return self->map[pos].top.id;
}

static inline void Level_cell_set_top_floor(Level* self,
                                            Position pos,
                                            TileID tile) {
  self->map[pos].top.id = tile;
}

static inline TileID Level_cell_get_bottom_floor(Level const* self,
                                                 Position pos) {
  return self->map[pos].bottom.id;
}

static inline void Level_cell_set_bottom_floor(Level* self,
                                               Position pos,
                                               TileID tile) {
  self->map[pos].bottom.id = tile;
}

/* Return the terrain tile found at the given location.
 */
static inline TileID Level_cell_get_terrain(Level const* self, Position pos) {
  MapCell const* cell = &self->map[pos];
  if (!TileID_is_key(cell->top.id) && !TileID_is_boots(cell->top.id) &&
      !TileID_is_actor(cell->top.id))
    return cell->top.id;
  if (!TileID_is_key(cell->bottom.id) && !TileID_is_boots(cell->bottom.id) &&
      !TileID_is_actor(cell->bottom.id))
    return cell->bottom.id;
  return Empty;
}

static inline void Level_cell_set_terrain(Level* self,
                                          Position pos,
                                          TileID tile) {
  MapCell* cell = &self->map[pos];
  if (!TileID_is_key(cell->top.id) && !TileID_is_boots(cell->top.id) &&
      !TileID_is_actor(cell->top.id))
    cell->top.id = tile;
  else if (!TileID_is_key(cell->bottom.id) &&
           !TileID_is_boots(cell->bottom.id) &&
           !TileID_is_actor(cell->bottom.id))
    cell->bottom.id = tile;
  else
    cell->bottom.id = tile;  // idk why but this is technically how TW does it
}

static inline Actor* Level_get_chip(Level* self) {
  return &self->actors[0];
}

static inline Position Level_get_mouse_goal(Level const* self) {
  return self->ms_state.mouse_goal;
}

static inline Position Level_has_mouse_goal(Level const* self) {
  return self->ms_state.mouse_goal >= 0;
}

static inline void Level_set_mouse_goal(Level* self, Position goal) {
  self->ms_state.mouse_goal = goal;
}

static inline bool Level_cancel_mouse_goal(Level* self) {
  self->ms_state.mouse_goal = POSITION_NULL;
  return true;
}

/* Return TRIRES_DIED or TRIRES_SUCCESS if gameplay is over.
 */
static TriRes Level_check_for_ending(Level* self) {
  if (self->ms_state.chip_status != CHIP_OKAY &&
      self->ms_state.chip_status != CHIP_SQUISHED) { /* Squish patch */
    if (self->win_state != TRIRES_DIED) {
      Level_add_sfx(self, SND_CHIP_LOSES);
    }
    self->win_state = TRIRES_DIED;
  } else if (self->level_complete) {
    if (self->win_state != TRIRES_SUCCESS) {
      Level_add_sfx(self, SND_CHIP_WINS);
    }
    self->win_state = TRIRES_SUCCESS;
  }
  return self->win_state;
}

/* Empty the list of "active" blocks.
 */
static void Level_reset_block_list(Level* self) {
  self->ms_state.block_list_count = 0;
}

/* Append the given block to the end of the block list.
 */
static Actor* Level_add_to_block_list(Level* self, Actor* block) {
  self->ms_state.block_list[self->ms_state.block_list_count] = block;
  self->ms_state.block_list_count++;
  return block;
}

/* Empty the list of sliding creatures.
 */
static void Level_reset_sliplist(Level* self) {
  self->ms_state.slip_count = 0;
}

/* Append the given creature to the end of the slip list.
 */
static Actor* Level_append_to_slip_list(Level* self,
                                        Actor* actor,
                                        Direction direction) {
  for (uint32_t n = 0; n < self->ms_state.slip_count; ++n) {
    if (self->ms_state.slip_list[n].actor == actor) {
      self->ms_state.slip_list[n].direction = direction;
      return actor;
    }
  }

  self->ms_state.slip_list[self->ms_state.slip_count].actor = actor;
  self->ms_state.slip_list[self->ms_state.slip_count].direction = direction;
  self->ms_state.slip_count++;
  self->ms_state.mscc_slippers++; /* new accounting */
  return actor;
}

/* Add the given creature to the start of the slip list.
 */
static Actor* Level_prepend_to_slip_list(Level* self,
                                         Actor* actor,
                                         Direction direction) {
  if (self->ms_state.slip_count && self->ms_state.slip_list[0].actor == actor) {
    self->ms_state.slip_list[0].direction = direction;
    return actor;
  }

  for (uint32_t n = self->ms_state.slip_count; n; --n)
    self->ms_state.slip_list[n] = self->ms_state.slip_list[n - 1];
  self->ms_state.slip_count++;
  self->ms_state.slip_list[0].actor = actor;
  self->ms_state.slip_list[0].direction = direction;
  return actor;
}

/* Return the sliding direction of a creature on the slip list.
 */
static Direction Level_get_actor_slip_dir(Level const* self,
                                          Actor const* actor) {
  for (uint32_t n = 0; n < self->ms_state.slip_count; ++n)
    if (self->ms_state.slip_list[n].actor == actor)
      return self->ms_state.slip_list[n].direction;
  return DIRECTION_NIL;
}

/* Remove the given creature from the slip list.
 */
static void Level_remove_actor_from_slip_list(Level* self, Actor const* actor) {
  uint32_t n;

  for (n = 0; n < self->ms_state.slip_count; ++n)
    if (self->ms_state.slip_list[n].actor == actor)
      break;
  if (n == self->ms_state.slip_count)
    return;
  --self->ms_state.slip_count;
  for (; n < self->ms_state.slip_count; ++n)
    self->ms_state.slip_list[n] = self->ms_state.slip_list[n + 1];
}

/*
 * Simple floor functions.
 */

/* Translate a slide floor into the direction it points in. In the
 * case of a random slide floor, a new direction is selected.
 */
static Direction Level_get_slide_dir(Level* self, TileID floor) {
  switch (floor) {
    case Slide_North:
      return DIRECTION_NORTH;
    case Slide_West:
      return DIRECTION_WEST;
    case Slide_South:
      return DIRECTION_SOUTH;
    case Slide_East:
      return DIRECTION_EAST;
    case Slide_Random:
      return 1 << Prng_random4(&self->prng);
    default:
      return DIRECTION_NIL;
  }
}

/* Alter a creature's direction if they are at an ice wall.
 */
static Direction get_ice_wall_turn_dir(TileID floor, Direction dir) {
  switch (floor) {
    case IceWall_Northeast:
      return dir == DIRECTION_SOUTH  ? DIRECTION_EAST
             : dir == DIRECTION_WEST ? DIRECTION_NORTH
                                     : dir;
    case IceWall_Southwest:
      return dir == DIRECTION_NORTH  ? DIRECTION_WEST
             : dir == DIRECTION_EAST ? DIRECTION_SOUTH
                                     : dir;
    case IceWall_Northwest:
      return dir == DIRECTION_SOUTH  ? DIRECTION_WEST
             : dir == DIRECTION_EAST ? DIRECTION_NORTH
                                     : dir;
    case IceWall_Southeast:
      return dir == DIRECTION_NORTH  ? DIRECTION_EAST
             : dir == DIRECTION_WEST ? DIRECTION_SOUTH
                                     : dir;
    default:
      return dir;
  }
}

/* Find the location of a bear trap from one of its buttons.
 */
static Position Level_locate_trap_by_button(Level const* self,
                                            Position button_pos) {
  ConnList const* traps = &self->trap_connections;
  for (uint8_t i = 0; i < traps->length; ++i)
    if (traps->items[i].from == button_pos)
      return traps->items[i].to;
  return POSITION_NULL;
}

/* Find the location of a bear trap from one of its buttons.
 */
static Position Level_locate_cloner_by_button(Level const* self,
                                              Position button_pos) {
  ConnList const* cloners = &self->cloner_connections;
  for (uint8_t i = 0; i < cloners->length; ++i)
    if (cloners->items[i].from == button_pos)
      return cloners->items[i].to;
  return POSITION_NULL;
}

/* Return TRUE if the brown button at the give location is currently
 * held down.
 */
static bool Level_is_trap_button_down(Level const* self, Position pos) {
  return pos >= 0 && pos < MAP_WIDTH * MAP_HEIGHT &&
         Level_cell_get_top_floor(self, pos) != Button_Brown;
}

/* Return TRUE if a bear trap is currently passable.
 */
static bool Level_is_trap_open(Level* self, Position pos, Position skip_pos) {
  ConnList* traps = &self->trap_connections;
  for (uint8_t i = 0; i < traps->length; ++i) {
    if (traps->items[i].to == pos && traps->items[i].from != skip_pos &&
        Level_is_trap_button_down(self, traps->items[i].from)) {
      return true;
    }
  }
  return false;
}

/* Flip-flop the state of any toggle walls.
 */
static void Level_toggle_walls(Level* level) {
  for (Position pos = 0; pos < MAP_WIDTH * MAP_HEIGHT; ++pos) {
    MapCell* cell = Level_get_map_cell(level, pos);
    MapTile* top = MapCell_get_top_tile(cell);
    MapTile* bottom = MapCell_get_bottom_tile(cell);
    if ((MapTile_get_floor(top) == SwitchWall_Open ||
         MapTile_get_floor(top) == SwitchWall_Closed) &&
        !(MapTile_get_state(top) & FS_BROKEN)) {
      MapTile_set_floor(top, MapTile_get_floor(top) == SwitchWall_Open
                                 ? SwitchWall_Closed
                                 : SwitchWall_Open);
    }
    if ((MapTile_get_floor(bottom) == SwitchWall_Open ||
         MapTile_get_floor(bottom) == SwitchWall_Closed) &&
        !(MapTile_get_state(bottom) & FS_BROKEN)) {
      MapTile_set_floor(bottom, MapTile_get_floor(bottom) == SwitchWall_Open
                                    ? SwitchWall_Closed
                                    : SwitchWall_Open);
    }
  }
}

/*
 * Functions that manage the list of entities.
 */

static Actor* Level_create_actor(Level* level) {
  if (level->ms_state.actor_count == MAX_CREATURES) {
    warn("%d: filled the actor array (note: this should NOT be possible)",
         level->current_tick);
    return NULL;
  }
  Actor* actor = &level->actors[level->ms_state.actor_count];
  *actor = (Actor){.id = Nothing,
                   .pos = POSITION_NULL,
                   .direction = DIRECTION_NIL,
                   .move_decision = DIRECTION_NIL,
                   .state = 0,
                   .animation_frame = 0,
                   .hidden = false,
                   .move_cooldown = 0};

  level->ms_state.actor_count++;
  return actor;
}

/* Return the creature located at pos. Ignores Chip unless includechip
 * is TRUE. Return NULL if no such creature is present.
 */
static Actor* Level_look_up_creature(Level* self,
                                     Position pos,
                                     bool includechip) {
  if (!self->actors)
    return NULL;
  for (uint32_t n = 0; n < self->ms_state.actor_count; ++n) {
    if (self->actors[n].hidden)
      continue;
    if (self->actors[n].pos == pos)
      if (self->actors[n].id != Chip || includechip)
        return &self->actors[n];
  }
  return NULL;
}

/* Return the block located at pos. If the block in question is not
 * currently "active", it is automatically added to the block list.
 */
static Actor* Level_look_up_block(Level* self, Position pos) {
  if (self->ms_state.block_list_count) {
    for (uint32_t n = 0; n < self->ms_state.block_list_count; ++n)
      if (self->ms_state.block_list[n]->pos == pos &&
          !self->ms_state.block_list[n]->hidden)
        return self->ms_state.block_list[n];
  }

  Actor* block = Level_create_actor(self);
  block->id = Block;
  block->pos = pos;
  TileID id = Level_cell_get_top_floor(self, pos);
  if (id == Block_Static)
    block->direction = DIRECTION_NIL;
  else if (TileID_actor_get_id(id) == Block)
    block->direction = TileID_actor_get_dir(id);
  else
    warn("%d: Level_look_up_block called on blockless location",
         self->current_tick);
  // _assert(!"lookupblock() called on blockless location");

  return Level_add_to_block_list(self, block);
}

/* Update the given creature's tile on the map to reflect its current
 * state.
 */
static void Actor_update_floor(Actor* self, Level* level) {
  if (self->hidden)
    return;
  MapTile* tile = MapCell_get_top_tile(Level_get_map_cell(level, self->pos));
  if (self->id == Block) {
    Level_cell_set_top_floor(level, self->pos, Block_Static);
    if (self->state & CS_MUTANT)
      MapTile_set_floor(tile, TileID_actor_with_dir(Chip, DIRECTION_NORTH));
    return;
  } else if (self->id == Chip) {
    if (level->ms_state.chip_status) {
      switch (level->ms_state.chip_status) {
        case CHIP_BURNED:
          MapTile_set_floor(tile, TileID_actor_with_dir(Chip, Burned_Chip));
          return;
        case CHIP_DROWNED:
          MapTile_set_floor(tile, TileID_actor_with_dir(Chip, Drowned_Chip));
          return;
      }
    } else if (Level_cell_get_bottom_floor(level, self->pos) == Water) {
      self->id = Swimming_Chip;
    }
  }

  if (self->state & CS_TURNING)
    self->direction = Direction_right(self->direction);

  MapTile_set_floor(tile, TileID_actor_with_dir(self->id, self->direction));
  MapTile_clear_state(tile);
}

/* Add the given creature's tile to the map.
 */
static void Actor_add_to_map(Actor* self, Level* level) {
  static MapTile const dummy = {Empty, 0};

  if (self->hidden)
    return;
  MapCell_push_tile(Level_get_map_cell(level, self->pos), dummy);
  Actor_update_floor(self, level);
}

/* Enervate an inert creature.
 */
static Actor* Level_awaken_creature(Level* self, Position pos) {
  TileID tileid = Level_cell_get_top_floor(self, pos);
  if (!TileID_is_actor(tileid) || TileID_actor_get_id(tileid) == Chip)
    return NULL;
  Actor* new = Level_create_actor(self);
  new->id = TileID_actor_get_id(tileid);
  new->direction = TileID_actor_get_dir(tileid);
  new->pos = pos;
  if (new->id == Block)
    Level_add_to_block_list(self, new);
  return new;
}

/* Mark a creature as dead.
 */
static void Actor_remove(Actor* self, Level* level) {
  self->state &= ~(CS_SLIP | CS_SLIDE);
  if (self->id == Chip) {
    if (level->ms_state.chip_status == CHIP_OKAY)
      level->ms_state.chip_status = CHIP_NOTOKAY;
  } else
    self->hidden = true;
}

/* Turn around any and all tanks. (A tank that is halfway through the
 * process of moving at the time is given special treatment.)
 */
static void Level_turn_tanks(Level* self, Actor const* invoking_actor) {
  for (uint32_t n = 0; n < self->ms_state.actor_count; ++n) {
    Actor* actor = &self->actors[n]; /* convenience, Tank Top Glitch */
    if (actor->hidden || actor->id != Tank)
      continue;
    actor->direction = Direction_back(actor->direction);
    if (actor->state & CS_SLIP && !(actor->state & CS_SLIDE) &&
        Actor_get_spare_direction(actor) != DIRECTION_NIL &&
        !(actor->state & CS_SPONTANEOUS))
      actor->direction = Direction_back(
          Actor_get_spare_direction(actor)); /* Tank Top Glitch */
    if (!(actor->state & CS_TURNING))
      actor->state |= CS_TURNING | CS_HASMOVED;
    if (actor == invoking_actor)
      continue;
    if (TileID_actor_get_id(Level_cell_get_top_floor(self, actor->pos)) ==
        Tank) {
      Actor_update_floor(actor, self);
    } else if ((actor->state & CS_SPONTANEOUS)) {
      /* handle Spontaneous Generation */
      if (actor->state & CS_TURNING) {
        /* always TRUE? */
        actor->state &= ~CS_TURNING;
        Actor_update_floor(actor, self);
        actor->state |= CS_TURNING;
      }
      actor->direction = Direction_back(
          actor->direction); /* OK with SGG, bad for stacked tanks */
    }
  }
}

/*
 * Maintaining the slip list.
 */

/* Add the given creature to the slip list if it is not already on it
 * (assuming that the given floor is a kind that causes slipping).
 */
static void Actor_start_floor_movement(Actor* self,
                                       Level* level,
                                       TileID floor,
                                       Direction fdir) {
  int dir = fdir; /* fdir used with tank reversal when stuck on teleporter */

  self->state &= ~(CS_SLIP | CS_SLIDE);

  if (TileID_is_ice(floor)) {
    if (fdir == DIRECTION_NIL) {
      /* tank reversal patch */
      dir = get_ice_wall_turn_dir(floor, self->direction);
    }
  } else if (TileID_is_slide(floor))
    dir = Level_get_slide_dir(level, floor);
  else if (floor == Teleport) {
    if (fdir == DIRECTION_NIL)
      dir = self->direction; /* tank reversal patch */
  } else if (floor == Beartrap && self->id == Block)
    dir = self->direction;
  else if (self->id != Chip) /* new with Convergence Patch */
    return;
  else
    dir = self->direction; /* new with Convergence Patch */

  if (self->id == Chip) {
    /* changed with Convergence Patch */
    /* cr->state |= isslide(floor) ? CS_SLIDE : CS_SLIP; */
    self->state |=
        (TileID_is_ice(floor) || (floor == Teleport && dir != DIRECTION_NIL))
            ? CS_SLIP
            : CS_SLIDE;
    Level_prepend_to_slip_list(level, self, dir);
    self->direction = dir;
    Actor_update_floor(self, level);
  } else {
    self->state |= CS_SLIP;
    Actor_set_spare_direction(self, DIRECTION_NIL);  // tank top glitch
    Level_append_to_slip_list(level, self, dir);
  }
}

/* Remove the given creature from the slip list.
 */
static void Actor_end_floor_movement(Actor* self, Level* level) {
  self->state &= ~(CS_SLIP | CS_SLIDE);
  Level_remove_actor_from_slip_list(level, self);
}

/* Clean out deadwood entries in the slip list.
 */
static void Level_update_sliplist(Level* self) {
  if (self->ms_state.slip_count == 0)
    return;
  for (int64_t n = self->ms_state.slip_count - 1; n >= 0; --n) {
    if (!(self->ms_state.slip_list[n].actor->state & (CS_SLIP | CS_SLIDE))) {
      Actor_end_floor_movement(self->ms_state.slip_list[n].actor, self);
    }
  }
}

/* Including the flag CMM_NOLEAVECHECK in a call to canmakemove()
 * indicates that the tile the creature is moving out of is
 * automatically presumed to permit such movement. CMM_NOEXPOSEWALLS
 * causes blue and hidden walls to remain unexposed.
 * CMM_CLONECANTBLOCK means that the creature will not be prevented
 * from moving by an identical creature standing in the way.
 * CMM_NOPUSHING prevents Chip from pushing blocks inside this
 * function. CMM_TELEPORTPUSH indicates to the block-pushing logic
 * that Chip is teleporting. This prevents a stack of two blocks from
 * being treated as a single block, and allows Chip to push a slipping
 * block away from him. CMM_NOFIRECHECK causes bugs and walkers to not
 * avoid fire. Finally, CMM_NODEFERBUTTONS causes buttons pressed by
 * pushed blocks to take effect immediately.
 */

/* Move a block at the given position forward in the given direction.
 * FALSE is returned if the block cannot be pushed.
 */
static bool Level_push_block(Level* self,
                             Position pos,
                             Direction dir,
                             enum CollisionCheckFlags flags) {
  /* new */
  // _assert(cellat(pos)->top.id == Block_Static);
  // _assert(dir != NIL);

  Actor* cr = Level_look_up_block(self, pos);
  if (!cr) {
    warn("%d: attempt to push disembodied block!", self->current_tick);
    return false;
  }
  bool slipping = (cr->state & (CS_SLIP | CS_SLIDE)); /* accounting */
  if (cr->state & (CS_SLIP | CS_SLIDE)) {
    Direction slipdir = Level_get_actor_slip_dir(self, cr);
    if (dir == slipdir || dir == Direction_back(slipdir)) {
      if (!(flags & CMM_TELEPORTPUSH)) {
        return false;
      }
    }
  }

  if (!(flags & CMM_TELEPORTPUSH) &&
      Level_cell_get_bottom_floor(self, pos) == Block_Static)
    Level_cell_set_bottom_floor(self, pos, Empty);
  if (!(flags & CMM_NODEFERBUTTONS))
    cr->state |= CS_DEFERPUSH;
  bool r = Actor_advance_movement(cr, self, dir);
  if (!(flags & CMM_NODEFERBUTTONS))
    cr->state &= ~CS_DEFERPUSH;
  if (!r) {
    cr->state &= ~(CS_SLIP | CS_SLIDE);
    if (slipping) {
      /* new MSCC-like accounting */
      self->ms_state.mscc_slippers--;
      Level_remove_actor_from_slip_list(self, cr);
    }
  }
  return r;
}

static bool TileID_impedes_move_into(TileID self,
                                     Actor const* actor,
                                     Direction dir) {
  switch (self) {
    case Nothing:
    case Wall:
    case HiddenWall_Perm:
    case SwitchWall_Closed:
    case CloneMachine:
    case Drowned_Chip:
    case Burned_Chip:
    case Bombed_Chip:
    case Exited_Chip:
    case Exit_Extra_1:
    case Exit_Extra_2:
    case Overlay_Buffer:
    case Floor_Reserved1:
    case Floor_Reserved2:
    case Water_Splash:
    case Bomb_Explosion:
    case Entity_Explosion:
      return true;

    case Empty:
    case Slide_North:
    case Slide_West:
    case Slide_South:
    case Slide_East:
    case Slide_Random:
    case Ice:
    case Water:
    case Fire:
    case Bomb:
    case Beartrap:
    case HintButton:
    case Button_Blue:
    case Button_Green:
    case Button_Red:
    case Button_Brown:
    case Teleport:
    case SwitchWall_Open:
    case Key_Red:
    case Key_Blue:
    case Key_Yellow:
    case Key_Green:
      return false;

    case Gravel:
    case Exit:
    case Boots_Ice:
    case Boots_Slide:
    case Boots_Fire:
    case Boots_Water:
      return actor->id != Chip && actor->id != Block;
    case Dirt:
    case Burglar:
    case HiddenWall_Temp:
    case BlueWall_Real:
    case BlueWall_Fake:
    case PopupWall:
    case Door_Red:
    case Door_Blue:
    case Door_Yellow:
    case Door_Green:
    case Socket:
    case ICChip:
    case Block_Static:
      return actor->id != Chip;

    case IceWall_Northwest:  // dir != instead of just dir == because rarely a
                             // NIL can get passed here as a result of tank top
      return dir != DIRECTION_NORTH && dir != DIRECTION_WEST;
    case IceWall_Northeast:
      return dir != DIRECTION_NORTH && dir != DIRECTION_EAST;
    case IceWall_Southwest:
      return dir != DIRECTION_SOUTH && dir != DIRECTION_WEST;
    case IceWall_Southeast:
    case Wall_Southeast:
      return dir != DIRECTION_SOUTH && dir != DIRECTION_EAST;
    case Wall_North:
      return dir != DIRECTION_NORTH && dir != DIRECTION_EAST &&
             dir != DIRECTION_WEST;
    case Wall_East:
      return dir != DIRECTION_NORTH && dir != DIRECTION_SOUTH &&
             dir != DIRECTION_WEST;
    case Wall_South:
      return dir != DIRECTION_SOUTH && dir != DIRECTION_EAST &&
             dir != DIRECTION_WEST;
    case Wall_West:
      return dir != DIRECTION_NORTH && dir != DIRECTION_SOUTH &&
             dir != DIRECTION_WEST;

    default:
      return false;
  }
}

/* Return TRUE if the given creature is allowed to attempt to move in
 * the given direction. Side effects can and will occur from calling
 * this function, as indicated by flags.
 */
static bool Actor_can_make_move(Actor* self,
                                Level* level,
                                Direction dir,
                                enum CollisionCheckFlags flags) {
  TileID floor;
  TileID id;

  if (dir == DIRECTION_NIL) {
    warn("%d: Actor_can_make_move called with DIRECTION_NIL",
         level->current_tick);
  }
  if (!self) {
    warn("%d: Actor_can_make_move called with NULL actor", level->current_tick);
  }

  Position y = self->pos / MAP_WIDTH;
  Position x = self->pos % MAP_WIDTH;
  y += dir == DIRECTION_NORTH ? -1 : dir == DIRECTION_SOUTH ? +1 : 0;
  x += dir == DIRECTION_WEST ? -1 : dir == DIRECTION_EAST ? +1 : 0;
  if (y < 0 || y >= MAP_HEIGHT || x < 0 || x >= MAP_WIDTH)
    return false;
  Position to = y * MAP_WIDTH + x;

  if (!(flags & CMM_NOLEAVECHECK)) {
    switch (Level_cell_get_bottom_floor(level, self->pos)) {
      case Wall_North:
        if (dir == DIRECTION_NORTH)
          return false;
        break;
      case Wall_West:
        if (dir == DIRECTION_WEST)
          return false;
        break;
      case Wall_South:
        if (dir == DIRECTION_SOUTH)
          return false;
        break;
      case Wall_East:
        if (dir == DIRECTION_EAST)
          return false;
        break;
      case Wall_Southeast:
        if (dir & (DIRECTION_SOUTH | DIRECTION_EAST))
          return false;
        break;
      case Beartrap:
        if (!(self->state & CS_RELEASED))
          return false;
        break;
      default:
        break;
    }
  }

  if (self->id == Chip) {
    floor = Level_cell_get_terrain(level, to);
    if (TileID_impedes_move_into(floor, self, dir))
      return false;
    if (floor == Socket && level->chips_left > 0)
      return false;
    if (TileID_is_door(floor) && !Level_player_has_item(level, floor))
      return false;
    if (TileID_is_actor(Level_cell_get_top_floor(level, to))) {
      id = TileID_actor_get_id(Level_cell_get_top_floor(level, to));
      if (id == Chip || id == Swimming_Chip || id == Block)
        return false;
    }
    if (floor == HiddenWall_Temp || floor == BlueWall_Real) {
      if (!(flags & CMM_NOEXPOSEWALLS))
        Level_cell_set_terrain(level, to, Wall);
      return false;
    }
    if (floor == Block_Static) {
      if (!Level_push_block(level, to, dir, flags))
        return false;
      else if (flags & CMM_NOPUSHING)
        return false;
      if (Level_cell_get_bottom_floor(level, to) == CloneMachine)
        return false; /* totally backwards: need to check this first */
      if ((flags & CMM_TELEPORTPUSH) &&
          Level_cell_get_terrain(level, to) == Block_Static)
        /* totally backwards: remove "&& cellat(to)->bot.id == Empty)" */
        return true;
      return Actor_can_make_move(self, level, dir, flags | CMM_NOPUSHING);
    }
  } else if (self->id == Block) {
    floor = Level_cell_get_top_floor(level, to);
    if (TileID_is_actor(floor)) {
      id = TileID_actor_get_id(floor);
      return id == Chip || id == Swimming_Chip;
    }
    if (TileID_impedes_move_into(floor, self, dir))
      return false;
  } else {
    floor = Level_cell_get_top_floor(level, to);
    if (TileID_is_actor(floor)) {
      id = TileID_actor_get_id(floor);
      if (id == Chip || id == Swimming_Chip) {
        floor = Level_cell_get_bottom_floor(level, to);
        if (TileID_is_actor(floor)) {
          id = TileID_actor_get_id(floor);
          return id == Chip || id == Swimming_Chip;
        }
      }
    }
    if (TileID_is_actor(floor)) {
      /* turning tank cloning patch */
      Actor* F = Level_look_up_creature(level, to, false);
      if (!(flags & CMM_CLONECANTBLOCK)) /* not cloning */
        return false;
      if ((F == NULL || !(F->state & CS_TURNING)) &&
          floor == TileID_actor_with_dir(self->id, self->direction))
        return true;
      /* must check "floor", so same-dir non-creature tank will clone */
      if (F == NULL)
        return false;
      if (F->direction == self->direction)
        return true;
      return false;
    }
    if (TileID_impedes_move_into(floor, self, dir))
      return false;
    if (floor == Fire && (self->id == Bug || self->id == Walker))
      if (!(flags & CMM_NOFIRECHECK))
        return false;
  }

  if (Level_cell_get_bottom_floor(level, to) == CloneMachine)
    return false;

  return true;
}

/*
 * How everyone selects their move.
 */

/* This function embodies the movement behavior of all the creatures.
 * Given a creature, this function enumerates its desired direction of
 * movement and selects the first one that is permitted. Note that
 * calling this function also updates the current controller
 * direction.
 */
static void Actor_choose_move_creature(Actor* self, Level* level) {
  Direction choices[4] = {DIRECTION_NIL, DIRECTION_NIL, DIRECTION_NIL,
                          DIRECTION_NIL};
  Direction dir;

  self->move_decision = DIRECTION_NIL;

  if (self->hidden)
    return;
  if (self->id == Block)
    return;
  if (level->current_tick & 2)
    return;
  if (self->id == Teeth || self->id == Blob) {
    if ((level->current_tick + level->init_step_parity) & 4)
      return;
  }
  if (self->state & CS_TURNING) {
    self->state &= ~(CS_TURNING | CS_HASMOVED);
    Actor_update_floor(self, level);
  }
  if (self->state & CS_HASMOVED) {
    /* should be a stalled tank */
    TileID floor =
        Level_cell_get_top_floor(level, self->pos); /* stacked tank patch */
    TileID id = TileID_actor_get_id(floor);
    if (TileID_is_actor(floor) && (id == Chip || id == Swimming_Chip))
      floor = Level_cell_get_bottom_floor(level, self->pos);
    if (!TileID_is_actor(floor) &&
        !TileID_impedes_move_into(floor, self, DIRECTION_NIL))
      self->hidden = true; /* hack with (0,0) movement success */
    /* maybe should check if (0,0) move goes on sliplist, but that's UB */
  }
  if (self->state & CS_HASMOVED) {
    level->ms_state.controller_dir = DIRECTION_NIL;
    return;
  }
  if (self->state & (CS_SLIP | CS_SLIDE))
    return;

  TileID floor = Level_cell_get_terrain(level, self->pos);

  Direction pdir = dir = self->direction;

  if (floor == CloneMachine || floor == Beartrap) {
    switch (self->id) {
      case Tank:
      case Ball:
      case Glider:
      case Fireball:
      case Walker:
        choices[0] = dir;
        break;
      case Blob:
        choices[0] = dir;
        choices[1] = Direction_left(dir);
        choices[2] = Direction_back(dir);
        choices[3] = Direction_right(dir);
        Prng_permute4(&level->prng, choices, sizeof(Direction));
        break;
      case Bug:
      case Paramecium:
      case Teeth:
        choices[0] = level->ms_state.controller_dir;
        self->move_decision = level->ms_state.controller_dir;
        return;
      default:
        warn("%d: Non-creature %02X at (%d, %d) trying to move",
             level->current_tick, self->pos % MAP_WIDTH, self->pos / MAP_WIDTH,
             self->id);
        break;
    }
  } else {
    Position y, x;
    Direction m, n;
    switch (self->id) {
      case Tank:
        choices[0] = dir;
        break;
      case Ball:
        choices[0] = dir;
        choices[1] = Direction_back(dir);
        break;
      case Glider:
        choices[0] = dir;
        choices[1] = Direction_left(dir);
        choices[2] = Direction_right(dir);
        choices[3] = Direction_back(dir);
        break;
      case Fireball:
        choices[0] = dir;
        choices[1] = Direction_right(dir);
        choices[2] = Direction_left(dir);
        choices[3] = Direction_back(dir);
        break;
      case Walker:
        choices[0] = dir;
        choices[1] = Direction_left(dir);
        choices[2] = Direction_back(dir);
        choices[3] = Direction_right(dir);
        Prng_permute3(&level->prng, choices + 1, sizeof(Direction));
        break;
      case Blob:
        choices[0] = dir;
        choices[1] = Direction_left(dir);
        choices[2] = Direction_back(dir);
        choices[3] = Direction_right(dir);
        Prng_permute4(&level->prng, choices, sizeof(Direction));
        break;
      case Bug:
        choices[0] = Direction_left(dir);
        choices[1] = dir;
        choices[2] = Direction_right(dir);
        choices[3] = Direction_back(dir);
        break;
      case Paramecium:
        choices[0] = Direction_right(dir);
        choices[1] = dir;
        choices[2] = Direction_left(dir);
        choices[3] = Direction_back(dir);
        break;
      case Teeth:
        y = Level_get_chip(level)->pos / MAP_WIDTH - self->pos / MAP_WIDTH;
        x = Level_get_chip(level)->pos % MAP_WIDTH - self->pos % MAP_WIDTH;
        n = y < 0 ? DIRECTION_NORTH : y > 0 ? DIRECTION_SOUTH : DIRECTION_NIL;
        if (y < 0)
          y = -y;
        m = x < 0 ? DIRECTION_WEST : x > 0 ? DIRECTION_EAST : DIRECTION_NIL;
        if (x < 0)
          x = -x;
        if (x > y) {
          choices[0] = m;
          choices[1] = n;
        } else {
          choices[0] = n;
          choices[1] = m;
        }
        pdir = choices[2] = choices[0];
        break;
      default:
        warn("%d: Non-creature %02X at (%d, %d) trying to move",
             level->current_tick, self->pos % MAP_WIDTH, self->pos / MAP_WIDTH,
             self->id);
        // _assert(!"Unknown creature trying to move");
        break;
    }
  }

  for (int n = 0; n < 4 && choices[n] != DIRECTION_NIL; ++n) {
    self->move_decision = choices[n];
    level->ms_state.controller_dir = self->move_decision;
    if (Actor_can_make_move(self, level, choices[n], 0))
      return;
  }

  if (self->id == Tank) {
    if ((self->state & CS_RELEASED) ||
        (floor !=
         Beartrap /*&& floor != CloneMachine*/)) /* (c) bug: tank clones should
                                                    stall */
      self->state |= CS_HASMOVED;
    self->move_decision = DIRECTION_NIL; /* handle stacked tanks */
  }

  if (self->id != Tank) /* handle stacked tanks */
    self->move_decision = pdir;
}

/* Select a direction for Chip to move towards the goal position.
 */
static Direction Level_get_chip_mouse_direction(Level* level) {
  if (!Level_has_mouse_goal(level))
    return DIRECTION_NIL;
  Actor* chip = Level_get_chip(level);
  if (Level_get_mouse_goal(level) == chip->pos) {
    Level_cancel_mouse_goal(level);
    return DIRECTION_NIL;
  }

  Position y = Level_get_mouse_goal(level) / MAP_WIDTH - chip->pos / MAP_WIDTH;
  Position x = Level_get_mouse_goal(level) % MAP_WIDTH - chip->pos % MAP_WIDTH;
  Direction dir;
  Direction d1 = y < 0   ? DIRECTION_NORTH
                 : y > 0 ? DIRECTION_SOUTH
                         : DIRECTION_NIL;
  if (y < 0)
    y = -y;
  Direction d2 = x < 0   ? DIRECTION_WEST
                 : x > 0 ? DIRECTION_EAST
                         : DIRECTION_NIL;
  if (x < 0)
    x = -x;
  if (x > y) {
    dir = d1;
    d1 = d2;
    d2 = dir;
  }
  if (d1 != DIRECTION_NIL && d2 != DIRECTION_NIL)
    dir = Actor_can_make_move(chip, level, d1, 0) ? d1 : d2;
  else
    dir = d2 == DIRECTION_NIL ? d1 : d2;

  return dir;
}

/* Unpack a Chip-relative map location.
 */
static Position Level_chip_rel_position_to_absolute(Actor const* chip,
                                                    Position relpos) {
  Position x = relpos % MOUSERANGE + MOUSERANGEMIN;
  Position y = relpos / MOUSERANGE + MOUSERANGEMIN;
  return chip->pos + y * MAP_WIDTH + x;
}

/* Determine the direction of Chip's next move. If discard is TRUE,
 * then Chip is not currently permitted to select a direction of
 * movement, and the player's input should not be retained.
 */
static void Actor_choose_move_chip(Actor* chip, Level* level, bool discard) {
  chip->move_decision = DIRECTION_NIL;

  if (chip->hidden)
    return;

  if (!(level->current_tick & 3))
    chip->state &= ~CS_HASMOVED;
  if (chip->state & CS_HASMOVED) {
    if (level->game_input != DIRECTION_NIL && Level_has_mouse_goal(level)) {
      Level_cancel_mouse_goal(level);
    }
    return;
  }

  GameInput input = level->game_input;
  // level->game_input = DIRECTION_NIL; //todo: is this valid anymore???,
  // commenting out because i don't think so
  if (discard || ((chip->state & CS_SLIDE) && input == chip->direction)) {
    if (level->current_tick && !(level->current_tick & 1))
      Level_cancel_mouse_goal(level);
    return;
  }

  if (input >= GAME_INPUT_ABS_MOUSE_MOVE_FIRST &&
      input <= GAME_INPUT_ABS_MOUSE_MOVE_LAST) {
    level->ms_state.mouse_goal = input - GAME_INPUT_ABS_MOUSE_MOVE_FIRST;
    input = DIRECTION_NIL;
  } else if (input >= GAME_INPUT_MOUSE_MOVE_FIRST &&
             input <= GAME_INPUT_MOUSE_MOVE_LAST) {
    level->ms_state.mouse_goal = Level_chip_rel_position_to_absolute(
        chip, input - GAME_INPUT_MOUSE_MOVE_FIRST);
    input = DIRECTION_NIL;
  } else {
    if ((input & (DIRECTION_NORTH | DIRECTION_SOUTH)) &&
        (input & (DIRECTION_EAST | DIRECTION_WEST)))
      input &= DIRECTION_NORTH | DIRECTION_SOUTH;
  }

  if (input == DIRECTION_NIL && Level_cancel_mouse_goal(level) &&
      (level->current_tick & 3) == 2)
    input = Level_get_chip_mouse_direction(level);

  chip->move_decision = input;
}

/* Teleport the given creature instantaneously from the teleport tile
 * at start to another teleport tile (if possible).
 */
static Position Actor_teleport(Actor* self, Level* level, Position start) {
  Direction origdir =
      self->direction; /* tank push IB onto blue button via teleporter */
  if (self->direction == DIRECTION_NIL) {
    warn("%d: directionless creature %02X on teleport at (%d %d)",
         level->current_tick, self->id, self->pos % MAP_WIDTH,
         self->pos / MAP_WIDTH);
  } else if (self->hidden) {
    warn("%d: hidden creature %02X on teleport at (%d %d)", level->current_tick,
         self->id, self->pos % MAP_WIDTH, self->pos / MAP_WIDTH);
  }

  Position origpos = self->pos;
  Position dest = start;

  for (;;) {
    --dest;
    if (dest < 0)
      dest += MAP_WIDTH * MAP_HEIGHT;
    if (dest == start)
      break;
    MapTile* tile = MapCell_get_top_tile(Level_get_map_cell(level, dest));
    if (MapTile_get_floor(tile) != Teleport ||
        (MapTile_get_state(tile) & FS_BROKEN))
      continue;
    self->pos = dest;
    bool f = Actor_can_make_move(self, level, self->direction,
                                 CMM_NOLEAVECHECK | CMM_NOEXPOSEWALLS |
                                     CMM_NODEFERBUTTONS | CMM_NOFIRECHECK |
                                     CMM_TELEPORTPUSH);
    self->direction =
        origdir; /* tank push IB onto blue button via teleporter */
    self->pos = origpos;
    if (f)
      break;
  }

  return dest;
}

/* Determine the move(s) a creature will make on the current tick.
 */
static void Actor_choose_move(Actor* self, Level* level) {
  if (self->id == Chip) {
    Actor_choose_move_chip(self, level, self->state & CS_SLIP);
  } else {
    if (self->state & CS_SLIP)
      self->move_decision = DIRECTION_NIL;
    else
      Actor_choose_move_creature(self, level);
  }
}

/* Initiate the cloning of a creature.
 */
static void Level_activate_cloner(Level* self, Position button_pos) {
  Actor dummy;
  Actor* actor;

  Position pos = Level_locate_cloner_by_button(self, button_pos);
  if (pos < 0 || pos >= MAP_WIDTH * MAP_HEIGHT)
    return;
  TileID tileid = Level_cell_get_top_floor(self, pos);
  if (!TileID_is_actor(tileid) || TileID_actor_get_id(tileid) == Chip)
    return;
  if (TileID_actor_get_dir(tileid) == Block) {
    actor = Level_look_up_block(self, pos);
    if (actor->direction != DIRECTION_NIL)
      Actor_advance_movement(actor, self, actor->direction);
  } else {
    if (MapTile_get_state(
            MapCell_get_bottom_tile(Level_get_map_cell(self, pos))) &
        FS_CLONING)
      return;
    memset(&dummy, 0, sizeof(dummy));
    dummy.id = TileID_actor_get_id(tileid);
    dummy.direction = TileID_actor_get_dir(tileid);
    dummy.pos = pos;
    if (!Actor_can_make_move(&dummy, self, dummy.direction, CMM_CLONECANTBLOCK))
      return;
    actor = Level_awaken_creature(self, pos);
    if (!actor)
      return;
    actor->state |= CS_CLONING;
    if (Level_cell_get_bottom_floor(self, pos) == CloneMachine)
      MapTile_add_cloning_state(
          MapCell_get_bottom_tile(Level_get_map_cell(self, pos)));
  }
}

/* Open a bear trap. Any creature already in the trap is released.
 */
static void Level_spring_trap(Level* self, Position buttonpos) {
  Actor* actor;

  Position pos = Level_locate_trap_by_button(self, buttonpos);
  if (pos < 0)
    return;
  if (pos >= MAP_WIDTH * MAP_HEIGHT) {
    warn("%d: Off-map trap opening attempted: (%d %d)", self->current_tick,
         pos % MAP_WIDTH, pos / MAP_WIDTH);
    return;
  }
  TileID id = Level_cell_get_top_floor(self, pos);
  if (id == Block_Static || (MapTile_get_state(MapCell_get_bottom_tile(
                                 Level_get_map_cell(self, pos))) &
                             FS_HASMUTANT)) {
    actor = Level_look_up_block(self, pos);
    if (actor)
      actor->state |= CS_RELEASED;
  } else if (TileID_is_actor(id)) {
    actor = Level_look_up_creature(self, pos, true);
    if (actor)
      actor->state |= CS_RELEASED;
  }
}

/* Mark all buttons everywhere as having been handled.
 */
static void Level_reset_buttons(Level* self) {
  for (uint32_t pos = 0; pos < MAP_WIDTH * MAP_HEIGHT; ++pos) {
    MapCell* cell = Level_get_map_cell(self, pos);
    MapTile_remove_button_down_state(&cell->top);
    MapTile_remove_button_down_state(&cell->bottom);
  }
}

/* Apply the effects of all deferred button presses, if any.
 */
static void Level_handle_buttons(Level* self) {
  TileID id;

  for (Position pos = 0; pos < MAP_WIDTH * MAP_HEIGHT; ++pos) {
    MapCell* cell = Level_get_map_cell(self, pos);
    MapTile* top_tile = MapCell_get_top_tile(cell);
    MapTile* bottom_tile = MapCell_get_bottom_tile(cell);
    if (MapTile_get_state(top_tile) & FS_BUTTONDOWN) {
      MapTile_remove_button_down_state(top_tile);
      id = MapTile_get_floor(top_tile);
    } else if (MapTile_get_state(bottom_tile) & FS_BUTTONDOWN) {
      MapTile_remove_button_down_state(bottom_tile);
      id = MapTile_get_floor(bottom_tile);
    } else {
      continue;
    }
    switch (id) {
      case Button_Blue:
        Level_add_sfx(self, SND_BUTTON_PUSHED);
        Level_turn_tanks(self, NULL);
        break;
      case Button_Green:
        Level_toggle_walls(self);
        break;
      case Button_Red:
        Level_activate_cloner(self, pos);
        Level_add_sfx(self, SND_BUTTON_PUSHED);
        break;
      case Button_Brown:
        Level_spring_trap(self, pos);
        Level_add_sfx(self, SND_BUTTON_PUSHED);
        break;
      default:
        warn("%d: Fooey! Tile %02X is not a button!", self->current_tick, id);
        break;
    }
  }
}

/*
 * When something actually moves.
 */

/* Initiate a move by the given creature in the given direction.
 * Return FALSE if the creature cannot initiate the indicated move
 * (side effects may still occur).
 */
static bool Actor_start_movement(Actor* self, Level* level, Direction dir) {
  TileID floor = Level_cell_get_bottom_floor(level, self->pos);
  Direction odir = self->direction; /* b2 fix with convergence glitch */

  if (dir == DIRECTION_NIL) {
    warn("%d: Actor_start_movement called with DIRECTION_NIL",
         level->current_tick);
  }

  if (!Actor_can_make_move(self, level, dir, 0)) {
    if (self->id == Chip || (floor != Beartrap && floor != CloneMachine &&
                             !(self->state & CS_SLIP))) {
      if (self->id != Chip || odir != DIRECTION_NIL)
        self->direction = dir; /* b2 fix */
      Actor_update_floor(self, level);
    }
    return false;
  }

  if (floor == Beartrap) {
    if (!(self->state & CS_RELEASED)) {
      warn("%d: Actor_start_movement from a beartrap without CS_RELEASED set",
           level->current_tick);
    }
    if (self->state & CS_MUTANT) {
      MapTile_add_mutant_state(&Level_get_map_cell(level, self->pos)->bottom);
    }
  }
  self->state &= ~CS_RELEASED;

  self->direction = dir;

  return true;
}

/* Complete the movement of the given creature. Most side effects
 * produced by moving onto a tile occur at this point. This function
 * is also the only place where a creature can be added to the slip
 * list.
 */
static void Actor_end_movement(Actor* self, Level* level, Direction dir) {
  bool dead = false;
  int64_t i;
  bool blockcloning = false; /* Squish patch */

  Position oldpos = self->pos;
  Position newpos = Position_neighbor(self->pos, dir);

  MapCell* cell = Level_get_map_cell(level, newpos);
  MapTile* tile = MapCell_get_top_tile(cell);
  TileID floor = MapTile_get_floor(tile);
  TileID actor_id_top = TileID_actor_get_id(
      Level_cell_get_top_floor(level, oldpos)); /* Non-existence patch */
  TileID floor_bottom = MapTile_get_floor(tile);
  if (self->id == Chip) {
    switch (floor) {
      case Empty:
        MapCell_pop_tile(cell);
        break;
      case Water:
        if (!Level_player_has_item(level, floor))
          level->ms_state.chip_status = CHIP_DROWNED;
        break;
      case Fire:
        if (!Level_player_has_item(level, floor))
          level->ms_state.chip_status = CHIP_BURNED;
        break;
      case Dirt:
        MapCell_pop_tile(cell);
        break;
      case BlueWall_Fake:
        MapCell_pop_tile(cell);
        break;
      case PopupWall:
        tile->id = Wall;
        break;
      case Door_Red:
      case Door_Blue:
      case Door_Yellow:
      case Door_Green:
        if (!Level_player_has_item(level, floor)) {
          warn("%d: Player entered door %0X without key!", level->current_tick,
               floor);
        }
        if (floor != Door_Green) {
          --*Level_player_item_ptr(level, floor);
        }
        MapCell_pop_tile(cell);
        Level_add_sfx(level, SND_DOOR_OPENED);
        break;
      case Boots_Ice:
      case Boots_Slide:
      case Boots_Fire:
      case Boots_Water:
      case Key_Red:
      case Key_Blue:
      case Key_Yellow:
      case Key_Green:
        if (TileID_is_actor(floor_bottom))
          level->ms_state.chip_status = CHIP_COLLIDED;
        *Level_player_item_ptr(level, floor) += 1;
        MapCell_pop_tile(cell);
        Level_add_sfx(level, SND_ITEM_COLLECTED);
        break;
      case Burglar:
        *Level_player_item_ptr(level, Boots_Ice) = 0;
        *Level_player_item_ptr(level, Boots_Slide) = 0;
        *Level_player_item_ptr(level, Boots_Fire) = 0;
        *Level_player_item_ptr(level, Boots_Water) = 0;
        Level_add_sfx(level, SND_BOOTS_STOLEN);
        break;
      case ICChip:
        if (level->chips_left)
          --level->chips_left;
        MapCell_pop_tile(cell);
        Level_add_sfx(level, SND_IC_COLLECTED);
        break;
      case Socket:
        if (level->chips_left)
          warn("%d: Entered socket with IC Chips still remaining",
               level->current_tick);
        MapCell_pop_tile(cell);
        Level_add_sfx(level, SND_SOCKET_OPENED);
        break;
      case Bomb:
        level->ms_state.chip_status = CHIP_BOMBED;
        Level_add_sfx(level, SND_BOMB_EXPLODES);
        break;
      default:
        if (TileID_is_actor(floor))
          level->ms_state.chip_status = CHIP_COLLIDED;
        break;
    }
  } else if (self->id == Block) {
    switch (floor) {
      case Empty:
        MapCell_pop_tile(cell);
        break;
      case Water:
        MapTile_set_floor(tile, Dirt);
        dead = true;
        Level_add_sfx(level, SND_WATER_SPLASH);
        break;
      case Bomb:
        MapTile_set_floor(tile, Empty);
        dead = true;
        Level_add_sfx(level, SND_BOMB_EXPLODES);
        break;
      case Teleport:
        if (!(MapTile_get_state(tile) & FS_BROKEN))
          newpos = Actor_teleport(self, level, newpos);
        break;
      default:
        break;
    }
    TileID id = Level_cell_get_top_floor(level, oldpos);
    if (TileID_is_actor(id) && TileID_actor_get_id(id) == Chip) {
      self->state |= CS_MUTANT;
    }
  } else {
    if (TileID_is_actor(floor)) {
      tile = MapCell_get_bottom_tile(cell);
      MapTile_get_floor(tile);
    }
    switch (floor) {
      case Water:
        if (actor_id_top !=
            Glider) /* use actor_id_top with Non-existence patch */
          dead = true;
        break;
      case Fire:
        if (actor_id_top !=
            Fireball) /* use actor_id_top with Non-existence patch */
          dead = true;
        break;
      case Bomb:
        MapTile_set_floor(tile, Empty);
        dead = true;
        Level_add_sfx(level, SND_BOMB_EXPLODES);
        break;
      case Teleport:
        if (!(MapTile_get_state(tile) & FS_BROKEN))
          newpos = Actor_teleport(self, level, newpos);
        break;
      default:
        break;
    }
  }

  MapCell* old_cell = Level_get_map_cell(level, oldpos);
  if (MapCell_get_bottom_floor(old_cell) != CloneMachine || self->id == Chip)
    MapCell_pop_tile(old_cell);
  if (dead) {
    Actor_remove(self, level);
    if (MapCell_get_bottom_floor(old_cell) == CloneMachine)
      MapTile_remove_cloning_state(MapCell_get_bottom_tile(old_cell));
    return;
  }

  if (self->id == Chip && floor == Teleport && !(tile->state & FS_BROKEN)) {
    i = newpos;
    newpos = Actor_teleport(self, level, newpos);
    if (true || newpos != i) {
      /* Convergence Patch */
      /* no idea, but Icysanity lvl 1 requires newpos=i to work */
      Level_add_sfx(level, SND_TELEPORTING);
      if (Level_cell_get_terrain(level, newpos) == Block_Static) {
        if (level->ms_state.chip_last_slip_dir == DIRECTION_NIL) {
          /* // these seem cosmetic/superfluous with new patch
          cr->dir = NORTH;
          cellat(newpos)->top.id = crtile(Chip, NORTH);
          */
          self->direction = DIRECTION_NIL; /* Convergence Patch */
          /* floor = Empty; */
          /* Removed with Convergence Patch, cf Chip on sliplist */
        } else {
          /* seems ok still, with new Convergence logic */
          self->direction = level->ms_state.chip_last_slip_dir;
        }
      }
    }
  }

  self->pos = newpos;
  Actor_add_to_map(self, level);
  self->pos = oldpos;

  tile = MapCell_get_bottom_tile(cell);
  switch (floor) {
    case Button_Blue:
      if (self->state & CS_DEFERPUSH)
        MapTile_add_button_down_state(tile);
      else
        Level_turn_tanks(level, self);
      Level_add_sfx(level, SND_BUTTON_PUSHED);
      break;
    case Button_Green:
      if (self->state & CS_DEFERPUSH)
        MapTile_add_button_down_state(tile);
      else
        Level_toggle_walls(level);
      break;
    case Button_Red:
      self->state |= CS_SPONTANEOUS;
      if (self->state & CS_DEFERPUSH)
        MapTile_add_button_down_state(tile);
      else
        Level_activate_cloner(level, newpos);
      Level_add_sfx(level, SND_BUTTON_PUSHED);
      self->state &= ~CS_SPONTANEOUS; /* Hack with SGG */
      break;
    case Button_Brown:
      if (self->state & CS_DEFERPUSH)
        MapTile_add_button_down_state(tile);
      else
        Level_spring_trap(level, newpos);
      Level_add_sfx(level, SND_BUTTON_PUSHED);
      break;
    default:
      break;
  }
  self->pos = newpos;

  if (MapCell_get_bottom_floor(old_cell) == CloneMachine && self->id == Block &&
      MapCell_get_top_floor(old_cell) != Block_Static)
    blockcloning = true; /* Squish patch */

  if (MapCell_get_bottom_floor(old_cell) == CloneMachine)
    MapTile_add_cloning_state(MapCell_get_bottom_tile(old_cell));

  if (floor == Beartrap) {
    if (Level_is_trap_open(level, newpos, oldpos))
      self->state |= CS_RELEASED;
  } else if (Level_cell_get_top_floor(level, newpos) == Beartrap) {
    for (i = 0; i < level->trap_connections.length; ++i) {
      if (level->trap_connections.items[i].to == newpos) {
        self->state |= CS_RELEASED;
        break;
      }
    }
  }

  if (self->id == Chip) {
    if (Level_get_mouse_goal(level) == self->pos)
      Level_cancel_mouse_goal(level);
    if (level->ms_state.chip_status != CHIP_OKAY &&
        level->ms_state.chip_status != CHIP_SQUISHED)
      return; /* CHIP_SQUISHED added with Squish patch */
    if (MapCell_get_bottom_floor(cell) == Exit) {
      level->level_complete = true;
      return;
    }
  } else {
    if (TileID_is_actor(MapCell_get_bottom_floor(cell))) {
      TileID id = MapCell_get_bottom_floor(cell);
      if (TileID_actor_get_id(id) == Chip ||
          TileID_actor_get_id(id) == Swimming_Chip) {
        if (self->id != Block || !blockcloning) /* Squish patch */
          level->ms_state.chip_status = CHIP_COLLIDED;
        else
          level->ms_state.chip_status = CHIP_SQUISHED; /* Squish patch */
        return;
      }
    }
  }

  bool was_slipping = self->state & (CS_SLIP | CS_SLIDE);

  if (floor == Teleport)
    Actor_start_floor_movement(self, level, floor,
                               DIRECTION_NIL); /* NIL for tank reversal patch */
  else if (TileID_is_ice(floor) &&
           (self->id != Chip || !Level_player_has_item(level, Boots_Ice)))
    Actor_start_floor_movement(self, level, floor,
                               DIRECTION_NIL); /* NIL for tank reversal patch */
  else if (TileID_is_slide(floor) &&
           (self->id != Chip || !Level_player_has_item(level, Boots_Slide)))
    Actor_start_floor_movement(self, level, floor,
                               DIRECTION_NIL); /* NIL for tank reversal patch */
  else if (floor == Beartrap && self->id == Block && was_slipping) {
    Actor_start_floor_movement(self, level, floor,
                               DIRECTION_NIL); /* NIL for tank reversal patch */
    if (self->state & CS_MUTANT) {
      MapTile_add_mutant_state(MapCell_get_bottom_tile(cell));
    }
  } else {
    /* changes for MSCC-style sliplist */
    self->state &= ~(CS_SLIP | CS_SLIDE);
    if (was_slipping && self->id != Chip) {
      level->ms_state.mscc_slippers--;
      Level_remove_actor_from_slip_list(level, self);
    }
  }
  if (!was_slipping && (self->state & (CS_SLIP | CS_SLIDE)) && self->id != Chip)
    level->ms_state.controller_dir = Level_get_actor_slip_dir(level, self);
}

/* Move the given creature in the given direction.
 */
static bool Actor_advance_movement(Actor* self, Level* level, Direction dir) {
  if (dir == DIRECTION_NIL)
    return true;

  if (self->id == Chip)
    level->ms_state.chip_ticks_since_moved = 0;

  if (!Actor_start_movement(self, level, dir)) {
    if (self->id == Chip) {
      Level_add_sfx(level, SND_CANT_MOVE);
      Level_reset_buttons(level);
      Level_cancel_mouse_goal(level);
    }
    return false;
  }

  Actor_end_movement(self, level, dir);
  if (self->id == Chip)
    Level_handle_buttons(level);

  return true;
}

/*
 * Automatic activities.
 */

/* Execute all forced moves for creatures on the slip list. (Note the
 * use of the savedcount variable, which is how slide delay is
 * implemented.)
 */
static void Level_chip_floor_movements(Level* self) /* split into two */
{
  for (uint32_t n = 0; n < self->ms_state.slip_count; ++n) {
    MsSlipper* slipper = &self->ms_state.slip_list[n];
    if (!(slipper->actor->state & (CS_SLIP | CS_SLIDE)))
      continue;
    Direction slipdir = slipper->direction;
    if (slipdir == DIRECTION_NIL &&
        slipper->actor->id == Chip) /* Convergence Patch */
      Level_cell_set_top_floor(self, slipper->actor->pos,
                               TileID_actor_with_dir(Chip, DIRECTION_NORTH));
    if (slipdir == DIRECTION_NIL)
      continue;
    if (slipper->actor->id != Chip)
      continue; /* new, non-Chip ignored */
    // TODO: see if the loop aspect of this can't be crushed out to just acting
    // directly on Chip
    self->ms_state.chip_last_slip_dir = slipdir;
    bool advanced = Actor_advance_movement(slipper->actor, self,
                                           slipdir); /* useful to have ac */
    if (advanced) {
      slipper->actor->state &= ~CS_HASMOVED;
    } else {
      TileID floor = Level_cell_get_bottom_floor(self, slipper->actor->pos);
      if (TileID_is_slide(floor)) {
        slipper->actor->state &= ~CS_HASMOVED;
      } else if (TileID_is_ice(floor)) {
        slipdir = get_ice_wall_turn_dir(floor, Direction_back(slipdir));
        self->ms_state.chip_last_slip_dir = slipdir;
        advanced = Actor_advance_movement(slipper->actor, self,
                                          slipdir); /* again useful with ac */
        if (advanced)
          slipper->actor->state &= ~CS_HASMOVED;
      } else if (floor == Teleport || floor == Block_Static) {
        self->ms_state.chip_last_slip_dir = slipdir = Direction_back(slipdir);
        if (Actor_advance_movement(slipper->actor, self, slipdir))
          slipper->actor->state &= ~CS_HASMOVED;
      }
      if (slipper->actor->state & (CS_SLIP | CS_SLIDE)) {
        Actor_end_floor_movement(slipper->actor, self);
        Actor_start_floor_movement(
            slipper->actor, self,
            Level_cell_get_bottom_floor(self, slipper->actor->pos),
            DIRECTION_NIL); /* 3rd argument with tank reversal patch */
      }
    }
    if (Level_check_for_ending(self))
      return;
  }
}

static void Level_non_chip_floor_movements(Level* self) /* split into two */
{
  int64_t advance = 0;

  for (uint32_t n = 0; n < self->ms_state.slip_count;) {
    uint32_t oldmsccslippers = self->ms_state.mscc_slippers;
    MsSlipper* slipper = &self->ms_state.slip_list[n];
    if (slipper->actor->id == Chip) {
      /* new splitting */
      ++n;
      continue;
    }
    if (advance) {
      advance--;
      ++n;
      continue;
    }
    if (!(self->ms_state.slip_list[n].actor->state & (CS_SLIP | CS_SLIDE))) {
      ++n;
      continue;
    }
    Direction slipdir = self->ms_state.slip_list[n].direction;
    Direction origdir = slipdir; /* tank reversal patch */
    if (slipdir == DIRECTION_NIL) {
      ++n;
      continue;
    }
    Actor_set_spare_direction(slipper->actor,
                              slipper->direction); /* Tank Top Glitch */
    bool ac = Actor_advance_movement(slipper->actor, self,
                                     slipdir); /* useful to have ac */
    if (!ac) {
      TileID floor = Level_cell_get_bottom_floor(self, slipper->actor->pos);
      if (TileID_is_ice(floor)) {
        slipdir = get_ice_wall_turn_dir(floor, Direction_back(slipdir));
        ac = Actor_advance_movement(slipper->actor, self,
                                    slipdir); /* again useful with ac */
      }
      if (slipper->actor->state & (CS_SLIP | CS_SLIDE)) {
        Actor_end_floor_movement(slipper->actor, self);
        self->ms_state.mscc_slippers--; /* new MSCC accounting */
        Actor_start_floor_movement(
            slipper->actor, self,
            Level_cell_get_bottom_floor(self, slipper->actor->pos),
            ac ? DIRECTION_NIL
               : origdir); /* 3rd argument with tank reversal patch */
      }
    }
    if (slipper->actor->state & CS_SLIP && ac)
      slipper->actor->state |= CS_SLIDE; /* Tank Top Glitch */
    Actor_set_spare_direction(slipper->actor, DIRECTION_NIL);  // tank top
                                                               // glitch
    if (Level_check_for_ending(self))
      return;
    if (self->ms_state.mscc_slippers == oldmsccslippers)
      advance++;
  }
}

static void Level_do_floor_movements(Level* self) /* split version with patch */
{
  Level_chip_floor_movements(self);
  Level_update_sliplist(self); /* remove deadwood */
  /* TSG stuff, not yet included */
  if (!Level_check_for_ending(self)) /* Squish patch (maybe was oversight?) */
    Level_non_chip_floor_movements(self);
  if (!self->level_complete && self->ms_state.chip_status == CHIP_SQUISHED)
    self->ms_state.chip_status = CHIP_SQUISHED_DEATH;
}

static void Level_create_clones(Level* self) {
  for (uint32_t n = 0; n < self->ms_state.actor_count; ++n) {
    if (self->actors[n].state & CS_CLONING) {
      self->actors[n].state &= ~CS_CLONING;
    }
  }
}

static bool ms_init_level(Level* self) {
  self->actors = xcalloc(MAX_CREATURES, sizeof(Actor));

  self->status_flags &= ~SF_BAD_TILES;
  self->status_flags |= SF_NO_ANIMATION;

  Position pos;
  MapCell* cell;
  for (pos = 0, cell = self->map; pos < MAP_WIDTH * MAP_HEIGHT; ++pos, ++cell) {
    if (TileID_is_terrain(cell->top.id) ||
        TileID_actor_get_id(cell->top.id) == Chip ||
        TileID_actor_get_id(cell->top.id) == Block) {
      if (cell->bottom.id == Teleport || cell->bottom.id == SwitchWall_Open ||
          cell->bottom.id == SwitchWall_Closed) {
        cell->bottom.state |= FS_BROKEN;
      }
    }
  }

  Actor* chip = Level_create_actor(self);
  chip->pos = 0;
  chip->id = Chip;
  chip->direction = DIRECTION_SOUTH;
  Actor_add_to_map(chip, self);
  for (uint32_t n = 0; n < self->ms_state.init_actors_n; ++n) {
    pos = self->ms_state.init_actor_list[n];
    if (pos < 0 || pos >= MAP_WIDTH * MAP_HEIGHT) {
      warn("level has invalid creature location (%d %d)", pos % MAP_WIDTH,
           pos / MAP_WIDTH);
      continue;
    }
    cell = Level_get_map_cell(self, pos);
    TileID top_id = Level_cell_get_top_floor(self, pos);
    TileID bottom_id = Level_cell_get_bottom_floor(self, pos);
    if (!TileID_is_actor(top_id)) {
      warn("level has no creature at location (%d %d)", pos % MAP_WIDTH,
           pos / MAP_WIDTH);
      continue;
    }
    if (TileID_actor_get_id(top_id) != Block && bottom_id != CloneMachine) {
      Actor* actor = Level_create_actor(self);
      actor->pos = pos;
      actor->id = TileID_actor_get_id(top_id);
      actor->direction = TileID_actor_get_dir(top_id);
      if (TileID_is_actor(bottom_id) &&
          TileID_actor_get_id(bottom_id) == Chip) {
        chip->pos = pos;
        chip->direction = TileID_actor_get_dir(bottom_id);
      }
    }
    cell->top.state |= FS_MARKER;
  }
  for (pos = 0, cell = self->map; pos < MAP_WIDTH * MAP_HEIGHT; ++pos, ++cell) {
    MapTile* top_tile = MapCell_get_top_tile(cell);
    MapTile* bottom_tile = MapCell_get_bottom_tile(cell);
    if (cell->top.state & FS_MARKER) {
      MapTile_remove_marker_state(top_tile);
    } else if (TileID_is_actor(MapTile_get_floor(top_tile)) &&
               TileID_actor_get_id(MapTile_get_floor(top_tile)) == Chip) {
      chip->pos = pos;
      chip->direction = TileID_actor_get_dir(MapTile_get_floor(bottom_tile));
    }
  }

  ConnList* traps = &self->trap_connections;
  for (uint8_t n = 0; n < traps->length; ++n) {
    if (traps->items[n].to == Level_get_chip(self)->pos ||
        Level_cell_get_top_floor(self, traps->items[n].to) == Block_Static ||
        Level_is_trap_button_down(self, traps->items[n].from)) {
      Level_spring_trap(self, traps->items[n].from);
    }
  }

  return true;
}

static void ms_uninit_level(Level* level) {
  free(level->actors);
}

/* Advance the game state by one tick.
 */
static void ms_tick_level(Level* self) {
  Actor* cr;
  int n;

  // timeoffset() = -1;    int n;

  if (!(self->current_tick & 3)) {
    for (n = 1; n < self->ms_state.actor_count; ++n) {
      if (self->actors[n].state & CS_TURNING) {
        self->actors[n].state &= ~(CS_TURNING | CS_HASMOVED);
        Actor_update_floor(&self->actors[n], self);
      }
    }
    self->ms_state.chip_ticks_since_moved++;
    if (self->ms_state.chip_ticks_since_moved > 3) {
      self->ms_state.chip_ticks_since_moved = 3;
      if (Level_get_chip(self)->direction !=
          DIRECTION_NIL) /* Convergence Glitch patch (a) */
        Level_get_chip(self)->direction = DIRECTION_SOUTH;
      Actor_update_floor(Level_get_chip(self), self);
    }
  }

  self->ms_state.mscc_slippers = self->ms_state.slip_count;
  if (Level_get_chip(self)->state & (CS_SLIP | CS_SLIDE)) /* new accounting */
    self->ms_state.mscc_slippers--;

  if (self->current_tick && !(self->current_tick & 1)) {
    self->ms_state.controller_dir = DIRECTION_NIL;
    for (n = 0; n < self->ms_state.actor_count; ++n) {
      cr = &self->actors[n];
      if (!cr->hidden && cr->id != Chip && !(self->current_tick & 3) &&
          self->ms_state.chip_status == CHIP_SQUISHED && !self->level_complete)
        self->ms_state.chip_status = CHIP_SQUISHED_DEATH; /* Squish patch */
      if (cr->hidden || (cr->state & CS_CLONING) || cr->id == Chip)
        continue;
      Actor_choose_move(cr, self);
      if (cr->move_decision != DIRECTION_NIL)
        Actor_advance_movement(cr, self, cr->move_decision);
    }
    if (Level_check_for_ending(self))
      return;
  }

  if (self->current_tick && !(self->current_tick & 1)) {
    Level_do_floor_movements(self);
    if (Level_check_for_ending(self))
      return;
  }
  Level_update_sliplist(self);

  // timeoffset() = 0;
  if (self->time_limit) {
    if (self->current_tick >= self->time_limit) {
      self->ms_state.chip_status = CHIP_OUTOFTIME;
      Level_add_sfx(self, SND_TIME_OUT);
      return;
    } else if (self->time_limit - self->current_tick <= 15 * 20 &&
               self->current_tick % 20 == 0) {
      Level_add_sfx(self, SND_TIME_LOW);
    }
  }

  cr = Level_get_chip(self);
  Actor_choose_move(cr, self);
  if (cr->move_decision != DIRECTION_NIL) {
    Actor_advance_movement(
        cr, self, cr->move_decision); /* Squish patch, TW checked this?! */
    if (Level_check_for_ending(self)) /* TW checks advancecreature() status */
      return; /* guess it's a remnant of Chip starting on exit? */
    cr->state |= CS_HASMOVED;
  }
  Level_update_sliplist(self);
  Level_create_clones(self);
}

Ruleset const ms_logic = {.id = Ruleset_MS,
                          .init_level = ms_init_level,
                          .tick_level = ms_tick_level,
                          .uninit_level = ms_uninit_level};
