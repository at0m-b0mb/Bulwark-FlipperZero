#!/usr/bin/env python3
"""Bulwark - README screenshots, drawn the way the firmware draws them.

Every layout constant below is copied from the view it mirrors, and every
number on the screens comes out of test/demo_dump.json, which is written by
the real engine compiled for the host. So these are not artists' impressions:
if a label collides with a value here, it collides on the device too, and if
a score is wrong here the tests are wrong as well.

    make -C test dump
    python3 tools_gen_mockups.py
"""

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
IMAGES = HERE / "images"
DUMP = HERE / "test" / "demo_dump.json"

W, H = 128, 64
S = 6  # one Flipper pixel, in image pixels

# The Flipper Zero's screen is a monochrome LCD behind an orange backlight:
# warm orange where nothing is drawn, near-black where a pixel is on.
PAPER = (255, 130, 0)
INK = (10, 8, 4)

# --- constants mirrored from views/ -----------------------------------------

WALL_BAR_W = 10  # views/wall_art.h
WALL_SPAN_X = 4  # views/wall_art.c
WALL_SPAN_W = 104
WALL_MHZ_LO = 2402
WALL_MHZ_HI = 2480

WV_HDR_H = 12  # views/watch_view.c
WV_CREN_Y = 12
WV_CREN_H = 3
WV_BAR_BASE = 47
WV_BAR_H = 30
WV_LABEL_BASE = 55
WV_STATUS_BASE = 63
WV_GRAPH_X = 5

DV_VISIBLE = 5  # views/detail_view.c
DV_ROW_H = 9
DV_FIRST_BASE = 23

LV_ART_Y = 16  # views/learn_view.c
LV_TEXT = (45, 53, 61)

SPL_WALL_Y = 29  # views/splash_view.c
SPL_WALL_H = 12


def load_font(names, size):
    for name in names:
        for path in (
            f"/System/Library/Fonts/{name}",
            f"/System/Library/Fonts/Supplemental/{name}",
            f"/Library/Fonts/{name}",
            f"/usr/share/fonts/truetype/dejavu/{name}",
        ):
            if Path(path).exists():
                try:
                    return ImageFont.truetype(path, size)
                except OSError:
                    continue
    return ImageFont.load_default()


def fit_font(names, advance_px):
    """Pick the point size whose advance matches the device font's.

    The whole point of these mockups is that a line which fits here fits on
    the Flipper, so the substitute face has to be at least as wide as the one
    it stands in for. FontSecondary advances 5 pixels per character and
    FontPrimary about 7; a face chosen by eye is usually narrower than that,
    which would quietly hide real collisions.
    """
    best, best_err = None, None
    for size in range(12 * S, 3 * S, -1):
        font = load_font(names, size)
        got = font.getlength("MMMMMMMMMM") / 10 / S
        err = abs(got - advance_px)
        if best_err is None or err < best_err:
            best, best_err = font, err
    return best


FONT_SECONDARY = fit_font(["Andale Mono.ttf", "DejaVuSansMono.ttf"], 5.0)
FONT_PRIMARY = fit_font(["Arial Bold.ttf", "DejaVuSans-Bold.ttf"], 7.0)


class Screen:
    """The Flipper's screen, with the drawing calls the firmware has.

    Everything is drawn straight at 6x, so the graphics land on the pixel grid
    the way the LCD would show them while the text keeps its real shapes -
    which is how the device actually looks, rather than a blurred upscale.
    """

    def __init__(self):
        self.img = Image.new("RGB", (W * S, H * S), PAPER)
        self.d = ImageDraw.Draw(self.img)
        self.color = INK

    def set_color(self, color):
        self.color = color

    def box(self, x, y, w, h):
        if w <= 0 or h <= 0:
            return
        self.d.rectangle(
            [x * S, y * S, (x + w) * S - 1, (y + h) * S - 1], fill=self.color
        )

    def frame(self, x, y, w, h):
        self.box(x, y, w, 1)
        self.box(x, y + h - 1, w, 1)
        self.box(x, y, 1, h)
        self.box(x + w - 1, y, 1, h)

    def line(self, x0, y0, x1, y1):
        """Bresenham, one Flipper pixel at a time, like canvas_draw_line."""
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.dot(x0, y0)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def dot(self, x, y):
        self.d.rectangle(
            [x * S, y * S, (x + 1) * S - 1, (y + 1) * S - 1], fill=self.color
        )

    def str(self, x, baseline, text, font=FONT_SECONDARY):
        self.d.text(
            (x * S, baseline * S), text, font=font, fill=self.color, anchor="ls"
        )

    def str_right(self, x, baseline, text, font=FONT_SECONDARY):
        self.d.text(
            (x * S, baseline * S), text, font=font, fill=self.color, anchor="rs"
        )

    def str_center(self, x, baseline, text, font=FONT_SECONDARY):
        self.d.text(
            (x * S, baseline * S), text, font=font, fill=self.color, anchor="ms"
        )

    def save(self, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.img.save(path)
        print(f"wrote {path.relative_to(HERE)}")


# --- shared drawing ---------------------------------------------------------


def chan_x(mhz):
    return WALL_SPAN_X + ((mhz - WALL_MHZ_LO) * WALL_SPAN_W) // (WALL_MHZ_HI - WALL_MHZ_LO)


def crenellations(s, x, y, w, h):
    for i in range(x, x + w, 8):
        s.box(i, y, min(4, x + w - i), h)


def bar_h(busy_ppt, height):
    """wall_art_bar_h(), pixel for pixel."""
    busy_ppt = min(busy_ppt, 1000)
    low = height * 40 // 100
    mid = height * 35 // 100
    top = height - low - mid
    if busy_ppt <= 100:
        h = busy_ppt * low // 100
    elif busy_ppt <= 300:
        h = low + (busy_ppt - 100) * mid // 200
    else:
        h = low + mid + (busy_ppt - 300) * top // 700
    return min(h, height)


def spectrum(s, chans, base_y, height, marked=None):
    for c in chans:
        x = chan_x(c["mhz"])
        h = bar_h(c["busy_ppt"], height)
        if c["adv"]:
            if h > 0:
                s.box(x, base_y - h, WALL_BAR_W, h)
                s.set_color(PAPER)
                s.dot(x + 3, base_y - h)
                s.dot(x + 6, base_y - h)
                s.set_color(INK)
            else:
                s.line(x, base_y, x + WALL_BAR_W - 1, base_y)
        else:
            for y in range(base_y - h, base_y, 2):
                s.line(x, y, x + WALL_BAR_W - 1, y)
            s.line(x, base_y, x + WALL_BAR_W - 1, base_y)


def watch_header(s, title, right):
    s.box(0, 0, W, WV_HDR_H)
    crenellations(s, 0, WV_CREN_Y, W, WV_CREN_H)
    s.set_color(PAPER)
    s.str(2, 10, title, FONT_PRIMARY)
    if right:
        s.str_right(126, 10, right, FONT_PRIMARY)
    s.set_color(INK)


def watch_status(s, left, right):
    s.str(2, WV_STATUS_BASE, left)
    s.str_right(126, WV_STATUS_BASE, right)


# --- the screens ------------------------------------------------------------


def screen_wall(scene, status_right, marked="38", baseline_ppt=None):
    s = Screen()
    watch_header(s, scene["verdict"], str(scene["score"]))
    if baseline_ppt is not None:
        y = WV_BAR_BASE - bar_h(baseline_ppt, WV_BAR_H)
        for x in range(0, 128, 3):
            s.dot(x, y)
    spectrum(s, scene["chan"], WV_BAR_BASE, WV_BAR_H)
    s.line(0, WV_BAR_BASE + 1, 127, WV_BAR_BASE + 1)
    for c in scene["chan"]:
        x = chan_x(c["mhz"])
        if c["label"] == marked:
            s.box(x - 1, WV_LABEL_BASE - 7, len(c["label"]) * 5 + 2, 8)
            s.set_color(PAPER)
            s.str(x, WV_LABEL_BASE, c["label"])
            s.set_color(INK)
            continue
        s.str(x, WV_LABEL_BASE, c["label"])
    watch_status(s, f"6s  #{scene['streak']}  {scene['rate_hz']}Hz", status_right)
    return s


def screen_trend(scene, history):
    s = Screen()
    watch_header(s, scene["verdict"], str(scene["score"]))
    base = WV_BAR_BASE
    for mark in (25, 45, 70):
        y = base - (mark * WV_BAR_H) // 100
        for x in range(0, W, 4):
            s.dot(x, y)
    for i, v in enumerate(history):
        h = v * WV_BAR_H // 100
        x = WV_GRAPH_X + i
        if x > 126:
            break
        if h <= 0:
            s.dot(x, base)
        else:
            s.line(x, base - h, x, base)
    s.line(0, base + 1, 127, base + 1)
    s.str(2, WV_LABEL_BASE, f"peak {scene['peak_score']}  run {scene['streak']}")
    s.str_right(126, WV_LABEL_BASE, "TREND")
    watch_status(s, f"6s  #{scene['streak']}  {scene['rate_hz']}Hz", "NO BASE")
    return s


def wrap(text, width=23):
    words, lines, line = text.split(), [], ""
    for word in words:
        candidate = f"{line} {word}".strip()
        if len(candidate) > width and line:
            lines.append(line)
            line = word
        else:
            line = candidate
    if line:
        lines.append(line)
    return lines


def detail_rows(scene):
    """The same row list detail_view_update() builds, in the same order."""
    rows = [("section", "WHY NOT HIGHER", "")]
    rows.append(("cap", scene["binding_cap"], "held"))
    rows += [("para", line, "") for line in wrap(scene["binding_cap_reason"])]
    for fam in scene["families"]:
        rows.append(("section", fam["name"], f"{fam['points']}/{fam['max']}"))
        for sig in scene["signals"]:
            if sig["family"] != fam["name"]:
                continue
            rows.append(
                ("signal", sig["name"], f"+{sig['points']}" if sig["points"] else "-")
            )
            rows.append(("para", "  " + sig["detail"], ""))
    rows.append(("section", "CEILINGS", ""))
    for cap in scene["caps"]:
        rows.append(("cap", cap, "held" if cap == scene["binding_cap"] else ""))
    rows.append(("section", "MEASURED", ""))
    rows.append(("fact", "Raw score", str(scene["raw_score"])))
    rows.append(("fact", "Adv busy", pct(scene["adv_busy_ppt"])))
    rows.append(("fact", "Wi-Fi busy", pct(scene["ref_busy_ppt"])))
    for c in scene["chan"]:
        if c["label"] in ("37", "38", "39"):
            mhz = next(x["mhz"] for x in scene["chan"] if x["label"] == c["label"])
            rows.append(("fact", f"  ch {c['label']} ({mhz} MHz)", pct(c["busy_ppt"])))
    rows.append(("fact", "Loudest adv", f"{scene['adv_peak_dbm']} dBm"))
    rows.append(("fact", "Band floor", f"{scene['band_floor_dbm']} dBm"))
    rows.append(("fact", "Peak swing", f"{scene['adv_swing_db']} dB"))
    return rows


def pct(ppt):
    ppt = min(ppt, 1000)
    return f"{ppt // 10}.{ppt % 10}%"


def screen_detail(scene, top, ceiling):
    s = Screen()
    s.box(0, 0, W, 13)
    s.set_color(PAPER)
    s.str(2, 10, scene["verdict"], FONT_PRIMARY)
    s.str_right(126, 10, f"{scene['score']}/{ceiling}")
    s.set_color(INK)

    rows = detail_rows(scene)
    for i in range(DV_VISIBLE):
        if top + i >= len(rows):
            break
        kind, left, right = rows[top + i]
        base = DV_FIRST_BASE + i * DV_ROW_H
        if kind == "section":
            s.box(0, base - 7, 122, 8)
            s.set_color(PAPER)
            s.str(2, base, left)
            if right:
                s.str_right(120, base, right)
            s.set_color(INK)
            continue
        s.str(2, base, left)
        if right:
            s.str_right(120, base, right)

    # elements_scrollbar_pos(canvas, 126, 14, 50, top, count - visible + 1)
    total = max(len(rows) - DV_VISIBLE + 1, 1)
    s.line(126, 14, 126, 63)
    bar_h = max(50 * DV_VISIBLE // max(len(rows), 1), 4)
    bar_y = 14 + (50 - bar_h) * top // max(total - 1, 1)
    s.box(125, bar_y, 3, bar_h)
    return s


def screen_learn_channels(chans):
    s = Screen()
    s.box(0, 0, W, 12)
    crenellations(s, 0, 12, W, 3)
    s.set_color(PAPER)
    s.str(2, 9, "ONLY THREE CHANNELS")
    s.str_right(126, 9, "3/7")
    s.set_color(INK)

    base = LV_ART_Y + 22 - 3
    for c in chans:
        if c["adv"]:
            continue
        cx = chan_x(c["mhz"]) + WALL_BAR_W // 2
        for y in range(base - 8, base, 2):
            s.line(cx - 13, y, cx + 13, y)
    s.line(0, base, 127, base)
    for i in range(40):
        s.dot(4 + (i * 104) // 39, base - 1)
    for c in chans:
        if not c["adv"]:
            continue
        cx = chan_x(c["mhz"]) + WALL_BAR_W // 2
        s.box(cx - 1, base - 14, 3, 14)

    s.str(2, LV_TEXT[0], "All that shouting uses")
    s.str(2, LV_TEXT[1], "2402, 2426, 2480 MHz -")
    s.str(2, LV_TEXT[2], "the gaps between Wi-Fi.")
    return s


def screen_splash():
    s = Screen()
    s.box(0, SPL_WALL_Y + 4, 128, SPL_WALL_H - 4)
    crenellations(s, 0, SPL_WALL_Y, 128, 4)
    s.set_color(PAPER)
    for x in range(4, 128, 12):
        s.line(x, SPL_WALL_Y + 8, x, SPL_WALL_Y + SPL_WALL_H - 1)
    s.line(0, SPL_WALL_Y + 7, 127, SPL_WALL_Y + 7)
    s.set_color(INK)

    # wall_art_shield(canvas, 50, 3, false)
    x, y, w, shoulder = 50, 2, 28, 14
    s.line(x, y, x + w, y)
    s.line(x, y, x, y + shoulder)
    s.line(x + w, y, x + w, y + shoulder)
    s.line(x, y + shoulder, x + w // 2, y + shoulder + 12)
    s.line(x + w, y + shoulder, x + w // 2, y + shoulder + 12)
    for i, bh in enumerate((6, 10, 8)):
        s.box(x + 7 + i * 5, y + 4 + (10 - bh), 3, bh)

    for i in range(3):
        yy = 12 + i * 6
        s.line(20, yy, 16, yy)
        s.line(108, yy, 112, yy)

    s.str_center(64, 53, "BULWARK", FONT_PRIMARY)
    s.str_center(64, 63, "Hears the popup flood")
    return s


def main():
    if not DUMP.exists():
        raise SystemExit("run 'make -C test dump' first")
    data = json.loads(DUMP.read_text())
    ceiling = data["ceiling"]

    # The dump lists the channel plan once and every scene's occupancy in the
    # same order; join them so a screen only has to look in one place.
    for scene in data["scenes"]:
        for measured, plan in zip(scene["chan"], data["channels"]):
            measured.update(mhz=plan["mhz"], adv=plan["adv"])
    scenes = {s["name"]: s for s in data["scenes"]}

    spam = scenes["Spam in pocket"]
    quiet = scenes["Quiet room"]
    oven = scenes["Microwave oven"]

    screen_wall(spam, "NO BASE", marked="38").save(IMAGES / "screen_watch_spam.png")
    screen_wall(quiet, "BASE  x0", marked="W6", baseline_ppt=14).save(
        IMAGES / "screen_watch_clear.png"
    )
    screen_wall(oven, "WIFI HIGH", marked="39").save(IMAGES / "screen_watch_oven.png")

    # A believable session: quiet, then something arrives and stays.
    history = [4, 6, 4, 8, 5, 4, 6, 4, 5, 7, 4, 5, 4, 6, 5] + [
        30, 52, 64, 71, 78, 84, 88, 88, 86, 88, 88, 84, 88, 88, 88, 88, 86, 88
    ]
    screen_trend(spam, history).save(IMAGES / "screen_trend.png")

    screen_detail(spam, 0, ceiling).save(IMAGES / "screen_detail_why.png")
    screen_detail(spam, 6, ceiling).save(IMAGES / "screen_detail_signals.png")
    screen_detail(oven, 0, ceiling).save(IMAGES / "screen_detail_oven.png")

    screen_learn_channels(data["channels"]).save(IMAGES / "screen_learn_channels.png")
    screen_splash().save(IMAGES / "screen_splash.png")


if __name__ == "__main__":
    main()
