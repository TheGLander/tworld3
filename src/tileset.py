from abc import ABC, abstractmethod
from enum import Enum
from typing import Dict, List, Sequence
from libchips import Actor, AnyTileID, Direction, TileID
from PySide6.QtGui import QImage, qRgb
from PySide6.QtCore import QPoint, QRect, QSize, Qt


class RenderPosition(Enum):
    Normal = 0
    Stretch = 1
    ThreeByThree = 2


class Tileset(ABC):
    image: QImage
    tile_size: int

    @staticmethod
    @abstractmethod
    def looks_like_this_tileset(image: QImage) -> bool:
        pass

    @abstractmethod
    def get_image_for_terrain(self, id: AnyTileID, tick: int) -> QPoint:
        pass

    @abstractmethod
    def get_image_for_actor(self, actor: Actor) -> tuple[RenderPosition, QPoint]:
        pass


ms_tileset_idx_to_tileid_list = [
    # Row 0
    TileID.Empty,
    TileID.Wall,
    TileID.ICChip,
    TileID.Water,
    TileID.Fire,
    TileID.HiddenWall_Perm,
    TileID.Wall_North,
    TileID.Wall_West,
    TileID.Wall_South,
    TileID.Wall_East,
    TileID.Block_Static,
    TileID.Dirt,
    TileID.Ice,
    TileID.Slide_South,
    # Row 1
    (TileID.Block, Direction.North),
    (TileID.Block, Direction.West),
    (TileID.Block, Direction.South),
    (TileID.Block, Direction.East),
    TileID.Slide_North,
    TileID.Slide_East,
    TileID.Slide_West,
    TileID.Exit,
    TileID.Door_Blue,
    TileID.Door_Red,
    TileID.Door_Green,
    TileID.Door_Yellow,
    TileID.IceWall_Northwest,
    TileID.IceWall_Northeast,
    TileID.IceWall_Southeast,
    TileID.IceWall_Southwest,
    TileID.BlueWall_Fake,
    TileID.BlueWall_Real,
    # Row 2
    1,
    TileID.Burglar,
    TileID.Socket,
    TileID.Button_Green,
    TileID.Button_Red,
    TileID.SwitchWall_Closed,
    TileID.SwitchWall_Open,
    TileID.Button_Brown,
    TileID.Button_Blue,
    TileID.Teleport,
    TileID.Bomb,
    TileID.Beartrap,
    TileID.HiddenWall_Temp,
    TileID.Gravel,
    TileID.PopupWall,
    TileID.HintButton,
    # Row 3
    TileID.Wall_Southeast,
    TileID.CloneMachine,
    TileID.Slide_Random,
    TileID.Drowned_Chip,
    TileID.Burned_Chip,
    TileID.Bombed_Chip,
    1,
    1,
    1,
    TileID.Exited_Chip,
    TileID.Exit_Extra_1,
    TileID.Exit_Extra_2,
    (TileID.Swimming_Chip, Direction.North),
    (TileID.Swimming_Chip, Direction.West),
    (TileID.Swimming_Chip, Direction.South),
    (TileID.Swimming_Chip, Direction.East),
    # Row 4,
    (TileID.Bug, Direction.North),
    (TileID.Bug, Direction.West),
    (TileID.Bug, Direction.South),
    (TileID.Bug, Direction.East),
    (TileID.Fireball, Direction.North),
    (TileID.Fireball, Direction.West),
    (TileID.Fireball, Direction.South),
    (TileID.Fireball, Direction.East),
    (TileID.Ball, Direction.North),
    (TileID.Ball, Direction.West),
    (TileID.Ball, Direction.South),
    (TileID.Ball, Direction.East),
    (TileID.Tank, Direction.North),
    (TileID.Tank, Direction.West),
    (TileID.Tank, Direction.South),
    (TileID.Tank, Direction.East),
    # Row 5
    (TileID.Glider, Direction.North),
    (TileID.Glider, Direction.West),
    (TileID.Glider, Direction.South),
    (TileID.Glider, Direction.East),
    (TileID.Teeth, Direction.North),
    (TileID.Teeth, Direction.West),
    (TileID.Teeth, Direction.South),
    (TileID.Teeth, Direction.East),
    (TileID.Walker, Direction.North),
    (TileID.Walker, Direction.West),
    (TileID.Walker, Direction.South),
    (TileID.Walker, Direction.East),
    (TileID.Blob, Direction.North),
    (TileID.Blob, Direction.West),
    (TileID.Blob, Direction.South),
    (TileID.Blob, Direction.East),
    # Row 6
    (TileID.Paramecium, Direction.North),
    (TileID.Paramecium, Direction.West),
    (TileID.Paramecium, Direction.South),
    (TileID.Paramecium, Direction.East),
    TileID.Key_Blue,
    TileID.Key_Red,
    TileID.Key_Green,
    TileID.Key_Yellow,
    TileID.Boots_Water,
    TileID.Boots_Fire,
    TileID.Boots_Ice,
    TileID.Boots_Slide,
    (TileID.Chip, Direction.North),
    (TileID.Chip, Direction.West),
    (TileID.Chip, Direction.South),
    (TileID.Chip, Direction.East),
]

ms_tileset_tileid_to_pos: Dict[AnyTileID, QPoint] = {}
for idx, tile in enumerate(ms_tileset_idx_to_tileid_list):
    ms_tileset_tileid_to_pos[tile] = QPoint(idx // 16, idx % 16)


def mask_out_color(image: QImage, color: int) -> QImage:
    mask = image.createMaskFromColor(color, Qt.MaskMode.MaskOutColor)
    new_image = image.convertToFormat(QImage.Format.Format_ARGB32)
    new_image.setAlphaChannel(mask)
    return new_image


class TilesetParseError(Exception):
    pass


class TwMsTileset(Tileset):
    def __init__(self, image: QImage) -> None:
        if not self.looks_like_this_tileset(image):
            raise TilesetParseError("Invalid tileset image")
        self.image = mask_out_color(image, qRgb(255, 0, 255))
        self.tile_size = image.height() // 16

    @staticmethod
    def looks_like_this_tileset(image: QImage) -> bool:
        width = image.width()
        height = image.height()
        if width % 7 != 0 or height % 16 != 0:
            return False
        tilesize_w = width / 7
        tilesize_h = height / 16
        return tilesize_w == tilesize_h

    def get_image_for_terrain(self, id: AnyTileID, tick: int) -> QPoint:
        return QPoint(ms_tileset_tileid_to_pos[id])

    def get_image_for_actor(self, actor: Actor) -> tuple[RenderPosition, QPoint]:
        return (
            RenderPosition.Normal,
            ms_tileset_tileid_to_pos[actor.full_id],
        )


class LynxImageType(Enum):
    NormalSingle = 1
    Normal = 2
    Actor = 3
    Animation = 4


lynx_tileset_idx_to_tileid_list: List[tuple[AnyTileID, LynxImageType]] = [
    (TileID.Empty, LynxImageType.NormalSingle),
    (TileID.Slide_North, LynxImageType.Normal),
    (TileID.Slide_West, LynxImageType.Normal),
    (TileID.Slide_South, LynxImageType.Normal),
    (TileID.Slide_East, LynxImageType.Normal),
    (TileID.Slide_Random, LynxImageType.Normal),
    (TileID.Ice, LynxImageType.Normal),
    (TileID.IceWall_Northwest, LynxImageType.Normal),
    (TileID.IceWall_Northeast, LynxImageType.Normal),
    (TileID.IceWall_Southwest, LynxImageType.Normal),
    (TileID.IceWall_Southeast, LynxImageType.Normal),
    (TileID.Gravel, LynxImageType.Normal),
    (TileID.Dirt, LynxImageType.Normal),
    (TileID.Water, LynxImageType.Normal),
    (TileID.Fire, LynxImageType.Normal),
    (TileID.Bomb, LynxImageType.Normal),
    (TileID.Beartrap, LynxImageType.Normal),
    (TileID.Burglar, LynxImageType.Normal),
    (TileID.HintButton, LynxImageType.Normal),
    (TileID.Button_Blue, LynxImageType.Normal),
    (TileID.Button_Green, LynxImageType.Normal),
    (TileID.Button_Red, LynxImageType.Normal),
    (TileID.Button_Brown, LynxImageType.Normal),
    (TileID.Teleport, LynxImageType.Normal),
    (TileID.Wall, LynxImageType.Normal),
    (TileID.Wall_North, LynxImageType.Normal),
    (TileID.Wall_West, LynxImageType.Normal),
    (TileID.Wall_South, LynxImageType.Normal),
    (TileID.Wall_East, LynxImageType.Normal),
    (TileID.Wall_Southeast, LynxImageType.Normal),
    # ( TileID.HiddenWall_Perm,		 TILEIMG_IMPLICIT ),
    # ( TileID.HiddenWall_Temp,		 TILEIMG_IMPLICIT ),
    (TileID.BlueWall_Real, LynxImageType.Normal),
    # ( TileID.BlueWall_Fake,		 TILEIMG_IMPLICIT ),
    (TileID.SwitchWall_Open, LynxImageType.Normal),
    (TileID.SwitchWall_Closed, LynxImageType.Normal),
    (TileID.PopupWall, LynxImageType.Normal),
    (TileID.CloneMachine, LynxImageType.Normal),
    (TileID.Door_Red, LynxImageType.Normal),
    (TileID.Door_Blue, LynxImageType.Normal),
    (TileID.Door_Yellow, LynxImageType.Normal),
    (TileID.Door_Green, LynxImageType.Normal),
    (TileID.Socket, LynxImageType.Normal),
    (TileID.Exit, LynxImageType.Normal),
    (TileID.ICChip, LynxImageType.Normal),
    (TileID.Key_Red, LynxImageType.Normal),
    (TileID.Key_Blue, LynxImageType.Normal),
    (TileID.Key_Yellow, LynxImageType.Normal),
    (TileID.Key_Green, LynxImageType.Normal),
    (TileID.Boots_Ice, LynxImageType.Normal),
    (TileID.Boots_Slide, LynxImageType.Normal),
    (TileID.Boots_Fire, LynxImageType.Normal),
    (TileID.Boots_Water, LynxImageType.Normal),
    # ( TileID.Block_Static,		 TILEIMG_IMPLICIT ),
    # ( TileID.Overlay_Buffer,		 TILEIMG_IMPLICIT ),
    (TileID.Exit_Extra_1, LynxImageType.NormalSingle),
    (TileID.Exit_Extra_2, LynxImageType.NormalSingle),
    (TileID.Burned_Chip, LynxImageType.NormalSingle),
    (TileID.Bombed_Chip, LynxImageType.NormalSingle),
    (TileID.Exited_Chip, LynxImageType.NormalSingle),
    (TileID.Drowned_Chip, LynxImageType.NormalSingle),
    ((TileID.Swimming_Chip, Direction.North), LynxImageType.NormalSingle),
    ((TileID.Swimming_Chip, Direction.West), LynxImageType.NormalSingle),
    ((TileID.Swimming_Chip, Direction.South), LynxImageType.NormalSingle),
    ((TileID.Swimming_Chip, Direction.East), LynxImageType.NormalSingle),
    (TileID.Chip, LynxImageType.Actor),
    (TileID.Pushing_Chip, LynxImageType.Actor),
    (TileID.Block, LynxImageType.Actor),
    (TileID.Tank, LynxImageType.Actor),
    (TileID.Ball, LynxImageType.Actor),
    (TileID.Glider, LynxImageType.Actor),
    (TileID.Fireball, LynxImageType.Actor),
    (TileID.Bug, LynxImageType.Actor),
    (TileID.Paramecium, LynxImageType.Actor),
    (TileID.Teeth, LynxImageType.Actor),
    (TileID.Blob, LynxImageType.Actor),
    (TileID.Walker, LynxImageType.Actor),
    (TileID.Water_Splash, LynxImageType.Animation),
    (TileID.Bomb_Explosion, LynxImageType.Animation),
    (TileID.Entity_Explosion, LynxImageType.Animation),
]


class TwLynxTileset(Tileset):
    map: Dict[AnyTileID, tuple[RenderPosition, Sequence[QPoint]]]

    def __init__(self, image: QImage) -> None:
        tile_size = TwLynxTileset.detect_image_tilesize(image)
        if tile_size.width() != tile_size.height():
            raise TilesetParseError("Non-square tilesets not supported")
        tile_size = tile_size.width()
        trans_color = image.pixel(1, 0)
        self.map = {}

        # The current pixel position
        pos = QPoint(0, 0)
        # The height of the current row, in tiles
        current_height = 1

        for tile_id, image_type in lynx_tileset_idx_to_tileid_list:
            pixel_width = TwLynxTileset.find_horiz_terminator(image, trans_color, pos)
            while pixel_width == None:
                pos.setX(0)
                pos.setY(pos.y() + current_height * tile_size + 1)
                current_height = TwLynxTileset.find_vert_terminator(
                    image, trans_color, pos
                )
                if current_height == None:
                    raise TilesetParseError("Tileset lacks a final vertical terminator")
                current_height //= tile_size
                pixel_width = TwLynxTileset.find_horiz_terminator(
                    image, trans_color, pos
                )

            if pixel_width % tile_size != 0:
                raise TilesetParseError(
                    f"Invalid tile size: a {pixel_width}x{current_height * tile_size} tile in a {tile_size}x{tile_size} tileset"
                )
            current_width = pixel_width // tile_size
            # print(f"{tile_id}: {current_width}x{current_height}")

            if current_height != 1 and image_type in (
                LynxImageType.NormalSingle,
                LynxImageType.Normal,
            ):
                raise TilesetParseError(
                    f"Non-actor tile {tile_id} must only have one row of tiles"
                )

            first_frame_pos = QPoint(pos.x() + 1, pos.y() + 1)

            if image_type == LynxImageType.NormalSingle:
                if current_width != 1:
                    raise TilesetParseError(f"Tile {tile_id} must only have one frame")
                self.map[tile_id] = (RenderPosition.Normal, [first_frame_pos])
            elif image_type == LynxImageType.Normal:
                frame_positions = [
                    QPoint(first_frame_pos.x() + idx * tile_size, first_frame_pos.y())
                    for idx in range(current_width)
                ]
                self.map[tile_id] = (RenderPosition.Normal, frame_positions)
            elif image_type == LynxImageType.Animation:
                if current_width % 3 != 0 or current_height != 3:
                    raise TilesetParseError(
                        f"Tile {tile_id} is an animation and must have 3x3 frames, received {current_width}x{current_height}"
                    )
                frame_positions = [
                    QPoint(
                        first_frame_pos.x() + 3 * idx * tile_size,
                        first_frame_pos.y(),
                    )
                    for idx in range(current_width // 3)
                ]
                self.map[tile_id] = (RenderPosition.ThreeByThree, frame_positions)
            elif image_type == LynxImageType.Actor:
                if isinstance(tile_id, tuple):
                    raise Exception(
                        f"Internal error: Actor image types must not be directional"
                    )
                north_id = (tile_id, Direction.North)
                west_id = (tile_id, Direction.West)
                south_id = (tile_id, Direction.South)
                east_id = (tile_id, Direction.East)
                # 1x1: Use the one sprite for all directions
                if current_width == 1 and current_height == 1:
                    self.map[north_id] = self.map[west_id] = self.map[south_id] = (
                        self.map[east_id]
                    ) = (RenderPosition.Normal, [first_frame_pos])
                # 1x2 or 2x1: Use one sprite for North/South, and the other for East/West
                elif current_width == 2 and current_height == 1:
                    self.map[north_id] = self.map[south_id] = (
                        RenderPosition.Normal,
                        [first_frame_pos],
                    )
                    self.map[east_id] = self.map[west_id] = (
                        RenderPosition.Normal,
                        [QPoint(first_frame_pos.x() + tile_size, first_frame_pos.y())],
                    )
                elif current_width == 1 and current_height == 2:
                    self.map[north_id] = self.map[south_id] = (
                        RenderPosition.Normal,
                        [first_frame_pos],
                    )
                    self.map[east_id] = self.map[west_id] = (
                        RenderPosition.Normal,
                        [QPoint(first_frame_pos.x(), first_frame_pos.y() + tile_size)],
                    )
                # 4x1 or 2x2: One sprite for each direction
                elif current_width == 4 and current_height == 1:
                    self.map[north_id] = (RenderPosition.Normal, [first_frame_pos])
                    self.map[west_id] = (
                        RenderPosition.Normal,
                        [QPoint(first_frame_pos.x() + tile_size, first_frame_pos.y())],
                    )
                    self.map[south_id] = (
                        RenderPosition.Normal,
                        [
                            QPoint(
                                first_frame_pos.x() + 2 * tile_size, first_frame_pos.y()
                            )
                        ],
                    )
                    self.map[east_id] = (
                        RenderPosition.Normal,
                        [
                            QPoint(
                                first_frame_pos.x() + 3 * tile_size, first_frame_pos.y()
                            )
                        ],
                    )
                elif current_width == 2 and current_height == 2:
                    self.map[north_id] = (RenderPosition.Normal, [first_frame_pos])
                    self.map[west_id] = (
                        RenderPosition.Normal,
                        [QPoint(first_frame_pos.x() + tile_size, first_frame_pos.y())],
                    )
                    self.map[south_id] = (
                        RenderPosition.Normal,
                        [QPoint(first_frame_pos.x(), first_frame_pos.y() + tile_size)],
                    )
                    self.map[east_id] = (
                        RenderPosition.Normal,
                        [
                            QPoint(
                                first_frame_pos.x() + tile_size,
                                first_frame_pos.y() + tile_size,
                            )
                        ],
                    )
                # 8x2: Four sprites for each direction
                elif current_width == 8 and current_height == 2:
                    all_frames = [
                        QPoint(
                            first_frame_pos.x() + (idx % 8) * tile_size,
                            first_frame_pos.y() + (tile_size if idx >= 8 else 0),
                        )
                        for idx in range(16)
                    ]
                    self.map[north_id] = (RenderPosition.Normal, all_frames[0:4])
                    self.map[west_id] = (RenderPosition.Normal, all_frames[4:8])
                    self.map[south_id] = (RenderPosition.Normal, all_frames[8:12])
                    self.map[east_id] = (RenderPosition.Normal, all_frames[12:16])
                # 16x2: A stretchy sprite!
                elif current_width == 16 and current_height == 2:
                    vert_frames = [
                        QPoint(
                            first_frame_pos.x() + idx * tile_size, first_frame_pos.y()
                        )
                        for idx in range(8)
                    ]
                    west_frames = [
                        QPoint(
                            first_frame_pos.x() + tile_size * (8 + 2 * idx),
                            first_frame_pos.y(),
                        )
                        for idx in range(4)
                    ]
                    east_frames = [
                        QPoint(
                            first_frame_pos.x() + tile_size * (8 + 2 * idx),
                            first_frame_pos.y() + tile_size,
                        )
                        for idx in range(4)
                    ]
                    self.map[north_id] = (RenderPosition.Stretch, vert_frames[0:4])
                    self.map[south_id] = (RenderPosition.Stretch, vert_frames[4:8])
                    self.map[west_id] = (RenderPosition.Stretch, west_frames)
                    self.map[east_id] = (RenderPosition.Stretch, east_frames)
                else:
                    raise TilesetParseError(
                        f"Actor {tile_id} has an unrecognized frame layout {current_width}x{current_height}"
                    )
            pos.setX(pos.x() + current_width * tile_size)

        # Map some of the identical-looking tiles
        self.map[TileID.HiddenWall_Perm] = self.map[TileID.HiddenWall_Temp] = self.map[
            TileID.Overlay_Buffer
        ] = self.map[TileID.Empty]
        self.map[TileID.BlueWall_Fake] = self.map[TileID.BlueWall_Real]
        self.map[TileID.Block_Static] = self.map[(TileID.Block, Direction.North)]

        self.image = mask_out_color(image, trans_color)
        self.tile_size = tile_size

    @staticmethod
    def detect_image_tilesize(image: QImage) -> QSize:
        first_terminator = image.pixel(0, 0)
        trans_color = image.pixel(1, 0)
        trans_color_y = image.pixel(0, 1)
        if first_terminator == trans_color or trans_color != trans_color_y:
            raise TilesetParseError("Invalid tile terminators")

        tile_width = TwLynxTileset.find_horiz_terminator(
            image, trans_color, QPoint(0, 0)
        )
        if tile_width == None:
            raise TilesetParseError("Reached end of line before finding a terminator")
        tile_height = TwLynxTileset.find_vert_terminator(
            image, trans_color, QPoint(0, 0)
        )
        if tile_height == None:
            raise TilesetParseError("Reached end of line before finding a terminator")

        return QSize(tile_width, tile_height)

    @staticmethod
    def find_horiz_terminator(
        image: QImage, trans_color: int, pos: QPoint
    ) -> int | None:
        if pos.x() == image.width() - 1:
            return None
        tile_width = 1
        while image.pixel(pos.x() + tile_width, pos.y()) == trans_color:
            tile_width += 1
            if pos.x() + tile_width == image.width():
                return None
        return tile_width

    @staticmethod
    def find_vert_terminator(
        image: QImage, trans_color: int, pos: QPoint
    ) -> int | None:
        if pos.y() == image.height() - 1:
            return None
        tile_height = 1
        while image.pixel(pos.x(), pos.y() + tile_height) == trans_color:
            tile_height += 1
            if pos.y() + tile_height == image.height():
                return None
        return tile_height - 1

    @staticmethod
    def looks_like_this_tileset(image: QImage) -> bool:
        try:
            TwLynxTileset.detect_image_tilesize(image)
        except TilesetParseError:
            return False
        return True

    def get_image_for_terrain(self, id: AnyTileID, tick: int) -> QPoint:
        frames = self.map[id][1]
        return frames[(tick + 1) % len(frames)]

    def get_image_for_actor(self, actor: Actor) -> tuple[RenderPosition, QPoint]:
        render_pos, frames = self.map[actor.full_id]
        frame_idx = actor.animation_frame if len(frames) > 1 else 0
        # TW Lynx actor frames are always played in reverse. I don't know why.
        return (render_pos, frames[-frame_idx - 1])
