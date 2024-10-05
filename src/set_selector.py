from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterator, Optional, Sequence, Tuple, Union
from PySide6.QtCore import (
    QAbstractListModel,
    QModelIndex,
    QPersistentModelIndex,
    QStandardPaths,
    Signal,
)
from PySide6.QtGui import Qt
from PySide6.QtWidgets import (
    QComboBox,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QListView,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QRadioButton,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from libchips import LevelSet, Ruleset, parse_ccl, lynx_logic, ms_logic
from renderer import LevelRenderer
from preferences import get_tileset


def find_local_sets() -> Iterator[Path]:
    # TODO: Disambiguate multiple sets with the same name

    # Add in our local CCLP1
    cwd = Path(__file__).parent.parent
    if (cwd / "CCLP1.dat").exists():
        yield cwd / "CCLP1.dat"

    for location in QStandardPaths.standardLocations(
        QStandardPaths.StandardLocation.AppDataLocation
    ):
        sets_dir = Path(location) / "sets"
        if not sets_dir.exists() or not sets_dir.is_dir():
            continue
        yield from (
            file
            for file in sets_dir.iterdir()
            if file.suffix.lower() in (".dat", ".ccl")
        )


@dataclass
class LocalSet:
    path: Path
    set: Optional[LevelSet] = None

    def load_set(self) -> LevelSet:
        if self.set:
            return self.set
        self.set = parse_ccl(self.path.read_bytes())
        self.set.name = self.path.stem
        return self.set


SET_ROLE = 0xCCBBC00


class LocalSetsModel(QAbstractListModel):
    sets: Sequence[LocalSet]

    def data(self, index: Union[QModelIndex, QPersistentModelIndex], role: int = 0):
        row = index.row()
        if row >= len(self.sets):
            return None
        level_set = self.sets[row]
        if role == Qt.ItemDataRole.DisplayRole:
            return level_set.path.stem
        elif role == SET_ROLE:
            return level_set
        else:
            return None

    def rowCount(
        self, parent: Union[QModelIndex, QPersistentModelIndex] = QModelIndex()
    ) -> int:
        return len(self.sets)


@dataclass
class SelectableRuleset:
    display_name: str
    solution_suffix: str
    lib_ruleset: Ruleset

    def __hash__(self) -> int:
        return hash(self.display_name)


rulesets: Sequence[SelectableRuleset] = [
    SelectableRuleset("Lynx", "lynx", lynx_logic),
    SelectableRuleset("MS", "ms", ms_logic),
]


class SetSelector(QWidget):
    set_opened = Signal(Any)
    local_sets_w: QListView
    local_sets_model: LocalSetsModel

    set_thumbnail_w: LevelRenderer
    set_name_w: QLabel
    selected_ruleset: SelectableRuleset

    profile_dropdown_w: QComboBox
    ruleset_radios: Dict[SelectableRuleset, Tuple[QRadioButton]]

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.ruleset_radios = {}
        layout = QGridLayout(self)
        layout.setColumnStretch(0, 1)
        layout.setColumnStretch(1, 1)
        layout.addWidget(QLabel("Sets:", self), 0, 0)
        self.local_sets_w = QListView(self)
        self.local_sets_model = LocalSetsModel(self)
        self.local_sets_w.setModel(self.local_sets_model)
        self.local_sets_w.selectionModel().currentChanged.connect(
            self.set_item_selected
        )
        layout.addWidget(self.local_sets_w, 1, 0)
        self.local_sets_w.activated.connect(self.set_item_picked)
        self.build_set_list()

        rlayout = QVBoxLayout()
        layout.addLayout(rlayout, 0, 1, 2, 1)

        self.set_thumbnail_w = LevelRenderer(self)
        self.set_thumbnail_w.do_interpolation = False
        rlayout.addWidget(self.set_thumbnail_w, alignment=Qt.AlignmentFlag.AlignHCenter)

        self.set_name_w = QLabel(self)
        set_name_font = self.set_name_w.font()
        set_name_font.setPixelSize(20)
        set_name_font.setBold(True)
        self.set_name_w.setFont(set_name_font)
        rlayout.addWidget(self.set_name_w)

        rlayout.addStretch(1)

        profile_layout = QHBoxLayout()
        rlayout.addLayout(profile_layout)

        profile_layout.addWidget(QLabel("Profile:", self))

        self.profile_dropdown_w = QComboBox(self)
        self.profile_dropdown_w.addItems(["default"])
        profile_layout.addWidget(self.profile_dropdown_w)

        rulesets_box = QWidget(self)
        ruleset_layout = QGridLayout(rulesets_box)

        # Need this to rebind a custom `ruleset` for each lambda
        def make_select_ruleset(l_rset):
            return lambda: self.select_ruleset(l_rset)

        for idx, ruleset in enumerate(rulesets):
            radio = QRadioButton(ruleset.display_name, rulesets_box)
            radio.clicked.connect(make_select_ruleset(ruleset))
            ruleset_layout.addWidget(radio, idx, 0)
            self.ruleset_radios[ruleset] = (radio,)
            ruleset_stats = QLabel(rulesets_box)
            # TODO: Populate this
            ruleset_stats.setText(f"Level 1 | Beaten 0/149 | Score: 6,123,456")
            ruleset_layout.addWidget(ruleset_stats, idx, 1)

            if ruleset.solution_suffix == "lynx":
                radio.setChecked(True)
                self.selected_ruleset = ruleset

        rlayout.addWidget(rulesets_box)

        button_layout = QHBoxLayout()
        rlayout.addLayout(button_layout)
        start_button = QPushButton("Start", self)
        start_button.clicked.connect(
            lambda: self.set_item_picked(self.get_current_index())
        )
        button_layout.addWidget(start_button, 5)

    def get_current_index(self):
        return self.local_sets_w.selectionModel().currentIndex()

    def select_ruleset(self, ruleset: SelectableRuleset):
        self.selected_ruleset = ruleset
        self.build_thumbnail()

    def build_thumbnail(self):
        set_local: LocalSet = self.get_current_index().data(SET_ROLE)
        self.set_thumbnail_w.tileset = get_tileset(self.selected_ruleset.lib_ruleset)
        level_set = set_local.load_set()
        self.set_thumbnail_w.level = level_set.get_level(0).make_level(
            self.selected_ruleset.lib_ruleset
        )
        self.set_thumbnail_w.rescale()
        self.set_thumbnail_w.repaint()
        self.set_name_w.setText(level_set.name or "[UNNAMED]")
        self.set_name_w.setAlignment(
            Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignHCenter
        )

    def set_item_selected(self, index: QModelIndex):
        self.build_thumbnail()

    def build_set_list(self):
        self.local_sets_model.layoutAboutToBeChanged.emit()
        self.local_sets_model.sets = [LocalSet(path) for path in find_local_sets()]
        self.local_sets_model.layoutChanged.emit()

    def set_item_picked(self, index: QModelIndex):
        level_set: LocalSet = index.data(SET_ROLE)
        self.set_opened.emit((level_set.load_set(), self.selected_ruleset))

    def close_page(self):
        pass
