from __future__ import annotations

import argparse
import re
from pathlib import Path
from urllib.request import urlopen
from xml.etree import ElementTree

from GenerateDebugGuiMsdf import (
    TtfFont,
    contour_to_segments,
    distance_to_segment,
    encode_distance,
    point_in_contours,
)


CELL_W = 64
CELL_H = 64
PX_RANGE = 6.0
PADDING = 4.0
COLS = 5

FONT_URL = "https://raw.githubusercontent.com/google/material-design-icons/master/font/MaterialIcons-Regular.ttf"
CODEPOINTS_URL = "https://raw.githubusercontent.com/google/material-design-icons/master/font/MaterialIcons-Regular.codepoints"
MDI_CUBE_URL = "https://raw.githubusercontent.com/Templarian/MaterialDesign/master/svg/cube.svg"

ICONS: list[tuple[int, str, str]] = [
    (1, "account_tree", "font"),
    (2, "videocam", "font"),
    (3, "tune", "font"),
    (4, "folder", "font"),
    (5, "terminal", "font"),
    (6, "folder", "font"),
    (7, "insert_drive_file", "font"),
    (8, "subdirectory_arrow_left", "font"),
    (9, "extension", "font"),
    (10, "open_with", "font"),
    (11, "photo_camera", "font"),
    (12, "lightbulb_outline", "font"),
    (13, "view_in_ar", "font"),
    (14, "code", "font"),
    (15, "hub", "font"),
    (16, "cube", "svg"),
]


def download_text(url: str) -> str:
    with urlopen(url) as response:
        return response.read().decode("utf-8")


def download_bytes(url: str) -> bytes:
    with urlopen(url) as response:
        return response.read()


def parse_codepoints(text: str) -> dict[str, int]:
    mapping: dict[str, int] = {}
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        mapping[parts[0]] = int(parts[1], 16)
    return mapping


def tokenize_svg_path(path_data: str) -> list[str]:
    return re.findall(r"[A-Za-z]|[-+]?(?:\d*\.\d+|\d+)", path_data)


def flatten_cubic(
    p0: tuple[float, float],
    p1: tuple[float, float],
    p2: tuple[float, float],
    p3: tuple[float, float],
    steps: int = 12,
) -> list[tuple[float, float]]:
    points: list[tuple[float, float]] = []
    for index in range(1, steps + 1):
        t = index / float(steps)
        mt = 1.0 - t
        x = (
            mt * mt * mt * p0[0]
            + 3.0 * mt * mt * t * p1[0]
            + 3.0 * mt * t * t * p2[0]
            + t * t * t * p3[0]
        )
        y = (
            mt * mt * mt * p0[1]
            + 3.0 * mt * mt * t * p1[1]
            + 3.0 * mt * t * t * p2[1]
            + t * t * t * p3[1]
        )
        points.append((x, y))
    return points


def parse_svg_path_contours(path_data: str) -> list[list[tuple[float, float]]]:
    tokens = tokenize_svg_path(path_data)
    contours: list[list[tuple[float, float]]] = []
    current_contour: list[tuple[float, float]] = []
    current = (0.0, 0.0)
    start = (0.0, 0.0)
    index = 0
    command = ""
    while index < len(tokens):
        token = tokens[index]
        if re.fullmatch(r"[A-Za-z]", token):
            command = token
            index += 1
        if not command:
            raise RuntimeError("Invalid SVG path command sequence.")

        if command == "M":
            x = float(tokens[index])
            y = float(tokens[index + 1])
            index += 2
            if current_contour:
                if current_contour[0] != current_contour[-1]:
                    current_contour.append(current_contour[0])
                contours.append(current_contour)
            current = (x, y)
            start = current
            current_contour = [current]
            command = "L"
        elif command == "L":
            x = float(tokens[index])
            y = float(tokens[index + 1])
            index += 2
            current = (x, y)
            current_contour.append(current)
        elif command == "H":
            x = float(tokens[index])
            index += 1
            current = (x, current[1])
            current_contour.append(current)
        elif command == "V":
            y = float(tokens[index])
            index += 1
            current = (current[0], y)
            current_contour.append(current)
        elif command == "C":
            p0 = current
            p1 = (float(tokens[index]), float(tokens[index + 1]))
            p2 = (float(tokens[index + 2]), float(tokens[index + 3]))
            p3 = (float(tokens[index + 4]), float(tokens[index + 5]))
            index += 6
            current_contour.extend(flatten_cubic(p0, p1, p2, p3))
            current = p3
        elif command in {"Z", "z"}:
            if current_contour:
                if current_contour[0] != current_contour[-1]:
                    current_contour.append(start)
                contours.append(current_contour)
                current_contour = []
            command = ""
        else:
            raise RuntimeError(f"Unsupported SVG path command: {command}")

    if current_contour:
        if current_contour[0] != current_contour[-1]:
            current_contour.append(current_contour[0])
        contours.append(current_contour)
    return contours


def build_svg_sdf(path_data: str) -> list[int]:
    contours = parse_svg_path_contours(path_data)
    if not contours:
        return [0] * (CELL_W * CELL_H)

    transformed_edges: list[tuple[float, float, float, float]] = []
    transformed_contours: list[list[tuple[float, float]]] = []
    scale = min((CELL_W - PADDING * 2.0) / 24.0, (CELL_H - PADDING * 2.0) / 24.0)
    ox = (CELL_W - 24.0 * scale) * 0.5
    oy = (CELL_H - 24.0 * scale) * 0.5
    for contour in contours:
        transformed = [(ox + x * scale, oy + y * scale) for (x, y) in contour]
        transformed_contours.append(transformed)
        for point_index in range(len(transformed) - 1):
            x0, y0 = transformed[point_index]
            x1, y1 = transformed[point_index + 1]
            transformed_edges.append((x0, y0, x1, y1))

    pixels = [0] * (CELL_W * CELL_H)
    for y in range(CELL_H):
        for x in range(CELL_W):
            px = x + 0.5
            py = y + 0.5
            inside = point_in_contours(px, py, transformed_contours)
            distance = 1.0e9
            for x0, y0, x1, y1 in transformed_edges:
                distance = min(distance, distance_to_segment(px, py, x0, y0, x1, y1))
            signed_distance = distance if inside else -distance
            pixels[y * CELL_W + x] = encode_distance(signed_distance)
    return pixels


def build_icon_sdf(
    font: TtfFont,
    gid: int,
    scale: float,
    baseline: float,
    bbox_width: float,
    bbox_min_x: float,
) -> list[int]:
    contours, _ = font.glyph_outline(gid)
    if not contours:
        return [0] * (CELL_W * CELL_H)

    transformed_edges: list[tuple[float, float, float, float]] = []
    transformed_contours: list[list[tuple[float, float]]] = []
    ox = (CELL_W - bbox_width * scale) * 0.5 - bbox_min_x * scale
    for contour in contours:
        segs, poly = contour_to_segments(contour)
        if not segs or not poly:
            continue
        transformed_edges.extend(
            [(ox + x0 * scale, baseline - y0 * scale, ox + x1 * scale, baseline - y1 * scale)
             for (x0, y0, x1, y1) in segs]
        )
        transformed_contours.append([(ox + x * scale, baseline - y * scale) for (x, y) in poly])

    if not transformed_edges:
        return [0] * (CELL_W * CELL_H)

    pixels = [0] * (CELL_W * CELL_H)
    for y in range(CELL_H):
        for x in range(CELL_W):
            px = x + 0.5
            py = y + 0.5
            inside = point_in_contours(px, py, transformed_contours)
            distance = 1.0e9
            for x0, y0, x1, y1 in transformed_edges:
                distance = min(distance, distance_to_segment(px, py, x0, y0, x1, y1))
            signed_distance = distance if inside else -distance
            pixels[y * CELL_W + x] = encode_distance(signed_distance)
    return pixels


def write_inl(out_path: Path, pixels: list[int], icon_ids: list[int], rows: int) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="ascii", newline="\n") as file:
        file.write("// Generated monochrome SDF icon atlas for DebugGui editor icons.\n")
        file.write("// Generated by Tool/Font/GenerateEditorIconAtlas.py\n\n")
        file.write("#pragma once\n\n")
        file.write("inline auto GetEditorIconAtlasCols() noexcept -> u32 { return ")
        file.write(f"{COLS}U; }}\n")
        file.write("inline auto GetEditorIconAtlasRows() noexcept -> u32 { return ")
        file.write(f"{rows}U; }}\n")
        file.write("inline auto GetEditorIconAtlasIconCount() noexcept -> u32 { return ")
        file.write(f"{len(icon_ids)}U; }}\n\n")
        file.write("inline auto GetEditorIconAtlasIconId(u32 index) noexcept -> u32 {\n")
        file.write(f"    static constexpr u32 kIds[{len(icon_ids)}] = {{\n")
        for index, icon_id in enumerate(icon_ids):
            if index % 8 == 0:
                file.write("        ")
            file.write(f"{icon_id}U")
            if index + 1 != len(icon_ids):
                file.write(", ")
            if (index + 1) % 8 == 0:
                file.write("\n")
        if len(icon_ids) % 8 != 0:
            file.write("\n")
        file.write("    };\n")
        file.write("    return (index < ")
        file.write(f"{len(icon_ids)}U) ? kIds[index] : 0U;\n")
        file.write("}\n\n")
        file.write("inline auto GetEditorIconAtlasPixels() noexcept -> const u8* {\n")
        file.write(f"    static constexpr u8 kPixels[{len(pixels)}] = {{\n")
        for index, value in enumerate(pixels):
            if index % 16 == 0:
                file.write("        ")
            file.write(f"0x{value:02X}")
            if index + 1 != len(pixels):
                file.write(", ")
            if (index + 1) % 16 == 0:
                file.write("\n")
        if len(pixels) % 16 != 0:
            file.write("\n")
        file.write("    };\n")
        file.write("    return kPixels;\n")
        file.write("}\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the editor icon SDF atlas.")
    parser.add_argument(
        "--out",
        type=str,
        default="Source/Runtime/DebugGui/Private/DebugGui/EditorIconAtlasSDF64.inl",
        help="Output .inl path.",
    )
    args = parser.parse_args()

    codepoint_map = parse_codepoints(download_text(CODEPOINTS_URL))
    font = TtfFont(download_bytes(FONT_URL))
    cube_svg = download_text(MDI_CUBE_URL)
    svg_root = ElementTree.fromstring(cube_svg)
    path_node = svg_root.find("{http://www.w3.org/2000/svg}path")
    if path_node is None or "d" not in path_node.attrib:
        raise RuntimeError("Failed to parse mdi cube SVG path.")
    cube_path_data = path_node.attrib["d"]

    icon_glyphs: list[tuple[int, str, int, tuple[int, int, int, int] | None]] = []
    global_min_x = 1.0e9
    global_min_y = 1.0e9
    global_max_x = -1.0e9
    global_max_y = -1.0e9
    for icon_id, name, source in ICONS:
        if source == "svg":
            icon_glyphs.append((icon_id, source, 0, None))
            continue
        if name not in codepoint_map:
            raise RuntimeError(f"Missing icon codepoint: {name}")
        gid = font.glyph_index(codepoint_map[name])
        contours, bounds = font.glyph_outline(gid)
        if not contours:
            raise RuntimeError(f"Icon has no outline: {name}")
        icon_glyphs.append((icon_id, source, gid, bounds))
        x_min, y_min, x_max, y_max = bounds
        global_min_x = min(global_min_x, float(x_min))
        global_min_y = min(global_min_y, float(y_min))
        global_max_x = max(global_max_x, float(x_max))
        global_max_y = max(global_max_y, float(y_max))

    width = global_max_x - global_min_x
    height = global_max_y - global_min_y
    if width <= 1.0 or height <= 1.0:
        raise RuntimeError("Invalid icon bounds.")

    scale = min((CELL_W - PADDING * 2.0) / width, (CELL_H - PADDING * 2.0) / height)
    baseline = PADDING + global_max_y * scale

    rows = (len(icon_glyphs) + COLS - 1) // COLS
    atlas_pixels = [0] * (COLS * rows * CELL_W * CELL_H)
    icon_ids: list[int] = []
    for index, (icon_id, source, gid, bounds) in enumerate(icon_glyphs):
        if source == "svg":
            glyph_pixels = build_svg_sdf(cube_path_data)
        else:
            assert bounds is not None
            x_min, _, x_max, _ = bounds
            bbox_width = float(x_max - x_min)
            glyph_pixels = build_icon_sdf(font, gid, scale, baseline, bbox_width, float(x_min))
        cell_x = (index % COLS) * CELL_W
        cell_y = (index // COLS) * CELL_H
        for y in range(CELL_H):
            row_src = y * CELL_W
            row_dst = (cell_y + y) * (COLS * CELL_W) + cell_x
            atlas_pixels[row_dst:row_dst + CELL_W] = glyph_pixels[row_src:row_src + CELL_W]
        icon_ids.append(icon_id)

    write_inl(Path(args.out), atlas_pixels, icon_ids, rows)
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
