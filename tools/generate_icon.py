#!/usr/bin/env python3
"""Generate multi-resolution Windows ICO and PNG assets for VanillaBTN."""

import sys
from pathlib import Path
from PIL import Image

ICON_SIZES = [
    (256, 256),
    (128, 128),
    (64, 64),
    (48, 48),
    (32, 32),
    (24, 24),
    (16, 16),
]


def generate_icon(source_path: Path, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    with Image.open(source_path) as im:
        rgba = im.convert("RGBA")

        # Save 256x256 master PNG
        png_path = output_dir / "vanillabtn_icon.png"
        rgba.save(png_path, format="PNG")
        print(f"Saved master PNG: {png_path} ({rgba.size[0]}x{rgba.size[1]})")

        # Save multi-size ICO
        ico_path = output_dir / "vanillabtn.ico"
        rgba.save(ico_path, format="ICO", sizes=ICON_SIZES)
        print(f"Saved Windows ICO: {ico_path} with sizes: {ICON_SIZES}")


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    default_src = repo_root / "native" / "res" / "vanillabtn_icon.png"
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else default_src
    if not src.exists():
        print(f"Error: Source image not found at {src}", file=sys.stderr)
        return 1

    dest_dir = repo_root / "native" / "res"
    generate_icon(src, dest_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
