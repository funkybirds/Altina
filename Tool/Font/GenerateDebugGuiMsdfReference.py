from __future__ import annotations

import argparse
import io
import json
import math
import re
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile
from pathlib import Path

from GenerateDebugGuiMsdf import CELL_H
from GenerateDebugGuiMsdf import CELL_W
from GenerateDebugGuiMsdf import FIRST_CHAR
from GenerateDebugGuiMsdf import LAST_CHAR
from GenerateDebugGuiMsdf import PADDING
from GenerateDebugGuiMsdf import TARGET_DRAW_H
from GenerateDebugGuiMsdf import TARGET_DRAW_W
from GenerateDebugGuiMsdf import TtfError
from GenerateDebugGuiMsdf import TtfFont
from GenerateDebugGuiMsdf import write_inl

MSDF_ATLAS_GEN_VERSION = "1.4"
MSDF_ATLAS_GEN_RELEASE_URL = (
    "https://github.com/Chlumsky/msdf-atlas-gen/releases/download/"
    f"v{MSDF_ATLAS_GEN_VERSION}/msdf-atlas-gen-{MSDF_ATLAS_GEN_VERSION}-win64.zip"
)
UNIFORM_COLUMNS = 16
DEFAULT_CHARSET_SPEC = "[0x20,0x7E]"
DEFAULT_PX_RANGE = 4.0
DEFAULT_PX_PADDING = 0.0


def parse_metric_values(text: str, function_name: str) -> list[float]:
    pattern = (
        r"inline auto\s+"
        + re.escape(function_name)
        + r"\(u8 ch\)\s+noexcept\s+->\s+f32\s*\{.*?"
        + r"static constexpr f32 kValues\[kGlyphCount\]\s*=\s*\{(.*?)\};"
    )
    match = re.search(pattern, text, re.S)
    if not match:
        raise RuntimeError(f"failed to parse metric block: {function_name}")
    values: list[float] = []
    for token in match.group(1).replace("\n", " ").split(","):
        value = token.strip()
        if not value:
            continue
        if value.endswith("f"):
            value = value[:-1]
        values.append(float(value))
    return values


def parse_scalar_value(text: str, function_name: str) -> float:
    pattern = (
        r"inline auto\s+"
        + re.escape(function_name)
        + r"\(\)\s+noexcept\s+->\s+f32\s*\{\s*return\s+([0-9eE+\-.]+)f;\s*\}"
    )
    match = re.search(pattern, text, re.S)
    if not match:
        raise RuntimeError(f"failed to parse scalar block: {function_name}")
    return float(match.group(1))


def load_legacy_metrics(inl_path: Path) -> tuple[list[float], list[float], list[float], float, float]:
    text = inl_path.read_text(encoding="ascii")
    advances = parse_metric_values(text, "GetFont32x32GlyphAdvance")
    bearings_x = parse_metric_values(text, "GetFont32x32GlyphBearingX")
    bearings_y = parse_metric_values(text, "GetFont32x32GlyphBearingY")
    recommended_stretch_x = parse_scalar_value(text, "GetFont32x32RecommendedStretchX")
    nominal_aspect = parse_scalar_value(text, "GetFont32x32NominalAspect")
    return advances, bearings_x, bearings_y, nominal_aspect, recommended_stretch_x


def compute_metrics_from_ttf(ttf_path: Path) -> tuple[list[float], list[float], list[float], float, float]:
    font = TtfFont(ttf_path.read_bytes())

    cap_min_y = 1.0e9
    cap_max_y = -1.0e9
    cap_width_sum = 0.0
    cap_count = 0
    has_outline = False
    has_caps = False
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        gid = font.glyph_index(code)
        contours, _ = font.glyph_outline(gid)
        glyph_min_x = 1.0e9
        glyph_max_x = -1.0e9
        glyph_has = False
        for contour in contours:
            for point in contour:
                glyph_has = True
                has_outline = True
                if ord("A") <= code <= ord("Z"):
                    has_caps = True
                    cap_min_y = min(cap_min_y, point.y)
                    cap_max_y = max(cap_max_y, point.y)
                glyph_min_x = min(glyph_min_x, point.x)
                glyph_max_x = max(glyph_max_x, point.x)
        if glyph_has and ord("A") <= code <= ord("Z"):
            cap_width_sum += glyph_max_x - glyph_min_x
            cap_count += 1

    if not has_outline:
        raise TtfError("no outlines found for ASCII range")
    if not has_caps or cap_count == 0:
        raise TtfError("no cap outlines found for ASCII uppercase range")

    cap_h = cap_max_y - cap_min_y
    if cap_h <= 1.0:
        raise TtfError("invalid cap height")

    draw_scale = TARGET_DRAW_H / cap_h
    advances: list[float] = []
    bearings_x: list[float] = []
    bearings_y: list[float] = []
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        gid = font.glyph_index(code)
        adv = float(font.advance_widths[gid]) if gid < len(font.advance_widths) else 0.0
        lsb = float(font.left_side_bearings[gid]) if gid < len(font.left_side_bearings) else 0.0
        advances.append(adv * draw_scale)
        bearings_x.append(lsb * draw_scale)
        bearings_y.append(float(font.ascender) * draw_scale)

    cap_w_avg = cap_width_sum / float(cap_count)
    font_aspect = cap_w_avg / cap_h
    target_aspect = TARGET_DRAW_W / TARGET_DRAW_H
    recommended_stretch_x = target_aspect / font_aspect if font_aspect > 1.0e-6 else 1.0
    if recommended_stretch_x < 0.85:
        recommended_stretch_x = 0.85
    if recommended_stretch_x > 1.35:
        recommended_stretch_x = 1.35
    nominal_aspect = (float(sum(advances)) / len(advances)) / TARGET_DRAW_H
    return advances, bearings_x, bearings_y, nominal_aspect, recommended_stretch_x


def compute_cap_rule_size(ttf_path: Path) -> float:
    font = TtfFont(ttf_path.read_bytes())
    cap_min_y = 1.0e9
    cap_max_y = -1.0e9
    has_caps = False
    for code in range(ord("A"), ord("Z") + 1):
        gid = font.glyph_index(code)
        contours, _ = font.glyph_outline(gid)
        for contour in contours:
            for point in contour:
                has_caps = True
                cap_min_y = min(cap_min_y, point.y)
                cap_max_y = max(cap_max_y, point.y)

    if not has_caps:
        raise TtfError("no cap outlines found for ASCII uppercase range")

    cap_h = cap_max_y - cap_min_y
    if cap_h <= 1.0:
        raise TtfError("invalid cap height")

    scale = (float(CELL_H) - 2.0 * PADDING) / cap_h
    return scale * float(font.units_per_em)


def resolve_metrics_source(metrics_from: Path | None, out_path: Path) -> Path | None:
    if metrics_from is not None:
        return metrics_from if metrics_from.exists() else None
    if out_path.exists():
        return out_path
    return None


def ensure_msdf_atlas_gen(exe_path: Path) -> Path:
    if exe_path.exists():
        return exe_path

    exe_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"Downloading msdf-atlas-gen {MSDF_ATLAS_GEN_VERSION}...")
    with urllib.request.urlopen(MSDF_ATLAS_GEN_RELEASE_URL) as response:
        archive_bytes = response.read()
    with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
        member_name = None
        for name in archive.namelist():
            if name.endswith("/msdf-atlas-gen.exe"):
                member_name = name
                break
        if member_name is None:
            raise RuntimeError("msdf-atlas-gen.exe not found in downloaded archive")
        with archive.open(member_name) as source, exe_path.open("wb") as destination:
            shutil.copyfileobj(source, destination)
    return exe_path


def load_bin_image(bin_path: Path, width: int, height: int) -> tuple[bytes, int]:
    payload = bin_path.read_bytes()
    pixel_count = width * height
    if pixel_count <= 0:
        raise RuntimeError("invalid atlas dimensions")
    if len(payload) % pixel_count != 0:
        raise RuntimeError("unexpected atlas binary size")
    channels = len(payload) // pixel_count
    if channels not in (3, 4):
        raise RuntimeError(f"unexpected atlas channel count: {channels}")
    return payload, channels


def copy_cell(payload: bytes, atlas_width: int, atlas_height: int, channels: int, x0: int, y0: int) -> list[int]:
    if x0 < 0 or y0 < 0 or x0 + CELL_W > atlas_width or y0 + CELL_H > atlas_height:
        raise RuntimeError("glyph atlas bounds exceed source atlas")
    out: list[int] = []
    for row in range(CELL_H):
        row_start = ((y0 + row) * atlas_width + x0) * channels
        row_end = row_start + CELL_W * channels
        row_data = payload[row_start:row_end]
        if channels == 3:
            out.extend(row_data)
        else:
            for column in range(CELL_W):
                base = column * channels
                out.extend(row_data[base:base + 3])
    return out


def build_glyph_data_from_atlas(json_path: Path, bin_path: Path) -> list[int]:
    metadata = json.loads(json_path.read_text(encoding="utf-8"))
    atlas = metadata["atlas"]
    atlas_width = int(atlas["width"])
    atlas_height = int(atlas["height"])
    payload, channels = load_bin_image(bin_path, atlas_width, atlas_height)

    glyph_entries = {entry["unicode"]: entry for entry in metadata["glyphs"] if "unicode" in entry}
    glyph_data: list[int] = []
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        entry = glyph_entries.get(code)
        atlas_bounds = entry.get("atlasBounds") if entry is not None else None
        if atlas_bounds is None:
            glyph_data.extend([0] * (CELL_W * CELL_H * 3))
            continue
        x0 = int(math.floor(float(atlas_bounds["left"])))
        y0 = int(math.floor(float(atlas_bounds["top"])))
        x1 = int(math.ceil(float(atlas_bounds["right"])))
        y1 = int(math.ceil(float(atlas_bounds["bottom"])))
        if x1 - x0 != CELL_W or y1 - y0 != CELL_H:
            raise RuntimeError(
                f"unexpected cell size for U+{code:04X}: {(x1 - x0)}x{(y1 - y0)}"
            )
        glyph_data.extend(copy_cell(payload, atlas_width, atlas_height, channels, x0, y0))
    return glyph_data


def run_msdf_atlas_gen(
    exe_path: Path,
    ttf_path: Path,
    bin_path: Path,
    json_path: Path,
    px_range: float,
    em_size: float,
) -> None:
    command = [
        str(exe_path),
        "-font",
        str(ttf_path),
        "-type",
        "msdf",
        "-format",
        "bin",
        "-chars",
        DEFAULT_CHARSET_SPEC,
        "-uniformgrid",
        "-uniformcols",
        str(UNIFORM_COLUMNS),
        "-uniformcell",
        str(CELL_W),
        str(CELL_H),
        "-uniformorigin",
        "on",
        "-yorigin",
        "top",
        "-size",
        f"{em_size:.6f}",
        "-pxrange",
        f"{px_range:g}",
        "-pxpadding",
        f"{DEFAULT_PX_PADDING:g}",
        "-imageout",
        str(bin_path),
        "-json",
        str(json_path),
    ]
    subprocess.run(command, check=True)


def generate_reference_msdf(
    ttf_path: Path,
    out_path: Path,
    msdf_atlas_gen_path: Path,
    metrics_from: Path | None,
    keep_temp: bool,
    px_range: float,
) -> None:
    tool_path = ensure_msdf_atlas_gen(msdf_atlas_gen_path)
    temp_dir_obj: tempfile.TemporaryDirectory[str] | None = None
    temp_dir_path: Path
    if keep_temp:
        temp_dir_path = Path("Temp/DebugGuiMsdfReference")
        temp_dir_path.mkdir(parents=True, exist_ok=True)
    else:
        temp_dir_obj = tempfile.TemporaryDirectory(prefix="debuggui_msdf_")
        temp_dir_path = Path(temp_dir_obj.name)

    bin_path = temp_dir_path / "atlas.bin"
    json_path = temp_dir_path / "atlas.json"
    em_size = compute_cap_rule_size(ttf_path)
    run_msdf_atlas_gen(tool_path, ttf_path, bin_path, json_path, px_range, em_size)
    glyph_data = build_glyph_data_from_atlas(json_path, bin_path)

    metrics_source = resolve_metrics_source(metrics_from, out_path)
    if metrics_source is not None:
        advances, bearings_x, bearings_y, nominal_aspect, recommended_stretch_x = load_legacy_metrics(
            metrics_source
        )
    else:
        advances, bearings_x, bearings_y, nominal_aspect, recommended_stretch_x = compute_metrics_from_ttf(
            ttf_path
        )

    write_inl(
        out_path,
        glyph_data,
        advances,
        bearings_x,
        bearings_y,
        nominal_aspect,
        recommended_stretch_x,
    )

    if temp_dir_obj is not None:
        temp_dir_obj.cleanup()


def default_msdf_atlas_gen_path() -> Path:
    return Path("Tool/Font/.cache/msdf-atlas-gen") / MSDF_ATLAS_GEN_VERSION / "msdf-atlas-gen.exe"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate a DebugGui MSDF atlas using the official msdf-atlas-gen tool."
    )
    parser.add_argument("--ttf", type=str, required=True, help="Input TrueType file.")
    parser.add_argument("--out", type=str, required=True, help="Output .inl path.")
    parser.add_argument(
        "--msdf-atlas-gen",
        type=str,
        default="",
        help="Optional path to msdf-atlas-gen.exe. If omitted, the tool is cached/downloaded automatically.",
    )
    parser.add_argument(
        "--metrics-from",
        type=str,
        default="",
        help="Optional existing atlas .inl to reuse metrics/stretch/aspect from.",
    )
    parser.add_argument(
        "--keep-temp",
        action="store_true",
        help="Keep the intermediate atlas.bin and atlas.json in Temp/DebugGuiMsdfReference.",
    )
    parser.add_argument(
        "--px-range",
        type=float,
        default=DEFAULT_PX_RANGE,
        help="MSDF pixel range passed to msdf-atlas-gen.",
    )
    args = parser.parse_args()

    msdf_atlas_gen_path = Path(args.msdf_atlas_gen) if args.msdf_atlas_gen else default_msdf_atlas_gen_path()
    metrics_from = Path(args.metrics_from) if args.metrics_from else None
    generate_reference_msdf(
        Path(args.ttf),
        Path(args.out),
        msdf_atlas_gen_path,
        metrics_from,
        args.keep_temp,
        args.px_range,
    )
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
