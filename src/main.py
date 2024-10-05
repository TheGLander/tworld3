#!/usr/bin/env python3
from typing import Tuple
from PySide6.QtCore import QMimeData, QUrl, Slot
from PySide6.QtGui import QAction, QCursor, QDragEnterEvent, QDropEvent, QPixmap, Qt
from PySide6.QtWidgets import (
    QApplication,
    QFileDialog,
    QMainWindow,
    QStackedWidget,
    QWidget,
)

from pathlib import Path

from level_player import LevelPlayer
from libchips import LevelSet, parse_ccl, lynx_logic, ms_logic
from set_selector import SelectableRuleset, SetSelector


def path_from_mime_data(mime_data: QMimeData) -> Path:
    return Path(mime_data.urls()[0].path())


class MainWindow(QMainWindow):
    page_selector: QStackedWidget
    level_player: LevelPlayer
    set_selector: SetSelector

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Tile World 3")
        self.page_selector = QStackedWidget(self)
        self.setCentralWidget(self.page_selector)

        self.level_player = LevelPlayer(self.page_selector)
        self.page_selector.addWidget(self.level_player)

        self.set_selector = SetSelector(self.page_selector)
        self.set_selector.set_opened.connect(self.open_selected_set)
        self.page_selector.addWidget(self.set_selector)

        menu_bar = self.menuBar()

        def Action(label, shortcut, slot):
            action = QAction(label, self)
            action.setShortcut(shortcut)
            action.triggered.connect(slot)
            return action

        file_menu = menu_bar.addMenu("&Set")
        file_menu.addActions(
            [
                Action(
                    "&Open level set...",
                    Qt.Modifier.CTRL | Qt.Key.Key_O,
                    self.open_set_file,
                ),
                Action("Open &set selector", Qt.Key.Key_Escape, self.open_set_selector),
            ]
        )

        level_menu = menu_bar.addMenu("&Level")

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
                Action(
                    "&Next level",
                    Qt.Modifier.CTRL | Qt.Key.Key_N,
                    self.level_player.next_level,
                ),
                # Action("Level list", Qt.Key.Key_Backspace, self.level_player.toggle_paused),
            ]
        )
        level_menu.addSeparator()
        level_menu.addActions(
            [
                Action(
                    "Open debug tools",
                    Qt.Modifier.ALT | Qt.Key.Key_D,
                    self.level_player.open_debug_tools,
                ),
            ]
        )

        self.setAcceptDrops(True)

        self.level_player.ruleset = lynx_logic
        self.page_selector.setCurrentWidget(self.set_selector)

    def open_page(self, page: QWidget):
        old_page = self.page_selector.currentWidget()
        assert getattr(old_page, "close_page") != None
        old_page.close_page()  # type: ignore
        self.page_selector.setCurrentWidget(page)

    def open_selected_set(self, set_info: Tuple[LevelSet, SelectableRuleset]):
        set, ruleset = set_info
        self.level_player.ruleset = ruleset.lib_ruleset
        self.open_set(set)

    def open_set(self, set: LevelSet):
        self.level_player.open_set(set)
        self.open_page(self.level_player)

    def open_set_selector(self):
        self.open_page(self.set_selector)

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
    app.setApplicationName("tworld3")
    app.setApplicationDisplayName("Tile World 3")
    app.setDesktopFileName("TWorld3")
    icon = QPixmap("./icon.png")
    app.setWindowIcon(icon)
    win = MainWindow()
    win.show()
    app.exec()
