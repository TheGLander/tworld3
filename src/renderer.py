from typing import Callable, Optional
from time import monotonic as now

from PySide6.QtCore import QPointF, QRect, QRectF, QSize
from PySide6.QtGui import QPaintEvent, QPainter
from PySide6.QtWidgets import QSizePolicy, QWidget

from libchips import Actor, AnyTileID, Level, Position
from tileset import Tileset

GlobalRepaintCallback = Callable[..., None]


def scale_rect(src: QRectF, factor: float):
    src.moveLeft(int(src.left() * factor))
    src.moveTop(int(src.top() * factor))
    src.setWidth(int(src.width() * factor))
    src.setHeight(int(src.height() * factor))


class LevelRenderer(QWidget):
    painter: QPainter
    level: Optional[Level] = None
    tileset: Tileset
    camera_pos: QPointF
    camera_size: QSize = QSize(9, 9)
    tile_scale: float = 1.0
    request_global_repaint: Optional[GlobalRepaintCallback] = None
    auto_draw: bool = True

    interpolation: float = 1
    expected_tps = 20
    last_tick: float = 0
    do_interpolation = True

    def __init__(self, tileset: Tileset, parent=None) -> None:
        super().__init__(parent)
        self.tileset = tileset
        self.painter = QPainter()
        self.rescale()
        self.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

    def rescale(self):
        self.setFixedSize(
            int(self.tileset.tile_size * self.tile_scale * self.camera_size.width()),
            int(self.tileset.tile_size * self.tile_scale * self.camera_size.height()),
        )

    def level_updated(self):
        self.last_tick = now()

    def draw_terrain(self, pos: Position, terrain: AnyTileID):
        tile_pos = self.tileset.get_image_for_terrain(terrain)
        self.blit(QRectF(pos.x, pos.y, 1, 1), tile_pos)

    def draw_actor(self, actor: Actor):
        pos = actor.visual_position(self.interpolation)
        target_pos = QRectF(pos[0], pos[1], 1, 1)
        tile_pos = self.tileset.get_image_for_actor(actor)
        self.blit(target_pos, tile_pos)

    def blit(self, target: QRectF, source_i: QRect):
        source = source_i.toRectF()
        scale_rect(source, self.tileset.tile_size)
        target.translate(-self.camera_pos)
        scale_rect(target, self.tile_scale * self.tileset.tile_size)
        self.painter.drawImage(target, self.tileset.image, source)

    def recalc_camera(self):
        if not self.level:
            return
        pos = self.level.chip.visual_position(self.interpolation)
        self.camera_pos = QPointF(
            pos[0] - self.camera_size.width() / 2 + 0.5,
            pos[1] - self.camera_size.height() / 2 + 0.5,
        )

    def draw_viewport(self):
        if not self.level:
            return
        if self.do_interpolation:
            time_since_tick = now() - self.last_tick
            self.interpolation = min(1, time_since_tick * self.expected_tps)
        else:
            self.interpolation = 1
        self.painter.begin(self)
        self.recalc_camera()
        top_left = self.camera_pos.toPoint()
        # Draw terrain
        for dy in range(-1, self.camera_size.width() + 2):
            for dx in range(-1, self.camera_size.height() + 2):
                pos = Position(top_left.x() + dx, top_left.y() + dy)
                bottom_terrain = self.level.get_bottom_terrain(pos)
                self.draw_terrain(pos, bottom_terrain)
                top_terrain = self.level.get_top_terrain(pos)
                self.draw_terrain(pos, top_terrain)
        # Draw actors
        for actor in self.level.actors:
            self.draw_actor(actor)
        self.painter.end()

    def paintEvent(self, event: QPaintEvent) -> None:
        try:
            self.draw_viewport()
        finally:
            if self.painter.isActive():
                self.painter.end()
        if self.auto_draw and self.request_global_repaint:
            self.request_global_repaint()
