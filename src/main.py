#!/usr/bin/env python3
from typing import Optional
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QImage
from PySide6.QtWidgets import (
    QApplication,
    QBoxLayout,
    QLayout,
    QMainWindow,
    QVBoxLayout,
    QWidget,
)
from libchips import Level, Position, parse_ccl, ms_logic
from tileset import TwMsTileset
from renderer import LevelRenderer


class MainWindow(QMainWindow):
    renderer: LevelRenderer
    level: Optional[Level]
    main_container: QWidget
    tick_timer: QTimer

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.tick_timer = QTimer(self)
        self.tick_timer.timeout.connect(self.tick_level)

        self.main_container = QWidget(self)
        layout = QVBoxLayout(self.main_container)
        self.main_container.setLayout(layout)
        self.setCentralWidget(self.main_container)

        self.renderer = LevelRenderer(TwMsTileset(QImage("./tiles.bmp")), self)
        layout.addWidget(self.renderer)

    def start_level(self, level: Level):
        self.level = level
        self.renderer.level = level
        self.tick_timer.setTimerType(Qt.TimerType.PreciseTimer)
        self.tick_timer.start(1000 // 20)

    def tick_level(self):
        if not self.level:
            return
        self.level.tick()
        self.renderer.draw_viewport()
        print(self.level.current_tick)

    def show_example_level(self):
        with open("/home/glander/CC1Sets/CCLP1.dat", "rb") as set_file:
            set_bytes = set_file.read()
        levelset = parse_ccl(set_bytes)
        level_meta = levelset.get_level(2)
        level = level_meta.make_level(ms_logic)
        self.start_level(level)


if __name__ == "__main__":
    app = QApplication()
    win = MainWindow()
    win.show()
    win.show_example_level()
    app.exec()
