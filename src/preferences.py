from PySide6.QtGui import QImage
from libchips import Ruleset
from tileset import Tileset, TwLynxTileset, TwMsTileset


def get_tileset(ruleset: Ruleset) -> Tileset:
    return (
        TwLynxTileset(QImage("./atiles.bmp"))
        if ruleset.id == Ruleset.RulesetID.Lynx
        else TwMsTileset(QImage("./tiles.bmp"))
    )
