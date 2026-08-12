#!/usr/bin/env python3
"""Bulwark - the app icon.

A bulwark is a wall built to be stood behind, so the icon is a battlement:
three merlons over coursed stone. Ten by ten, one bit deep, which is what the
launcher wants, and legible at that size because it is drawn as pixels rather
than shrunk from something bigger.

    python3 tools_gen_icons.py
"""

from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ICONS = HERE / "icons"

# '#' is ink. Read it as a wall seen head on: crenellations along the top,
# then courses of stone with the joints staggered.
BULWARK_10 = [
    "##.##.##.#",
    "##.##.##.#",
    "##########",
    "###.###.##",
    "##########",
    "##########",
    "#.###.###.",
    "##########",
    "##########",
    "###.###.##",
]


def write_bitmap(rows, path):
    w = len(rows[0])
    h = len(rows)
    assert all(len(r) == w for r in rows), f"{path}: ragged art"

    img = Image.new("1", (w, h), 1)  # 1 = white
    px = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            px[x, y] = 0 if ch == "#" else 1

    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)
    print(f"wrote {path.relative_to(HERE)}  ({w}x{h})")


def main():
    write_bitmap(BULWARK_10, ICONS / "bulwark_10px.png")


if __name__ == "__main__":
    main()
