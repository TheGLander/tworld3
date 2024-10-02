from ctypes import (
    CDLL,
    Structure,
    Union,
    addressof,
    c_bool,
    c_char_p,
    c_int16,
    c_int8,
    c_uint16,
    c_uint32,
    c_uint8,
    c_void_p,
    create_string_buffer,
)
from enum import Enum, IntEnum
from dataclasses import dataclass
from os import getcwd
from pathlib import Path
from typing import Iterator, Optional
from itertools import count as range_inf

from PySide6.QtCore import QPoint


def get_libchips_path():
    cwd = Path(__file__).parent.parent
    cwd_lib = cwd / "libchips.so"
    if cwd_lib.exists():
        return str(cwd_lib)
    local_build_lib = cwd / "libchips" / "build" / "libchips.so"
    if local_build_lib.exists():
        return str(local_build_lib)
    raise Exception("Couldn't find libchips")


libchips = CDLL(get_libchips_path())


class LibchipsError(Exception):
    pass


def Result(val_type: type):
    class Result_union(Union):
        _fields_ = [("value", val_type), ("error", c_char_p)]

    class Result_struct(Structure):
        _fields_ = [("success", c_bool), ("anon", Result_union)]
        _anonymous_ = ("anon",)

        def raise_if_error(self):
            if self.success:
                return
            raise LibchipsError(self.error.decode("ascii"))

    return Result_struct


class Ruleset(c_void_p):
    class RulesetID(Enum):
        Nil = 0
        Lynx = 1
        Ms = 2

    @staticmethod
    def from_global_var(var: str):
        return Ruleset(addressof(c_bool.in_dll(libchips, var)))

    @property
    def id(self):
        return self.RulesetID(libchips.Ruleset_get_id(self))


lynx_logic = Ruleset.from_global_var("lynx_logic")
ms_logic = Ruleset.from_global_var("ms_logic")


class Direction(Enum):
    Nil = 0
    North = 1
    Northwest = 1 | 2
    Northeast = 1 | 8
    South = 4
    Southwest = 4 | 2
    Southeast = 4 | 8
    West = 2
    East = 8

    def to_idx(self):
        return (0x30210 >> (self.value * 2)) & 3

    @staticmethod
    def from_idx(idx: int):
        return Direction(1 << idx)

    def add_dir(self, dir: "Direction") -> "Direction":
        return Direction(self.value | dir.value)

    def remove_dir(self, dir: "Direction") -> "Direction":
        return Direction(self.value & ~dir.value)


class TileID(Enum):
    Nothing = 0

    Empty = 0x01

    Slide_North = 0x02
    Slide_West = 0x03
    Slide_South = 0x04
    Slide_East = 0x05
    Slide_Random = 0x06
    Ice = 0x07
    IceWall_Northwest = 0x08
    IceWall_Northeast = 0x09
    IceWall_Southwest = 0x0A
    IceWall_Southeast = 0x0B
    Gravel = 0x0C
    Dirt = 0x0D
    Water = 0x0E
    Fire = 0x0F
    Bomb = 0x10
    Beartrap = 0x11
    Burglar = 0x12
    HintButton = 0x13
    Button_Blue = 0x14
    Button_Green = 0x15
    Button_Red = 0x16
    Button_Brown = 0x17
    Teleport = 0x18
    Wall = 0x19
    Wall_North = 0x1A
    Wall_West = 0x1B
    Wall_South = 0x1C
    Wall_East = 0x1D
    Wall_Southeast = 0x1E
    HiddenWall_Perm = 0x1F
    HiddenWall_Temp = 0x20
    BlueWall_Real = 0x21
    BlueWall_Fake = 0x22
    SwitchWall_Open = 0x23
    SwitchWall_Closed = 0x24
    PopupWall = 0x25
    CloneMachine = 0x26
    Door_Red = 0x27
    Door_Blue = 0x28
    Door_Yellow = 0x29
    Door_Green = 0x2A
    Socket = 0x2B
    Exit = 0x2C
    ICChip = 0x2D
    Key_Red = 0x2E
    Key_Blue = 0x2F
    Key_Yellow = 0x30
    Key_Green = 0x31
    Boots_Ice = 0x32
    Boots_Slide = 0x33
    Boots_Fire = 0x34
    Boots_Water = 0x35
    Block_Static = 0x36
    Drowned_Chip = 0x37
    Burned_Chip = 0x38
    Bombed_Chip = 0x39
    Exited_Chip = 0x3A
    Exit_Extra_1 = 0x3B
    Exit_Extra_2 = 0x3C
    Overlay_Buffer = 0x3D
    Floor_Reserved2 = 0x3E
    Floor_Reserved1 = 0x3F

    Chip = 0x40
    Block = 0x44
    Tank = 0x48
    Ball = 0x4C
    Glider = 0x50
    Fireball = 0x54
    Walker = 0x58
    Blob = 0x5C
    Teeth = 0x60
    Bug = 0x64
    Paramecium = 0x68
    Swimming_Chip = 0x6C
    Pushing_Chip = 0x70
    Entity_Reserved2 = 0x74
    Entity_Reserved1 = 0x78
    Water_Splash = 0x7C
    Bomb_Explosion = 0x7D
    Entity_Explosion = 0x7E
    Animation_Reserved1 = 0x7F

    def is_actor(self):
        return TileID.Animation_Reserved1.value >= self.value >= TileID.Chip.value

    def is_animation(self):
        return (
            TileID.Animation_Reserved1.value >= self.value >= TileID.Water_Splash.value
        )

    @staticmethod
    def from_libchips(id: int) -> "AnyTileID":
        pure_id = TileID(id & ~3)
        if pure_id.is_actor():
            return (pure_id, Direction.from_idx(id & 3))
        return TileID(id)


AnyTileID = TileID | tuple[TileID, Direction]


@dataclass
class Position:
    x: int
    y: int

    def to_int(self):
        return self.x + self.y * 32

    @staticmethod
    def from_int(pos: int):
        return Position(pos % 32, pos // 32)

    def to_qpoint(self):
        return QPoint(self.x, self.y)


libchips.Actor_get_position.restype = c_int16
libchips.Actor_get_id.restype = c_uint8
libchips.Actor_get_move_cooldown.restype = c_int8
libchips.Actor_get_animation_frame.restype = c_int8
libchips.Actor_get_hidden.restype = c_bool
libchips.Actor_get_direction.restype = c_uint8


class Actor(c_void_p):
    @property
    def position(self) -> Position:
        return Position.from_int(libchips.Actor_get_position(self))

    def visual_position(self, interpolation: float = 1) -> tuple[float, float]:
        base_pos = self.position
        x = float(base_pos.x)
        y = float(base_pos.y)
        if self.move_cooldown == 0:
            return (x, y)
        if self.id.is_animation():
            interpolation = 1
        offset = (self.move_cooldown + 1 - interpolation) / 8

        match self.direction:
            case Direction.North:
                y += offset
            case Direction.West:
                x += offset
            case Direction.South:
                y -= offset
            case Direction.East:
                x -= offset
            case _:
                pass
        return (x, y)

    @property
    def direction(self) -> Direction:
        return Direction(libchips.Actor_get_direction(self))

    @property
    def id(self) -> TileID:
        return TileID(libchips.Actor_get_id(self))

    @property
    def full_id(self) -> AnyTileID:
        id = self.id
        if not id.is_actor() or id.is_animation():
            return id
        return (id, self.direction)

    @property
    def move_cooldown(self) -> int:
        return libchips.Actor_get_move_cooldown(self)

    @property
    def animation_frame(self) -> int:
        return libchips.Actor_get_animation_frame(self)

    @property
    def hidden(self) -> bool:
        return libchips.Actor_get_hidden(self)


libchips.Level_get_ruleset.restype = Ruleset
libchips.Level_get_time_offset.restype = c_int8
libchips.Level_get_time_limit.restype = c_uint32
libchips.Level_get_current_tick.restype = c_uint32
libchips.Level_get_status_flags.restype = c_uint16
libchips.Level_get_sfx.restype = c_uint32
libchips.Level_get_top_terrain.restype = c_uint8
libchips.Level_get_bottom_terrain.restype = c_uint8
libchips.Level_get_actor_by_idx.restype = Actor
libchips.Level_get_game_input.restype = c_uint16
libchips.Level_get_win_state.restype = c_int8
libchips.Level_get_metadata.restype = c_void_p
libchips.Level_player_has_item.restype = c_bool


class GameInput(int):
    @staticmethod
    def none() -> "GameInput":
        return GameInput(0)

    @staticmethod
    def dir(dir: Direction) -> "GameInput":
        return GameInput(dir.value)

    @staticmethod
    def mouse_abs(pos: Position) -> "GameInput":
        return GameInput(512 + pos.to_int())


class TriRes(Enum):
    Died = -1
    Nothing = 0
    Success = 1


class Level(c_void_p):
    class StatusFlags(IntEnum):
        InvalidLevel = 0x2
        HasBadTiles = 0x4
        ShowHint = 0x8
        DontAnimateActors = 0x10
        ShutterWhenPaused = 0x20

    @property
    def ruleset(self) -> Ruleset:
        return libchips.Level_get_ruleset(self)

    @property
    def time_offset(self) -> int:
        return libchips.Level_get_time_offset(self)

    @property
    def time_limit(self) -> int:
        return libchips.Level_get_time_limit(self)

    @property
    def current_tick(self) -> int:
        return libchips.Level_get_current_tick(self)

    @property
    def chips_left(self) -> int:
        return libchips.Level_get_chips_left(self)

    # @property
    # def chips_left(self):
    #     return libchips.Level_get_chips_left(self)
    @property
    def status_flags(self) -> int:
        return libchips.Level_get_status_flags(self)

    @property
    def sfx(self) -> int:
        return libchips.Level_get_sfx(self)

    @property
    def win_state(self) -> TriRes:
        return TriRes(libchips.Level_get_win_state(self))

    def get_top_terrain(self, pos: Position) -> AnyTileID:
        return TileID.from_libchips(libchips.Level_get_top_terrain(self, pos.to_int()))

    def get_bottom_terrain(self, pos: Position) -> AnyTileID:
        return TileID.from_libchips(
            libchips.Level_get_bottom_terrain(self, pos.to_int())
        )

    def get_actor_by_idx(self, idx: int) -> Actor:
        return libchips.Level_get_actor_by_idx(self, idx)

    @property
    def chip(self) -> Actor:
        return self.get_actor_by_idx(0)

    def tick(self):
        libchips.Level_tick(self)

    @property
    def game_input(self) -> GameInput:
        return GameInput(libchips.Level_get_game_input(self))

    @game_input.setter
    def game_input(self, input: GameInput):
        libchips.Level_set_game_input(self, input)

    @property
    def actors(self) -> Iterator[Actor]:
        for idx in range_inf(0):
            actor = self.get_actor_by_idx(idx)
            if actor.id == TileID.Nothing:
                break
            yield actor

    size = (32, 32)

    @property
    def metadata(self) -> Optional["LevelMetadata"]:
        ptr: int | None = libchips.Level_get_metadata(self)
        return LevelMetadata(ptr) if ptr != None else None

    def player_has_item(self, item: TileID):
        return libchips.Level_player_has_item(self, item.value)


libchips.LevelMetadata_get_title.restype = c_char_p
libchips.LevelMetadata_get_level_number.restype = c_uint16
libchips.LevelMetadata_get_time_limit.restype = c_uint16
libchips.LevelMetadata_get_chips_required.restype = c_uint16
libchips.LevelMetadata_get_password.restype = c_char_p
libchips.LevelMetadata_get_hint.restype = c_char_p
libchips.LevelMetadata_get_author.restype = c_char_p

libchips.LevelMetadata_make_level.restype = Result(Level)


class LevelMetadata(c_void_p):
    @property
    def title(self) -> str | None:
        ptr = libchips.LevelMetadata_get_title(self)
        return ptr.decode("latin-1") if ptr != None else None

    @property
    def level_number(self) -> int:
        return libchips.LevelMetadata_get_level_number(self)

    @property
    def time_limit(self) -> int:
        return libchips.LevelMetadata_get_time_limit(self)

    @property
    def chips_required(self) -> int:
        return libchips.LevelMetadata_get_chips_required(self)

    @property
    def password(self) -> str:
        return libchips.LevelMetadata_get_password(self).decode("latin-1")

    @property
    def hint(self) -> str | None:
        ptr = libchips.LevelMetadata_get_hint(self)
        return ptr.decode("latin-1") if ptr != None else None

    @property
    def author(self) -> str | None:
        ptr = libchips.LevelMetadata_get_author(self)
        return ptr.decode("latin-1") if ptr != None else None

    def make_level(self, ruleset: Ruleset) -> Level:
        level_res = libchips.LevelMetadata_make_level(self, ruleset)
        level_res.raise_if_error()
        return level_res.value


libchips.LevelSet_get_levels_n.restype = c_uint16
libchips.LevelSet_get_level.restype = LevelMetadata
libchips.LevelSet_get_name.restype = c_char_p


class LevelSet(c_void_p):
    @property
    def name(self) -> Optional[str]:
        ptr = libchips.LevelSet_get_name(self)
        return ptr.decode("latin-1") if ptr != None else None

    @name.setter
    def name(self, val: str | None):
        if val == None:
            libchips.LevelSet_set_name(self, c_void_p(0))
            return
        buf = create_string_buffer(val.encode("utf-8"))
        libchips.LevelSet_set_name(self, buf)

    @property
    def levels_n(self) -> int:
        return libchips.LevelSet_get_levels_n(self)

    def get_level(self, idx: int) -> LevelMetadata:
        return libchips.LevelSet_get_level(self, idx)

    @property
    def levels(self) -> Iterator[LevelMetadata]:
        for level_idx in range(self.levels_n):
            yield self.get_level(level_idx)


libchips.parse_ccl.restype = Result(LevelSet)


def parse_ccl(data: bytes) -> LevelSet:
    levelset_bytes = create_string_buffer(data)
    parse_result = libchips.parse_ccl(levelset_bytes, len(levelset_bytes) - 1)
    parse_result.raise_if_error()
    return parse_result.value
