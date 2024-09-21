#!/usr/bin/env -S python3
from libchips import parse_ccl

if __name__ == "__main__":
    with open("/home/glander/CC1Sets/CCLP1.dat", "rb") as set_file:
        set_bytes = set_file.read()
    levelset = parse_ccl(set_bytes)
    for level in levelset.levels:
        print(level.title)
