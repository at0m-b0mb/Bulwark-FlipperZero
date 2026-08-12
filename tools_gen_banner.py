#!/usr/bin/env python3
"""Bulwark - the README banner.

A wall at night with something throwing itself at it. The three tall bars are
the Bluetooth advertising channels at their real spacing across the band; the
short hatched ones are the Wi-Fi centres in between, which is the whole reason
those three frequencies can be told apart from everything else in 2.4 GHz.

Drawn at 3x and downsampled, so the edges are clean.

    python3 tools_gen_banner.py
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
OUT = HERE / "images" / "banner.png"

W, H = 1280, 380
SS = 3  # supersample

BG_TOP = (9, 12, 20)
BG_BOTTOM = (17, 22, 34)
WALL = (228, 233, 242)
WALL_DARK = (150, 159, 178)
ACCENT = (255, 130, 0)  # the Flipper's own backlight orange
MUTED = (66, 79, 102)
TEXT = (236, 240, 248)
DIM = (146, 158, 180)

# adv 37, Wi-Fi 1, adv 38, Wi-Fi 6, Wi-Fi 11, adv 39
CHANNELS = [
    (2402, True, 0.86),
    (2412, False, 0.10),
    (2426, True, 0.92),
    (2436, False, 0.13),
    (2462, False, 0.09),
    (2480, True, 0.83),
]


def load_font(size, bold=False):
    names = (
        ["Arial Bold.ttf", "Tahoma Bold.ttf", "DejaVuSans-Bold.ttf"]
        if bold
        else ["Arial.ttf", "Tahoma.ttf", "HelveticaNeue.ttc", "DejaVuSans.ttf"]
    )
    for name in names:
        for folder in (
            "/System/Library/Fonts",
            "/System/Library/Fonts/Supplemental",
            "/Library/Fonts",
            "/usr/share/fonts/truetype/dejavu",
        ):
            path = Path(folder) / name
            if path.exists():
                try:
                    return ImageFont.truetype(str(path), size)
                except OSError:
                    continue
    return ImageFont.load_default()


def background(d):
    for y in range(H * SS):
        t = y / (H * SS)
        d.line(
            [0, y, W * SS, y],
            fill=tuple(int(a + (b - a) * t) for a, b in zip(BG_TOP, BG_BOTTOM)),
        )


def crenellated_wall(d, base_y, height, merlon_w=26, gap=22, merlon_h=16):
    """The thing the app is named after, along the bottom edge."""
    d.rectangle(
        [0, base_y * SS, W * SS, (base_y + height) * SS], fill=(23, 29, 44)
    )
    x = 0
    while x < W:
        d.rectangle(
            [x * SS, (base_y - merlon_h) * SS, (x + merlon_w) * SS, base_y * SS],
            fill=(23, 29, 44),
        )
        x += merlon_w + gap

    # Courses of stone, staggered.
    for i, y in enumerate(range(base_y + 14, base_y + height, 22)):
        d.line([0, y * SS, W * SS, y * SS], fill=(13, 17, 27), width=2 * SS)
        offset = 0 if i % 2 else 60
        for x in range(offset, W, 120):
            d.line(
                [x * SS, y * SS, x * SS, (y + 22) * SS],
                fill=(13, 17, 27),
                width=2 * SS,
            )


def shield(d, cx, cy, w, h, outline, fill=None):
    shoulder = cy - h // 2 + int(h * 0.52)
    pts = [
        (cx - w // 2, cy - h // 2),
        (cx + w // 2, cy - h // 2),
        (cx + w // 2, shoulder),
        (cx, cy + h // 2),
        (cx - w // 2, shoulder),
    ]
    d.polygon(
        [(x * SS, y * SS) for x, y in pts], fill=fill, outline=outline, width=4 * SS
    )


def spectrum(d, x0, base_y, span, height):
    lo, hi = 2402, 2480
    for mhz, adv, level in CHANNELS:
        x = x0 + int((mhz - lo) * span / (hi - lo))
        bar_w = 34
        h = int(height * level)
        if adv:
            d.rectangle(
                [x * SS, (base_y - h) * SS, (x + bar_w) * SS, base_y * SS], fill=ACCENT
            )
            # crenellated cap, so even the bars are little walls
            for notch in (10, 22):
                d.rectangle(
                    [
                        (x + notch) * SS,
                        (base_y - h) * SS,
                        (x + notch + 5) * SS,
                        (base_y - h + 7) * SS,
                    ],
                    fill=BG_BOTTOM,
                )
        else:
            for y in range(base_y - h, base_y, 8):
                d.line(
                    [x * SS, y * SS, (x + bar_w) * SS, y * SS], fill=MUTED, width=3 * SS
                )
        d.line(
            [x * SS, base_y * SS, (x + bar_w) * SS, base_y * SS],
            fill=WALL_DARK,
            width=3 * SS,
        )


def main():
    img = Image.new("RGB", (W * SS, H * SS), BG_TOP)
    d = ImageDraw.Draw(img)

    background(d)
    crenellated_wall(d, base_y=318, height=H - 318)

    # --- the mark ---------------------------------------------------------
    shield(d, cx=124, cy=146, w=142, h=168, outline=ACCENT)
    bars_bottom = 146 + 26
    for i, bh in enumerate((46, 74, 60)):
        bx = 124 - 48 + i * 34
        d.rectangle(
            [bx * SS, (bars_bottom - bh) * SS, (bx + 22) * SS, bars_bottom * SS],
            fill=WALL,
        )

    # --- wordmark ---------------------------------------------------------
    title = load_font(112, bold=True)
    tag = load_font(40, bold=True)
    body = load_font(29)

    d.text((238 * SS, 58 * SS), "BULWARK", font=title, fill=TEXT)
    d.text((243 * SS, 182 * SS), "Hears the popup flood.", font=tag, fill=ACCENT)
    d.text(
        (243 * SS, 238 * SS),
        "Bluetooth LE advertising-spam detector, on the Flipper Zero's own radio.",
        font=body,
        fill=DIM,
    )
    d.text(
        (243 * SS, 276 * SS),
        "No extra hardware. It hears the flood, never reads a packet, and says so.",
        font=body,
        fill=DIM,
    )

    # --- the band ---------------------------------------------------------
    spectrum(d, x0=760, base_y=210, span=430, height=170)
    small = load_font(24)
    for mhz, adv, _ in CHANNELS:
        x = 760 + int((mhz - 2402) * 430 / 78)
        label = {2402: "37", 2426: "38", 2480: "39"}.get(mhz)
        if label:
            d.text(((x + 6) * SS, 220 * SS), label, font=small, fill=ACCENT)
    d.text((760 * SS, 262 * SS), "2402", font=small, fill=MUTED)
    d.text((1156 * SS, 262 * SS), "2480 MHz", font=small, fill=MUTED)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.resize((W, H), Image.LANCZOS).save(OUT)
    print(f"wrote {OUT.relative_to(HERE)}  ({W}x{H})")


if __name__ == "__main__":
    main()
