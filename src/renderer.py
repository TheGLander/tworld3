from typing import Callable, Optional
from time import monotonic as now

from PySide6.QtCore import QPoint, QPointF, QRect, QRectF, QSize, QSizeF, Signal
from PySide6.QtGui import QCursor, QMouseEvent, QPaintEvent, QPainter, Qt
from PySide6.QtWidgets import QSizePolicy, QWidget

from libchips import Actor, AnyTileID, Direction, GameInput, Level, Position, TileID
from tileset import RenderPosition, Tileset

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

    _clickable = False

    @property
    def clickable(self) -> bool:
        return self._clickable

    @clickable.setter
    def clickable(self, val: bool):
        if val:
            self.setCursor(Qt.CursorShape.CrossCursor)
        else:
            self.setCursor(Qt.CursorShape.ArrowCursor)
        self._clickable = val

    def get_repaint(self):
        # HACK: Apparently calling `winId` on a widget causes it to suddenly know its QWindow, which it otherwise might not provide?
        self.winId()
        window = self.windowHandle()
        if not window:
            return None
        return window.requestUpdate

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.painter = QPainter()
        self.request_global_repaint = self.get_repaint()
        self.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

    def rescale(self):
        self.setFixedSize(
            int(self.tileset.tile_size * self.tile_scale * self.camera_size.width()),
            int(self.tileset.tile_size * self.tile_scale * self.camera_size.height()),
        )

    def level_updated(self):
        self.last_tick = now()

    def draw_terrain(self, pos: Position, terrain: AnyTileID):
        if not self.level:
            return
        tile_pos = self.tileset.get_image_for_terrain(terrain, self.level.current_tick)
        self.blit(
            QRectF(pos.x, pos.y, 1, 1),
            QRect(
                tile_pos.x(),
                tile_pos.y(),
                self.tileset.tile_size,
                self.tileset.tile_size,
            ),
        )

    def draw_actor(self, actor: Actor):
        render_pos, tile_pos = self.tileset.get_image_for_actor(actor)
        if render_pos == RenderPosition.Stretch:
            sprite_size = (
                QSize(1, 2)
                if actor.direction in (Direction.North, Direction.South)
                else QSize(2, 1)
            )
            tile_pos = QRect(tile_pos, sprite_size * self.tileset.tile_size)
            actor_pos = actor.position
            target_pos = QRectF(
                actor_pos.x - (1 if actor.direction == Direction.East else 0),
                actor_pos.y - (1 if actor.direction == Direction.South else 0),
                sprite_size.width(),
                sprite_size.height(),
            )
        elif render_pos == RenderPosition.ThreeByThree:
            tile_pos = QRect(
                tile_pos.x(),
                tile_pos.y(),
                self.tileset.tile_size * 3,
                self.tileset.tile_size * 3,
            )
            actor_pos = actor.visual_position(self.interpolation)
            target_pos = QRectF(actor_pos[0] - 1, actor_pos[1] - 1, 3, 3)
        else:
            tile_pos = QRect(
                tile_pos.x(),
                tile_pos.y(),
                self.tileset.tile_size,
                self.tileset.tile_size,
            )
            actor_pos = actor.visual_position(self.interpolation)
            target_pos = QRectF(*actor_pos, 1, 1)
        self.blit(target_pos, tile_pos)

    def blit(self, target: QRectF, source: QRect):
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
        if self.camera_pos.x() < 0:
            self.camera_pos.setX(0)
        elif self.camera_pos.x() + self.camera_size.width() >= self.level.size[0]:
            self.camera_pos.setX(self.level.size[0] - self.camera_size.width())
        if self.camera_pos.y() < 0:
            self.camera_pos.setY(0)
        elif self.camera_pos.y() + self.camera_size.height() >= self.level.size[1]:
            self.camera_pos.setY(self.level.size[1] - self.camera_size.height())

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
                if pos.x < 0 or pos.y < 0 or pos.x > 31 or pos.y > 31:
                    continue
                bottom_terrain = self.level.get_bottom_terrain(pos)
                self.draw_terrain(pos, bottom_terrain)
                top_terrain = self.level.get_top_terrain(pos)
                self.draw_terrain(pos, top_terrain)
        # Draw actors
        for actor in self.level.actors:
            if actor.hidden or self.level.status_flags & Level.StatusFlags.DontAnimateActors :
                continue
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

    mouse_press = Signal(GameInput)

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if not self._clickable:
            return super().mousePressEvent(event)
        tile_size = int(self.tile_scale * self.tileset.tile_size)
        top_left_px_pos = QPoint(
            int(self.camera_pos.x() * tile_size), int(self.camera_pos.y() * tile_size)
        )
        px_pos = event.pos() + top_left_px_pos
        pos = Position(px_pos.x() // tile_size, px_pos.y() // tile_size)
        self.mouse_press.emit(GameInput.mouse_abs(pos))


class Inventory(QWidget):
    painter: QPainter
    tileset: Tileset
    tile_scale: float = 1.0
    level: Optional[Level] = None

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.painter = QPainter()
        self.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

    def draw_terrain(self, target_p: QPoint, id: TileID):
        tile_size = self.tile_scale * self.tileset.tile_size
        target = QRectF(target_p.x(), target_p.y(), 1, 1)
        scale_rect(target, tile_size)

        source = QRectF(
            self.tileset.get_image_for_terrain(id, 0).toPointF(),
            QSizeF(tile_size, tile_size),
        )
        self.painter.drawImage(target, self.tileset.image, source)

    def rescale(self):
        self.setFixedSize(
            int(self.tileset.tile_size * self.tile_scale * 4),
            int(self.tileset.tile_size * self.tile_scale * 2),
        )

    def paintEvent(self, event: QPaintEvent):
        self.painter.begin(self)
        boots_order = [
            TileID.Boots_Ice,
            TileID.Boots_Slide,
            TileID.Boots_Fire,
            TileID.Boots_Water,
        ]
        keys_order = [
            TileID.Key_Red,
            TileID.Key_Blue,
            TileID.Key_Yellow,
            TileID.Key_Green,
        ]
        for idx, boot in enumerate(boots_order):
            self.draw_terrain(QPoint(idx, 0), TileID.Empty)
            if self.level and self.level.player_has_item(boot):
                self.draw_terrain(QPoint(idx, 0), boot)
        for idx, key in enumerate(keys_order):
            self.draw_terrain(QPoint(idx, 1), TileID.Empty)
            if self.level and self.level.player_has_item(key):
                self.draw_terrain(QPoint(idx, 1), key)
        self.painter.end()
