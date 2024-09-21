#!/usr/bin/env python3
from PySide6.QtGui import QImage
from PySide6.QtWidgets import (
    QApplication,
    QBoxLayout,
    QLayout,
    QMainWindow,
    QVBoxLayout,
    QWidget,
)
from libchips import Position, parse_ccl, lynx_logic
from tileset import TwMsTileset
from renderer import LevelRenderer


class MainWindow(QMainWindow):
    renderer: LevelRenderer
    main_container: QWidget

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.main_container = QWidget(self)
        layout = QVBoxLayout(self.main_container)
        self.main_container.setLayout(layout)
        # self.tileset =
        self.setCentralWidget(self.main_container)

    def show_example_level(self):
        with open("/home/glander/CC1Sets/CCLP1.dat", "rb") as set_file:
            set_bytes = set_file.read()
        levelset = parse_ccl(set_bytes)
        level_meta = levelset.get_level(0)
        level = level_meta.make_level(lynx_logic)

        self.renderer = LevelRenderer(level, TwMsTileset(QImage("./tiles.bmp")), self)
        self.main_container.layout().addWidget(self.renderer)


if __name__ == "__main__":
    app = QApplication()
    win = MainWindow()
    win.show()
    win.show_example_level()
    app.exec()
