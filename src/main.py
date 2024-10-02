#!/usr/bin/env python3
from PySide6.QtCore import QMimeData, QUrl
from PySide6.QtGui import QAction, QCursor, QDragEnterEvent, QDropEvent, Qt
from PySide6.QtWidgets import QApplication, QFileDialog, QMainWindow, QStackedWidget

from pathlib import Path

from level_player import LevelPlayer
from libchips import LevelSet, parse_ccl, lynx_logic, ms_logic


def path_from_mime_data(mime_data: QMimeData) -> Path:
    return Path(mime_data.urls()[0].path())


class MainWindow(QMainWindow):
    page_selector: QStackedWidget
    level_player: LevelPlayer

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Tile World 3")
        self.page_selector = QStackedWidget(self)
        self.setCentralWidget(self.page_selector)

        self.level_player = LevelPlayer(self.page_selector)
        self.page_selector.addWidget(self.level_player)

        menu_bar = self.menuBar()

        def Action(label, shortcut, slot):
            action = QAction(label, self)
            action.setShortcut(shortcut)
            action.triggered.connect(slot)
            return action

        file_menu = menu_bar.addMenu("&File")
        file_menu.addActions(
            [
                Action(
                    "&Open level set...",
                    Qt.Modifier.CTRL | Qt.Key.Key_O,
                    self.open_set_file,
                ),
            ]
        )

        level_menu = menu_bar.addMenu("Level")

        level_menu.addActions(
            [
                Action("&Start", Qt.Key.Key_Space, self.level_player.start_level),
                Action(
                    "&Restart",
                    Qt.Modifier.CTRL | Qt.Key.Key_R,
                    self.level_player.restart_level,
                ),
                Action("&Pause", Qt.Key.Key_Backspace, self.level_player.toggle_paused),
            ]
        )
        level_menu.addSeparator()
        level_menu.addActions(
            [
                Action(
                    "P&revious level",
                    Qt.Modifier.CTRL | Qt.Key.Key_P,
                    self.level_player.previous_level,
                ),
                Action("&Next level", Qt.Modifier.CTRL | Qt.Key.Key_N, self.level_player.next_level),
                # Action("Level list", Qt.Key.Key_Backspace, self.level_player.toggle_paused),
            ]
        )
        level_menu.addSeparator()
        level_menu.addActions(
            [
                Action(
                    "Toggle r&uleset",
                    Qt.Modifier.ALT | Qt.Key.Key_R,
                    self.level_player.toggle_ruleset,
                ),
                Action(
                    "Open debug tools",
                    Qt.Modifier.ALT | Qt.Key.Key_D,
                    self.level_player.open_debug_tools,
                ),
            ]
        )

        self.setAcceptDrops(True)

        self.level_player.ruleset = lynx_logic

    def open_set(self, set: LevelSet):
        self.page_selector.setCurrentWidget(self.level_player)
        self.level_player.open_set(set)

    def open_set_file(self):
        file, chosen_filter = QFileDialog.getOpenFileName(
            self, "Open Set", filter="Level Set (*.dat *.ccl)"
        )
        if file == "":
            return
        path = Path(file)
        set = parse_ccl(path.read_bytes())
        set.name = path.stem
        self.open_set(set)

    recognized_extensions = (".ccl", ".dat")

    def dragEnterEvent(self, event: QDragEnterEvent) -> None:
        mime_data = event.mimeData()
        if len(mime_data.urls()) != 1:
            return

        path = path_from_mime_data(mime_data)
        if path.suffix in self.recognized_extensions:
            event.acceptProposedAction()

    def dropEvent(self, event: QDropEvent) -> None:
        path = path_from_mime_data(event.mimeData())

        if path.suffix in (".ccl", ".dat"):
            # TODO: Add loaded set to local sets dir
            set = parse_ccl(path.read_bytes())
            set.name = path.stem
            self.open_set(set)


if __name__ == "__main__":
    app = QApplication()
    win = MainWindow()
    win.show()
    with open("./CCLP1.dat", "rb") as set_file:
        set_bytes = set_file.read()
    levelset = parse_ccl(set_bytes)
    levelset.name = "CCLP1"
    win.open_set(levelset)
    app.exec()
