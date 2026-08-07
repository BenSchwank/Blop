#!/usr/bin/env python3
"""Regenerate Android launcher mipmaps from assets/logo.jpg."""
from __future__ import annotations

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "logo.jpg"
RES = ROOT / "android" / "res"
BG = (15, 16, 30, 255)


def main() -> None:
    src = Image.open(SRC).convert("RGBA")
    w, h = src.size
    crop_h = int(h * 0.62)
    icon = src.crop((0, 0, w, crop_h))
    side = max(icon.size)
    square = Image.new("RGBA", (side, side), BG)
    square.paste(icon, ((side - icon.size[0]) // 2, (side - icon.size[1]) // 2), icon)

    sizes = {
        "mipmap-mdpi": 48,
        "mipmap-hdpi": 72,
        "mipmap-xhdpi": 96,
        "mipmap-xxhdpi": 144,
        "mipmap-xxxhdpi": 192,
    }
    for folder, px in sizes.items():
        d = RES / folder
        d.mkdir(parents=True, exist_ok=True)
        im = square.resize((px, px), Image.Resampling.LANCZOS)
        im.save(d / "ic_launcher.png", "PNG")
        im.save(d / "ic_launcher_round.png", "PNG")
        print(f"wrote {d} ({px}px)")

    fg_size = 432
    safe = int(fg_size * 0.66)
    bg = Image.new("RGBA", (fg_size, fg_size), BG)
    fg_content = square.resize((safe, safe), Image.Resampling.LANCZOS)
    fg = Image.new("RGBA", (fg_size, fg_size), (0, 0, 0, 0))
    off = (fg_size - safe) // 2
    fg.paste(fg_content, (off, off), fg_content)

    drawable = RES / "drawable"
    drawable.mkdir(parents=True, exist_ok=True)
    bg.save(drawable / "ic_launcher_background.png", "PNG")
    fg.save(drawable / "ic_launcher_foreground.png", "PNG")
    square.resize((192, 192), Image.Resampling.LANCZOS).save(
        drawable / "ic_launcher_blop.png", "PNG"
    )
    print("adaptive layers ok")


if __name__ == "__main__":
    main()
