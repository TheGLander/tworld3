from abc import ABC, abstractmethod
from typing import Dict
from libchips import Actor, AnyTileID, Direction, TileID
from PySide6.QtGui import QImage, QPainter, qRgb
from PySide6.QtCore import QPoint, QRect, Qt


class Tileset(ABC):
    image: QImage
    tile_size: int

    @staticmethod
    @abstractmethod
    def looks_like_this_tileset(image: QImage) -> bool:
        pass

    @abstractmethod
    def get_image_for_terrain(self, id: AnyTileID) -> QRect:
        pass

    @abstractmethod
    def get_image_for_actor(self, actor: Actor) -> QRect:
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

ms_tileset_tileid_to_pos: Dict[TileID | tuple[TileID, Direction], QRect] = {}
for idx, tile in enumerate(ms_tileset_idx_to_tileid_list):
    ms_tileset_tileid_to_pos[tile] = QRect(idx // 16, idx % 16, 1, 1)


def apply_mask(image: QImage, mask: QImage) -> QImage:
    new_image = image.convertToFormat(QImage.Format.Format_ARGB32)
    new_image.setAlphaChannel(mask)
    return new_image


class TwMsTileset(Tileset):
    def __init__(self, image: QImage) -> None:
        if not self.looks_like_this_tileset(image):
            raise Exception("Invalid tileset image")
        self.image = apply_mask(
            image,
            image.createMaskFromColor(qRgb(255, 0, 255), Qt.MaskMode.MaskOutColor),
        )
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

    def get_image_for_terrain(self, id: AnyTileID) -> QRect:
        rect = ms_tileset_tileid_to_pos[id]
        return QRect(rect)

    def get_image_for_actor(self, actor: Actor) -> QRect:
        return ms_tileset_tileid_to_pos[(actor.id, actor.direction)]
