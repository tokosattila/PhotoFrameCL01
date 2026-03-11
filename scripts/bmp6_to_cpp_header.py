#!/usr/bin/env python3
"""
Convert a 6-color dithered BMP image to a C++ header used by the photo frame fallback image.

Expected input colors (RGB):
- black  : (0, 0, 0)       -> 0x0
- white  : (255, 255, 255) -> 0x1
- yellow : (255, 255, 0)   -> 0x2
- red    : (255, 0, 0)     -> 0x3
- blue   : (0, 0, 255)     -> 0x5
- green  : (0, 255, 0)     -> 0x6

Packed output format:
- 2 pixels / byte
- even X pixel goes to high nibble
- odd X pixel goes to low nibble
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Dict, Tuple

from PIL import Image


Color = Tuple[int, int, int]

EPD_COLOR_MAP: Dict[Color, int] = {
    (0, 0, 0): 0x0,
    (255, 255, 255): 0x1,
    (255, 255, 0): 0x2,
    (255, 0, 0): 0x3,
    (0, 0, 255): 0x5,
    (0, 255, 0): 0x6,
}


def nearest_epd_code(rgb: Color) -> int:
    best_code = 0x1
    best_dist = None
    for palette_rgb, code in EPD_COLOR_MAP.items():
        dr = rgb[0] - palette_rgb[0]
        dg = rgb[1] - palette_rgb[1]
        db = rgb[2] - palette_rgb[2]
        dist = dr * dr + dg * dg + db * db
        if best_dist is None or dist < best_dist:
            best_dist = dist
            best_code = code
    return best_code


def pack_pixels_to_nibbles(img: Image.Image, strict: bool) -> bytes:
    rgb = img.convert("RGB")
    width, height = rgb.size
    out = bytearray((width * height + 1) // 2)

    out_index = 0
    for y in range(height):
        x = 0
        while x < width:
            p0 = rgb.getpixel((x, y))
            if p0 not in EPD_COLOR_MAP:
                if strict:
                    raise ValueError(
                        f"Unsupported color at ({x},{y}): {p0}. "
                        "Use --no-strict to map to nearest palette color."
                    )
                c0 = nearest_epd_code(p0)
            else:
                c0 = EPD_COLOR_MAP[p0]

            if x + 1 < width:
                p1 = rgb.getpixel((x + 1, y))
                if p1 not in EPD_COLOR_MAP:
                    if strict:
                        raise ValueError(
                            f"Unsupported color at ({x+1},{y}): {p1}. "
                            "Use --no-strict to map to nearest palette color."
                        )
                    c1 = nearest_epd_code(p1)
                else:
                    c1 = EPD_COLOR_MAP[p1]
            else:
                c1 = EPD_COLOR_MAP[(255, 255, 255)]

            out[out_index] = ((c0 & 0x0F) << 4) | (c1 & 0x0F)
            out_index += 1
            x += 2

    return bytes(out)


def format_c_array(data: bytes, bytes_per_line: int = 16) -> str:
    lines = []
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i : i + bytes_per_line]
        line = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"  {line}")
    return ",\n".join(lines)


def build_header(width: int, height: int, array_name: str, data: bytes, add_guard: bool) -> str:
    guard_name = f"{array_name.upper()}_H"
    body = []
    if add_guard:
        body.append(f"#ifndef {guard_name}")
        body.append(f"#define {guard_name}")
        body.append("")

    body.append(f"const uint32_t {array_name}Width = {width};")
    body.append(f"const uint32_t {array_name}Height = {height};")
    body.append(f"const uint8_t {array_name}[({width}*{height}+1)/2] = {{")
    body.append(format_c_array(data))
    body.append("};")

    if add_guard:
        body.append("")
        body.append("#endif")

    body.append("")
    return "\n".join(body)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert 6-color BMP to C++ header array.")
    parser.add_argument("input_bmp", type=Path, help="Input BMP file (6-color dithered).")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("src/Images/DefaultImage.h"),
        help="Output header path. Default: src/Images/DefaultImage.h",
    )
    parser.add_argument(
        "--array-name",
        default="DefaultImage",
        help="C++ array base name. Default: DefaultImage",
    )
    parser.add_argument(
        "--no-strict",
        action="store_true",
        help="Allow non-palette pixels and map them to nearest e-paper color.",
    )
    parser.add_argument(
        "--header-guard",
        action="store_true",
        help="Add include guard to generated header.",
    )
    args = parser.parse_args()

    if not args.input_bmp.exists():
        raise SystemExit(f"Input file not found: {args.input_bmp}")

    img = Image.open(args.input_bmp)
    width, height = img.size
    packed = pack_pixels_to_nibbles(img, strict=not args.no_strict)
    header_text = build_header(
        width=width,
        height=height,
        array_name=args.array_name,
        data=packed,
        add_guard=args.header_guard,
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header_text, encoding="utf-8")

    print(f"OK: generated {args.output} ({width}x{height}, {len(packed)} bytes)")


if __name__ == "__main__":
    main()
