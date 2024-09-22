#!/usr/bin/env python3
from typing import Optional
from PySide6.QtCore import QEnum, QEvent, QRect, QTimer
from PySide6.QtGui import QImage, QKeyEvent, QSurfaceFormat, Qt
from PySide6.QtWidgets import (
    QApplication,
    QMainWindow,
    QVBoxLayout,
    QWidget,
)
from libchips import Direction, GameInput, Level, parse_ccl, ms_logic, lynx_logic
from tileset import TwMsTileset
from renderer import LevelRenderer, GlobalRepaintCallback


class MainWindow(QMainWindow):
    renderer: LevelRenderer
    level: Optional[Level]
    main_container: QWidget
    tick_timer: QTimer

    def get_global_repaint_func(self) -> GlobalRepaintCallback:
        # HACK: The only way to repaint anything once every frame is to hijack the lower-level QWindow
        # `requestUpdate` inteded to be used by OpenGL-only applications, which queues a repaint of the
        # whole window in sync with VSync
        return self.windowHandle().requestUpdate

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.tick_timer = QTimer(self)
        self.tick_timer.timeout.connect(self.tick_level)

        self.main_container = QWidget(self)
        layout = QVBoxLayout(self.main_container)
        self.main_container.setLayout(layout)
        self.setCentralWidget(self.main_container)

        self.renderer = LevelRenderer(TwMsTileset(QImage("./tiles.bmp")), self)
        layout.addWidget(self.renderer, alignment=Qt.AlignmentFlag.AlignCenter)

    def start_level(self, level: Level):
        self.level = level
        self.renderer.level = level
        self.renderer.request_global_repaint = self.get_global_repaint_func()
        self.renderer.auto_draw = True
        self.tick_timer.setTimerType(Qt.TimerType.PreciseTimer)
        self.tick_timer.start(1000 // 20)

    def tick_level(self):
        if not self.level:
            return
        self.level.game_input = GameInput(self.current_input.value)
        self.level.tick()
        chip = self.level.get_actor_by_idx(0)
        self.renderer.camera = QRect(chip.position.x - 4, chip.position.y - 4, 9, 9)

    def show_example_level(self):
        with open("./CCLP1.dat", "rb") as set_file:
            set_bytes = set_file.read()
        levelset = parse_ccl(set_bytes)
        level_meta = levelset.get_level(2)
        level = level_meta.make_level(ms_logic)
        self.start_level(level)

    current_input: Direction = Direction.Nil

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Up:
            self.current_input = self.current_input.add_dir(Direction.North)
        elif event.key() == Qt.Key.Key_Right:
            self.current_input = self.current_input.add_dir(Direction.East)
        elif event.key() == Qt.Key.Key_Down:
            self.current_input = self.current_input.add_dir(Direction.South)
        elif event.key() == Qt.Key.Key_Left:
            self.current_input = self.current_input.add_dir(Direction.West)
        else:
            super().keyPressEvent(event)

    def keyReleaseEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Up:
            self.current_input = self.current_input.remove_dir(Direction.North)
        elif event.key() == Qt.Key.Key_Right:
            self.current_input = self.current_input.remove_dir(Direction.East)
        elif event.key() == Qt.Key.Key_Down:
            self.current_input = self.current_input.remove_dir(Direction.South)
        elif event.key() == Qt.Key.Key_Left:
            self.current_input = self.current_input.remove_dir(Direction.West)
        else:
            super().keyPressEvent(event)


if __name__ == "__main__":
    surface = QSurfaceFormat()
    surface.setSwapInterval(1)
    app = QApplication()
    win = MainWindow()
    win.show()
    win.show_example_level()
    app.exec()
