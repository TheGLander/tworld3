from enum import Enum
from typing import Optional
from PySide6.QtCore import QPoint, QSize, QTimer
from PySide6.QtGui import (
    QColor,
    QImage,
    QKeyEvent,
    QPaintEvent,
    QPainter,
    QSurfaceFormat,
    Qt,
)
from PySide6.QtWidgets import (
    QCheckBox,
    QDialog,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QSizePolicy,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)
from libchips import (
    Direction,
    GameInput,
    Level,
    LevelMetadata,
    LevelSet,
    Ruleset,
    TriRes,
    lynx_logic,
    ms_logic,
)
from tileset import Tileset, TwLynxTileset, TwMsTileset
from renderer import Inventory, LevelRenderer


class GameState(Enum):
    Preplay = 0
    Playing = 1
    Pause = 2


class ColorProgressBar(QFrame):
    value: int
    top_value: int
    bad_value: Optional[int] = None
    # public_value: Optional[int]

    good_color = QColor(32, 160, 32)
    bad_color = QColor(160, 32, 32)

    # public_color = QColor(200, 173, 40)
    def minimumSizeHint(self) -> QSize:
        return QSize(20, 20)

    def paintEvent(self, arg__1: QPaintEvent) -> None:
        painter = QPainter(self)
        self.drawFrame(painter)
        palette = self.palette()
        size = self.size()

        if self.top_value == 0:
            top_fill = 1.0
            bad_fill = 0.0
        else:
            top_fill = 1.0 if self.top_value == 0 else self.value / self.top_value
            # public_fill = min(self.value, self.public_value or 0) / self.top_value
            bad_fill = min(self.value, self.bad_value or 0) / self.top_value

        good_color = palette.link() if self.bad_value == None else self.good_color
        painter.fillRect(0, 0, size.width(), size.height(), palette.window())
        painter.fillRect(0, 0, int(top_fill * size.width()), size.height(), good_color)
        # painter.fillRect(0, 0, int(public_fill * size.width()), size.height(), self.public_color)
        painter.fillRect(
            0, 0, int(bad_fill * size.width()), size.height(), self.bad_color
        )
        painter.setPen(palette.text().color())
        painter.drawText(
            painter.window(), Qt.AlignmentFlag.AlignCenter, f"{self.value}"
        )
        painter.end()


class DebugWindow(QDialog):
    player: "LevelPlayer"

    def __init__(self, parent: "LevelPlayer") -> None:
        super().__init__(parent)
        self.player = parent
        self.setModal(False)
        self.setWindowTitle("Debug Tools")
        layout = QGridLayout(self)

        def toggle_timer(start: bool):
            if not start:
                self.player.tick_timer.stop()
            else:
                self.player.tick_timer.start()

        autotick_checkbox = QCheckBox("Auto-tick", self)
        autotick_checkbox.setChecked(True)
        autotick_checkbox.clicked.connect(toggle_timer)
        layout.addWidget(autotick_checkbox, 0, 0)

        tps_input = QSpinBox(self)
        tps_input.setValue(20)
        tps_input.setMinimum(1)
        tps_input.valueChanged.connect(
            lambda val: self.player.tick_timer.setInterval(1000 // val)
        )
        tps_label = QLabel("TpS:", self)
        tps_label.setBuddy(tps_input)
        layout.addWidget(tps_label, 0, 1)
        layout.addWidget(tps_input, 0, 2)

        step_button = QPushButton("Step tick", self)
        step_button.clicked.connect(lambda: self.player.tick_level())
        layout.addWidget(step_button, 1, 0)


# TODO: This a config-adjacent function, move it to a preference.py or something
def get_tileset(ruleset: Ruleset) -> Tileset:
    return (
        TwLynxTileset(QImage("./atiles.bmp"))
        if ruleset.id == Ruleset.RulesetID.Lynx
        else TwMsTileset(QImage("./tiles.bmp"))
    )


class LevelPlayer(QWidget):
    level_set: LevelSet
    cur_level_index: int
    level: Optional[Level] = None
    game_state: GameState = GameState.Preplay
    ruleset: Ruleset

    # Bottom bar
    # level_num_w: QLabel
    level_pack_w: QLabel
    level_name_w: QLabel
    level_author_w: QLabel
    game_state_w: QLabel

    # Right boxes
    level_password_w: QLabel
    chips_left_w: QLabel
    time_left_w: ColorProgressBar

    inventory_w: Inventory
    hint_w: QLabel

    # Inputs
    held_direction_keys = 0
    mouse_input = GameInput.none()

    tick_timer: QTimer
    tileset: Tileset
    renderer_w: LevelRenderer

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.tick_timer = QTimer(self)
        self.tick_timer.setTimerType(Qt.TimerType.PreciseTimer)
        self.tick_timer.setInterval(1000 // 20)
        self.tick_timer.timeout.connect(self.tick_level)

        layout = QGridLayout(self)
        layout.setRowStretch(0, 1)
        layout.setRowStretch(2, 0)
        layout.setColumnStretch(0, 1)
        layout.setColumnStretch(1, 0)

        text_info_layout = QHBoxLayout()
        layout.addLayout(text_info_layout, 2, 0, 1, 2)

        self.level_pack_w = QLabel(self)
        self.level_pack_w.setAlignment(Qt.AlignmentFlag.AlignCenter)
        text_info_layout.addWidget(self.level_pack_w)

        self.level_name_w = QLabel(self)
        self.level_name_w.setAlignment(Qt.AlignmentFlag.AlignCenter)
        text_info_layout.addWidget(self.level_name_w)

        self.level_author_w = QLabel(self)
        self.level_author_w.setAlignment(Qt.AlignmentFlag.AlignCenter)
        text_info_layout.addWidget(self.level_author_w)

        self.game_state_w = QLabel(self)
        self.game_state_w.setAlignment(Qt.AlignmentFlag.AlignCenter)
        text_info_layout.addWidget(self.game_state_w)

        sidebar_layout = QVBoxLayout()
        layout.addLayout(sidebar_layout, 0, 1, 2, 1)

        stats_box = QFrame(self)
        stats_box.setFrameStyle(QFrame.Shadow.Raised | QFrame.Shape.Panel)
        stats_layout = QGridLayout(stats_box)
        stats_layout.setColumnMinimumWidth(1, 0)
        stats_layout.setColumnStretch(2, 1)

        def Label(text: str):
            label = QLabel(text, stats_box)
            font = label.font()
            font.setPointSize(10)
            label.setFont(font)
            return label

        stats_layout.addWidget(Label("Password"), 0, 0)
        self.level_password_w = QLabel("ABCD", stats_box)
        self.level_password_w.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.level_password_w.setMargin(2)
        self.level_password_w.setFrameStyle(QFrame.Shadow.Sunken | QFrame.Shape.Panel)
        stats_layout.addWidget(self.level_password_w, 0, 2)

        stats_layout.addWidget(Label("Chips left"), 2, 0)
        self.chips_left_w = QLabel("0", stats_box)
        self.chips_left_w.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        self.chips_left_w.setMargin(2)
        self.chips_left_w.setFrameStyle(QFrame.Shadow.Sunken | QFrame.Shape.Panel)
        stats_layout.addWidget(self.chips_left_w, 2, 2)

        self.time_left_w = ColorProgressBar(stats_box)
        stats_layout.addWidget(self.time_left_w, 4, 0, 1, 3)

        sidebar_layout.addWidget(stats_box, 0)

        inventory_box = QFrame(self)
        inventory_box.setFrameStyle(QFrame.Shadow.Raised | QFrame.Shape.Panel)
        inventory_box.setContentsMargins(4, 4, 4, 4)
        inventory_layout = QVBoxLayout(inventory_box)

        sidebar_layout.addWidget(inventory_box, 0)

        self.inventory_w = Inventory(self)
        inventory_layout.addWidget(self.inventory_w)

        hint_box = QFrame(self)
        hint_box.setFrameStyle(QFrame.Shadow.Raised | QFrame.Shape.Panel)
        hint_box.setContentsMargins(4, 4, 4, 4)
        hint_box_layout = QVBoxLayout(hint_box)
        sidebar_layout.addWidget(hint_box, 1)

        self.hint_w = QLabel(hint_box)
        self.hint_w.setTextFormat(Qt.TextFormat.PlainText)
        self.hint_w.setWordWrap(True)
        self.hint_w.setMinimumSize(0, 0)
        self.hint_w.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft)
        self.hint_w.setSizePolicy(
            QSizePolicy.Policy.Ignored, QSizePolicy.Policy.Ignored
        )
        self.hint_w.setMargin(4)
        self.hint_w.setFrameStyle(QFrame.Shadow.Sunken | QFrame.Shape.Panel)
        hint_box_layout.addWidget(self.hint_w)

        self.renderer_w = LevelRenderer(self)
        self.renderer_w.mouse_press.connect(self.set_mouse_input)

        layout.addWidget(self.renderer_w, 0, 0, alignment=Qt.AlignmentFlag.AlignCenter)

    def set_mouse_input(self, input: GameInput):
        self.mouse_input = input

    def get_cur_level(self) -> Optional[LevelMetadata]:
        return self.level_set.get_level(self.cur_level_index)

    def previous_level(self):
        if self.cur_level_index == 0:
            return
        self.cur_level_index -= 1
        self.restart_level()
    
    def next_level(self):
        if self.cur_level_index == self.level_set.levels_n - 1:
            return
        self.cur_level_index += 1
        new_level = self.get_cur_level()
        assert new_level != None
        self.restart_level()


    def reload_tileset(self):
        self.tileset = get_tileset(self.ruleset)
        self.renderer_w.tileset = self.tileset
        self.renderer_w.rescale()
        self.inventory_w.tileset = self.tileset
        self.inventory_w.rescale()

    def open_set(self, set: LevelSet):
        self.level_set = set
        # TODO: Load NCCS
        # TODO: CCX Intermissions
        self.cur_level_index = 0
        self.reload_tileset()
        self.restart_level()

    def restart_level(self):
        self.set_game_state(GameState.Preplay)
        level_meta = self.get_cur_level()
        if not level_meta:
            return
        self.open_level(level_meta.make_level(self.ruleset))

    def set_game_state(self, state: GameState):
        if state == GameState.Playing:
            self.tick_timer.start()
            self.renderer_w.auto_draw = True
        else:
            self.tick_timer.stop()
            self.renderer_w.auto_draw = False
        if state == GameState.Pause:
            self.game_state_w.setText("(paused)")
        else:
            self.game_state_w.setText("")
        self.game_state = state
        self.renderer_w.repaint()
        self.update_game_text()


    def toggle_paused(self):
        if self.game_state == GameState.Playing:
            self.set_game_state(GameState.Pause)
        elif self.game_state == GameState.Pause:
            self.set_game_state(GameState.Playing)

    def update_game_text(self):
        if not self.level or not self.level.metadata:
            return
        self.chips_left_w.setText(f"{self.level.chips_left}")
        # TODO: [999] time
        if self.level.time_limit == 0:
            self.time_left_w.value = 0
        else:
            self.time_left_w.value = (
                self.level.time_limit - self.level.current_tick + 19
            ) // 20
        self.time_left_w.repaint()
        self.inventory_w.repaint()

        hint = self.level.metadata.hint
        if hint and self.level.status_flags & Level.StatusFlags.ShowHint:
            self.hint_w.setText(hint)
        else:
            self.hint_w.setText("")

    def open_level(self, level: Level):
        self.level = level
        self.renderer_w.level = level
        self.inventory_w.level = level
        level_meta = level.metadata
        self.level_password_w.setText(level_meta.password if level_meta else "")
        self.level_name_w.setText(
            level_meta.title if level_meta and level_meta.title else "???"
        )

        self.level_pack_w.setText(
            f"{self.level_set.name} #{level.metadata.level_number}"
        )

        if level_meta and level_meta.author:
            self.level_author_w.setText(f"By: {level_meta.author}")
        else:
            self.level_author_w.setText("")

        self.time_left_w.top_value = level.time_limit // 20
        self.set_game_state(GameState.Preplay)

    def start_level(self):
        if self.game_state != GameState.Preplay:
            return
        self.set_game_state(GameState.Playing)

    def tick_level(self):
        if self.game_state != GameState.Playing:
            return
        if not self.level or self.level.win_state != TriRes.Nothing:
            return
        self.level.game_input = self.get_input()
        self.mouse_input = GameInput.none()
        self.level.tick()
        self.renderer_w.level_updated()
        self.update_game_text()
        if self.level.win_state == TriRes.Success:
            QMessageBox.information(self, "You won!", "ayy!")
        elif self.level.win_state == TriRes.Died:
            QMessageBox.warning(self, "You died", "bummer")

    def toggle_ruleset(self):
        self.ruleset = lynx_logic if self.ruleset == ms_logic else ms_logic
        self.renderer_w.clickable = self.ruleset == ms_logic
        self.reload_tileset()
        self.restart_level()

    def open_debug_tools(self):
        dialog = DebugWindow(self)
        dialog.show()

    def keyPressEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Up:
            if self.game_state == GameState.Preplay:
                self.start_level()
            self.held_direction_keys |= 1
        elif event.key() == Qt.Key.Key_Left:
            if self.game_state == GameState.Preplay:
                self.start_level()
            self.held_direction_keys |= 2
        elif event.key() == Qt.Key.Key_Down:
            if self.game_state == GameState.Preplay:
                self.start_level()
            self.held_direction_keys |= 4
        elif event.key() == Qt.Key.Key_Right:
            if self.game_state == GameState.Preplay:
                self.start_level()
            self.held_direction_keys |= 8
        else:
            super().keyPressEvent(event)

    def keyReleaseEvent(self, event: QKeyEvent) -> None:
        if event.key() == Qt.Key.Key_Up:
            self.held_direction_keys &= ~1
        elif event.key() == Qt.Key.Key_Left:
            self.held_direction_keys &= ~2
        elif event.key() == Qt.Key.Key_Down:
            self.held_direction_keys &= ~4
        elif event.key() == Qt.Key.Key_Right:
            self.held_direction_keys &= ~8
        else:
            super().keyPressEvent(event)

    def get_input(self) -> GameInput:
        if self.mouse_input:
            return self.mouse_input
        if self.held_direction_keys.bit_count() in (
            1,
            2,
        ) and self.held_direction_keys not in (0b0101, 0b1010):
            return GameInput(self.held_direction_keys)
        return GameInput.none()
