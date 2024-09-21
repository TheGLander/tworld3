#!/usr/bin/env -S python3
from libchips import Position, parse_ccl, lynx_logic

if __name__ == "__main__":
    with open("/home/glander/CC1Sets/CCLP1.dat", "rb") as set_file:
        set_bytes = set_file.read()
    levelset = parse_ccl(set_bytes)
    level_meta = levelset.get_level(0)
    print(level_meta.title)
    level = level_meta.make_level(lynx_logic)
    print(level.get_top_terrain(Position(14, 19)))
