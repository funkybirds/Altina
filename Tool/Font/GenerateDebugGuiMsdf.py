from __future__ import annotations

import argparse
import math
import re
import struct
from dataclasses import dataclass
from pathlib import Path


FIRST_CHAR = 32
LAST_CHAR = 126
CELL_W = 64
CELL_H = 64
PX_RANGE = 4.0
PADDING = 1.5
TARGET_DRAW_W = 7.0
TARGET_DRAW_H = 11.0


class TtfError(RuntimeError):
    pass


@dataclass
class Point:
    x: float
    y: float
    on_curve: bool


@dataclass
class Segment:
    x0: float
    y0: float
    x1: float
    y1: float
    channel: int


class Reader:
    def __init__(self, data: bytes):
        self.data = data

    def check(self, offset: int, size: int) -> None:
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise TtfError(f"out-of-range read offset={offset} size={size}")

    def u16(self, offset: int) -> int:
        self.check(offset, 2)
        return struct.unpack_from(">H", self.data, offset)[0]

    def i16(self, offset: int) -> int:
        self.check(offset, 2)
        return struct.unpack_from(">h", self.data, offset)[0]

    def u32(self, offset: int) -> int:
        self.check(offset, 4)
        return struct.unpack_from(">I", self.data, offset)[0]

    def bytes(self, offset: int, size: int) -> bytes:
        self.check(offset, size)
        return self.data[offset:offset + size]


class TtfFont:
    def __init__(self, data: bytes):
        self.r = Reader(data)
        self.tables: dict[str, tuple[int, int]] = {}
        self.units_per_em = 0
        self.num_glyphs = 0
        self.index_to_loca_format = 0
        self.ascender = 0
        self.descender = 0
        self.number_of_hmetrics = 0
        self.advance_widths: list[int] = []
        self.left_side_bearings: list[int] = []
        self.glyph_offsets: list[int] = []
        self.cmap4: dict[int, int] = {}
        self._parse()

    def _table(self, tag: str) -> tuple[int, int]:
        if tag not in self.tables:
            raise TtfError(f"missing table {tag}")
        return self.tables[tag]

    def _parse(self) -> None:
        sfnt = self.r.u32(0)
        if sfnt not in (0x00010000, 0x4F54544F):
            raise TtfError("unsupported sfnt header")
        num_tables = self.r.u16(4)
        table_dir = 12
        for i in range(num_tables):
            rec = table_dir + i * 16
            tag = self.r.bytes(rec, 4).decode("ascii", errors="ignore")
            offset = self.r.u32(rec + 8)
            length = self.r.u32(rec + 12)
            self.r.check(offset, length)
            self.tables[tag] = (offset, length)

        self._parse_head()
        self._parse_maxp()
        self._parse_hhea()
        self._parse_hmtx()
        self._parse_loca()
        self._parse_cmap()

    def _parse_head(self) -> None:
        off, _ = self._table("head")
        self.units_per_em = self.r.u16(off + 18)
        self.index_to_loca_format = self.r.i16(off + 50)

    def _parse_maxp(self) -> None:
        off, _ = self._table("maxp")
        self.num_glyphs = self.r.u16(off + 4)

    def _parse_hhea(self) -> None:
        off, _ = self._table("hhea")
        self.ascender = self.r.i16(off + 4)
        self.descender = self.r.i16(off + 6)
        self.number_of_hmetrics = self.r.u16(off + 34)

    def _parse_hmtx(self) -> None:
        off, _ = self._table("hmtx")
        self.advance_widths = [0] * self.num_glyphs
        self.left_side_bearings = [0] * self.num_glyphs
        pos = off
        for i in range(self.number_of_hmetrics):
            self.advance_widths[i] = self.r.u16(pos)
            self.left_side_bearings[i] = self.r.i16(pos + 2)
            pos += 4
        last_adv = self.advance_widths[self.number_of_hmetrics - 1]
        for i in range(self.number_of_hmetrics, self.num_glyphs):
            self.advance_widths[i] = last_adv
            self.left_side_bearings[i] = self.r.i16(pos)
            pos += 2

    def _parse_loca(self) -> None:
        off, _ = self._table("loca")
        self.glyph_offsets = [0] * (self.num_glyphs + 1)
        if self.index_to_loca_format == 0:
            for i in range(self.num_glyphs + 1):
                self.glyph_offsets[i] = self.r.u16(off + i * 2) * 2
        elif self.index_to_loca_format == 1:
            for i in range(self.num_glyphs + 1):
                self.glyph_offsets[i] = self.r.u32(off + i * 4)
        else:
            raise TtfError("invalid indexToLocFormat")

    def _parse_cmap(self) -> None:
        off, _ = self._table("cmap")
        num_tables = self.r.u16(off + 2)
        best = None
        for i in range(num_tables):
            rec = off + 4 + i * 8
            platform = self.r.u16(rec)
            encoding = self.r.u16(rec + 2)
            sub_off = off + self.r.u32(rec + 4)
            fmt = self.r.u16(sub_off)
            if fmt != 4:
                continue
            score = 0
            if platform == 3 and encoding in (1, 0):
                score = 10
            elif platform == 0:
                score = 8
            else:
                score = 1
            if best is None or score > best[0]:
                best = (score, sub_off)
        if best is None:
            raise TtfError("cmap format 4 not found")
        self.cmap4 = self._parse_cmap_format4(best[1])

    def _parse_cmap_format4(self, off: int) -> dict[int, int]:
        length = self.r.u16(off + 2)
        seg_count = self.r.u16(off + 6) // 2
        end_off = off + 14
        start_off = end_off + seg_count * 2 + 2
        delta_off = start_off + seg_count * 2
        range_off = delta_off + seg_count * 2
        cmap: dict[int, int] = {}
        for i in range(seg_count):
            end_code = self.r.u16(end_off + i * 2)
            start_code = self.r.u16(start_off + i * 2)
            delta = self.r.i16(delta_off + i * 2)
            ro = self.r.u16(range_off + i * 2)
            for code in range(start_code, end_code + 1):
                if code == 0xFFFF:
                    continue
                if ro == 0:
                    gid = (code + delta) & 0xFFFF
                else:
                    glyph_addr = range_off + i * 2 + ro + (code - start_code) * 2
                    if glyph_addr < off or glyph_addr + 2 > off + length:
                        gid = 0
                    else:
                        gid = self.r.u16(glyph_addr)
                        if gid != 0:
                            gid = (gid + delta) & 0xFFFF
                cmap[code] = gid
        return cmap

    def glyph_index(self, codepoint: int) -> int:
        return self.cmap4.get(codepoint, 0)

    def glyph_outline(self, gid: int) -> tuple[list[list[Point]], tuple[int, int, int, int]]:
        return self._glyph_outline(gid, 0)

    def _glyph_outline(
        self, gid: int, depth: int) -> tuple[list[list[Point]], tuple[int, int, int, int]]:
        if gid < 0 or gid >= self.num_glyphs:
            return [], (0, 0, 0, 0)
        if depth > 12:
            raise TtfError("composite glyph recursion too deep")
        glyf_off, _ = self._table("glyf")
        start = glyf_off + self.glyph_offsets[gid]
        end = glyf_off + self.glyph_offsets[gid + 1]
        if end <= start:
            return [], (0, 0, 0, 0)

        number_of_contours = self.r.i16(start)
        x_min = self.r.i16(start + 2)
        y_min = self.r.i16(start + 4)
        x_max = self.r.i16(start + 6)
        y_max = self.r.i16(start + 8)

        if number_of_contours < 0:
            contours: list[list[Point]] = []
            pos = start + 10
            more_components = True
            while more_components:
                flags = self.r.u16(pos)
                comp_gid = self.r.u16(pos + 2)
                pos += 4

                if flags & 0x0001:
                    arg1 = self.r.i16(pos)
                    arg2 = self.r.i16(pos + 2)
                    pos += 4
                else:
                    arg1 = struct.unpack("b", self.r.bytes(pos, 1))[0]
                    arg2 = struct.unpack("b", self.r.bytes(pos + 1, 1))[0]
                    pos += 2

                if not (flags & 0x0002):
                    raise TtfError("composite glyph with point matching is not supported in v1")

                tx = float(arg1)
                ty = float(arg2)
                m00 = 1.0
                m01 = 0.0
                m10 = 0.0
                m11 = 1.0
                if flags & 0x0008:
                    s = self.r.i16(pos) / 16384.0
                    pos += 2
                    m00 = s
                    m11 = s
                elif flags & 0x0040:
                    m00 = self.r.i16(pos) / 16384.0
                    m11 = self.r.i16(pos + 2) / 16384.0
                    pos += 4
                elif flags & 0x0080:
                    m00 = self.r.i16(pos) / 16384.0
                    m01 = self.r.i16(pos + 2) / 16384.0
                    m10 = self.r.i16(pos + 4) / 16384.0
                    m11 = self.r.i16(pos + 6) / 16384.0
                    pos += 8

                comp_contours, _ = self._glyph_outline(comp_gid, depth + 1)
                for contour in comp_contours:
                    transformed: list[Point] = []
                    for p in contour:
                        x = p.x * m00 + p.y * m01 + tx
                        y = p.x * m10 + p.y * m11 + ty
                        transformed.append(Point(x, y, p.on_curve))
                    contours.append(transformed)

                more_components = (flags & 0x0020) != 0

            if flags & 0x0100:
                instruction_len = self.r.u16(pos)
                pos += 2 + instruction_len
            return contours, (x_min, y_min, x_max, y_max)
        if number_of_contours == 0:
            return [], (x_min, y_min, x_max, y_max)

        pos = start + 10
        end_pts = [self.r.u16(pos + i * 2) for i in range(number_of_contours)]
        pos += number_of_contours * 2
        instruction_len = self.r.u16(pos)
        pos += 2 + instruction_len

        point_count = end_pts[-1] + 1
        flags: list[int] = []
        while len(flags) < point_count:
            f = self.r.bytes(pos, 1)[0]
            pos += 1
            flags.append(f)
            if f & 0x08:
                repeat = self.r.bytes(pos, 1)[0]
                pos += 1
                flags.extend([f] * repeat)
        flags = flags[:point_count]

        xs = [0] * point_count
        x = 0
        for i, f in enumerate(flags):
            if f & 0x02:
                dx = self.r.bytes(pos, 1)[0]
                pos += 1
                x += dx if (f & 0x10) else -dx
            else:
                if f & 0x10:
                    dx = 0
                else:
                    dx = self.r.i16(pos)
                    pos += 2
                x += dx
            xs[i] = x

        ys = [0] * point_count
        y = 0
        for i, f in enumerate(flags):
            if f & 0x04:
                dy = self.r.bytes(pos, 1)[0]
                pos += 1
                y += dy if (f & 0x20) else -dy
            else:
                if f & 0x20:
                    dy = 0
                else:
                    dy = self.r.i16(pos)
                    pos += 2
                y += dy
            ys[i] = y

        points = [Point(float(xs[i]), float(ys[i]), bool(flags[i] & 0x01)) for i in range(point_count)]
        contours: list[list[Point]] = []
        start_idx = 0
        for end_idx in end_pts:
            contours.append(points[start_idx:end_idx + 1])
            start_idx = end_idx + 1
        return contours, (x_min, y_min, x_max, y_max)


def midpoint(a: Point, b: Point) -> Point:
    return Point((a.x + b.x) * 0.5, (a.y + b.y) * 0.5, True)


def flatten_quad(p0: Point, p1: Point, p2: Point) -> list[tuple[float, float]]:
    chord = math.hypot(p2.x - p0.x, p2.y - p0.y)
    control_dist = math.hypot(p1.x - (p0.x + p2.x) * 0.5, p1.y - (p0.y + p2.y) * 0.5)
    segs = max(4, min(24, int(chord * 0.01 + control_dist * 0.03) + 4))
    out: list[tuple[float, float]] = []
    for i in range(1, segs + 1):
        t = i / segs
        mt = 1.0 - t
        x = mt * mt * p0.x + 2.0 * mt * t * p1.x + t * t * p2.x
        y = mt * mt * p0.y + 2.0 * mt * t * p1.y + t * t * p2.y
        out.append((x, y))
    return out


def contour_to_segments(contour: list[Point]) -> tuple[list[tuple[float, float, float, float]], list[tuple[float, float]]]:
    if not contour:
        return [], []
    expanded: list[Point] = []
    n = len(contour)
    for i in range(n):
        a = contour[i]
        b = contour[(i + 1) % n]
        expanded.append(a)
        if (not a.on_curve) and (not b.on_curve):
            expanded.append(midpoint(a, b))

    if not expanded[0].on_curve:
        last = expanded[-1]
        first = expanded[0]
        m = midpoint(last, first)
        expanded.insert(0, m)

    segs: list[tuple[float, float, float, float]] = []
    poly: list[tuple[float, float]] = [(expanded[0].x, expanded[0].y)]
    i = 0
    while i < len(expanded):
        p0 = expanded[i]
        p1 = expanded[(i + 1) % len(expanded)]
        if p1.on_curve:
            segs.append((p0.x, p0.y, p1.x, p1.y))
            poly.append((p1.x, p1.y))
            i += 1
        else:
            p2 = expanded[(i + 2) % len(expanded)]
            curve_pts = flatten_quad(p0, p1, p2)
            x0, y0 = p0.x, p0.y
            for x1, y1 in curve_pts:
                segs.append((x0, y0, x1, y1))
                poly.append((x1, y1))
                x0, y0 = x1, y1
            i += 2
        if i >= len(expanded):
            break
    return segs, poly


def signed_area(poly: list[tuple[float, float]]) -> float:
    if len(poly) < 3:
        return 0.0
    s = 0.0
    for i in range(len(poly) - 1):
        x0, y0 = poly[i]
        x1, y1 = poly[i + 1]
        s += x0 * y1 - x1 * y0
    return 0.5 * s


def point_in_contours(x: float, y: float, contours: list[list[tuple[float, float]]]) -> bool:
    winding = 0
    for poly in contours:
        for i in range(len(poly) - 1):
            x0, y0 = poly[i]
            x1, y1 = poly[i + 1]
            if y0 <= y:
                if y1 > y and ((x1 - x0) * (y - y0) - (x - x0) * (y1 - y0)) > 0:
                    winding += 1
            else:
                if y1 <= y and ((x1 - x0) * (y - y0) - (x - x0) * (y1 - y0)) < 0:
                    winding -= 1
    return winding != 0


def distance_to_segment(px: float, py: float, x0: float, y0: float, x1: float, y1: float) -> float:
    vx = x1 - x0
    vy = y1 - y0
    wx = px - x0
    wy = py - y0
    vv = vx * vx + vy * vy
    if vv <= 1e-6:
        return math.hypot(px - x0, py - y0)
    t = (wx * vx + wy * vy) / vv
    if t < 0.0:
        t = 0.0
    elif t > 1.0:
        t = 1.0
    cx = x0 + vx * t
    cy = y0 + vy * t
    return math.hypot(px - cx, py - cy)


def color_edges(segs: list[tuple[float, float, float, float]]) -> list[Segment]:
    out: list[Segment] = []
    channel = 0
    for i, (x0, y0, x1, y1) in enumerate(segs):
        out.append(Segment(x0, y0, x1, y1, channel))
        if (i % 2) == 1:
            channel = (channel + 1) % 3
    return out


def encode_distance(d: float) -> int:
    n = 0.5 + 0.5 * (d / PX_RANGE)
    if n < 0.0:
        n = 0.0
    if n > 1.0:
        n = 1.0
    return int(round(n * 255.0))


def build_glyph_msdf(font: TtfFont, gid: int, scale: float, ox: float, baseline: float) -> list[int]:
    contours, _ = font.glyph_outline(gid)
    if not contours:
        return [0] * (CELL_W * CELL_H * 3)

    all_edges: list[Segment] = []
    contour_polys: list[list[tuple[float, float]]] = []
    for c in contours:
        segs, poly = contour_to_segments(c)
        if not segs or not poly:
            continue
        contour_polys.append(poly)
        all_edges.extend(color_edges(segs))

    if not all_edges:
        return [0] * (CELL_W * CELL_H * 3)

    transformed_edges: list[Segment] = []
    transformed_contours: list[list[tuple[float, float]]] = []
    for e in all_edges:
        transformed_edges.append(Segment(
            ox + e.x0 * scale,
            baseline - e.y0 * scale,
            ox + e.x1 * scale,
            baseline - e.y1 * scale,
            e.channel,
        ))
    for poly in contour_polys:
        transformed_contours.append([(ox + x * scale, baseline - y * scale) for (x, y) in poly])

    pixels = [0] * (CELL_W * CELL_H * 3)
    for y in range(CELL_H):
        for x in range(CELL_W):
            px = x + 0.5
            py = y + 0.5
            inside = point_in_contours(px, py, transformed_contours)
            d_ch = [1e9, 1e9, 1e9]
            d_all = 1e9
            for e in transformed_edges:
                d = distance_to_segment(px, py, e.x0, e.y0, e.x1, e.y1)
                if d < d_ch[e.channel]:
                    d_ch[e.channel] = d
                if d < d_all:
                    d_all = d
            for c in range(3):
                if d_ch[c] >= 1e8:
                    d_ch[c] = d_all
                sd = d_ch[c] if inside else -d_ch[c]
                pixels[(y * CELL_W + x) * 3 + c] = encode_distance(sd)
    return pixels


def write_inl(
    out_path: Path,
    glyph_data: list[int],
    advances: list[float],
    bearings_x: list[float],
    bearings_y: list[float],
    nominal_aspect: float,
    recommended_stretch_x: float,
) -> None:
    glyph_count = LAST_CHAR - FIRST_CHAR + 1
    expected = glyph_count * CELL_W * CELL_H * 3
    if len(glyph_data) != expected:
        raise RuntimeError("unexpected glyph payload size")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="ascii", newline="\n") as f:
        f.write("// Generated MSDF 32x32 font atlas for DebugGui.\n")
        f.write("// Generated by Tool/Font/GenerateDebugGuiMsdf.py\n")
        f.write("// Source type: TrueType glyf. Characters: ASCII 32..126.\n\n")
        f.write("#pragma once\n\n")
        f.write("inline auto GetFont32x32MsdfGlyph(u8 ch) noexcept -> const u8* {\n")
        f.write(f"    static constexpr u32 kFirstChar = {FIRST_CHAR}U;\n")
        f.write(f"    static constexpr u32 kLastChar  = {LAST_CHAR}U;\n")
        f.write(f"    static constexpr u32 kGlyphW    = {CELL_W}U;\n")
        f.write(f"    static constexpr u32 kGlyphH    = {CELL_H}U;\n")
        f.write("    static constexpr u32 kGlyphCount = (kLastChar - kFirstChar + 1U);\n")
        f.write("    static constexpr u32 kChannels = 3U;\n")
        f.write("    static constexpr u8 kGlyphData[kGlyphCount * kGlyphW * kGlyphH * kChannels] = {\n")
        for i, b in enumerate(glyph_data):
            if (i % 16) == 0:
                f.write("        ")
            f.write(f"0x{b:02X}")
            if i + 1 != len(glyph_data):
                f.write(", ")
            if (i + 1) % 16 == 0:
                f.write("\n")
        if len(glyph_data) % 16 != 0:
            f.write("\n")
        f.write("    };\n")
        f.write("    if (ch < kFirstChar || ch > kLastChar) {\n")
        f.write("        ch = static_cast<u8>('?');\n")
        f.write("    }\n")
        f.write("    const u32 idx = static_cast<u32>(ch) - kFirstChar;\n")
        f.write("    return &kGlyphData[idx * kGlyphW * kGlyphH * kChannels];\n")
        f.write("}\n\n")

        def write_metric_fn(name: str, values: list[float]) -> None:
            f.write(f"inline auto {name}(u8 ch) noexcept -> f32 {{\n")
            f.write(f"    static constexpr u32 kFirstChar = {FIRST_CHAR}U;\n")
            f.write(f"    static constexpr u32 kLastChar  = {LAST_CHAR}U;\n")
            f.write("    static constexpr u32 kGlyphCount = (kLastChar - kFirstChar + 1U);\n")
            f.write("    static constexpr f32 kValues[kGlyphCount] = {\n")
            for i, v in enumerate(values):
                if (i % 8) == 0:
                    f.write("        ")
                f.write(f"{v:.6f}f")
                if i + 1 != len(values):
                    f.write(", ")
                if (i + 1) % 8 == 0:
                    f.write("\n")
            if len(values) % 8 != 0:
                f.write("\n")
            f.write("    };\n")
            f.write("    if (ch < kFirstChar || ch > kLastChar) {\n")
            f.write("        ch = static_cast<u8>('?');\n")
            f.write("    }\n")
            f.write("    const u32 idx = static_cast<u32>(ch) - kFirstChar;\n")
            f.write("    return kValues[idx];\n")
            f.write("}\n\n")

        write_metric_fn("GetFont32x32GlyphAdvance", advances)
        write_metric_fn("GetFont32x32GlyphBearingX", bearings_x)
        write_metric_fn("GetFont32x32GlyphBearingY", bearings_y)
        f.write("inline auto GetFont32x32RecommendedStretchX() noexcept -> f32 {\n")
        f.write(f"    return {recommended_stretch_x:.6f}f;\n")
        f.write("}\n\n")
        f.write("inline auto GetFont32x32NominalAspect() noexcept -> f32 {\n")
        f.write(f"    return {nominal_aspect:.6f}f;\n")
        f.write("}\n")


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


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate DebugGui MSDF atlas from TTF.")
    parser.add_argument("--ttf", type=str, required=True, help="Input TrueType file.")
    parser.add_argument("--out", type=str, required=True, help="Output .inl path.")
    parser.add_argument(
        "--metrics-from",
        type=str,
        default="",
        help="Optional existing atlas .inl to reuse metrics/stretch/aspect from.",
    )
    parser.add_argument("--debug-grid-out", type=str, default="", help="Optional debug PNG output.")
    args = parser.parse_args()

    ttf_path = Path(args.ttf)
    out_path = Path(args.out)
    data = ttf_path.read_bytes()
    font = TtfFont(data)

    cap_min_y = 1e9
    cap_max_y = -1e9
    cap_width_sum = 0.0
    cap_count = 0
    has_outline = False
    has_caps = False
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        gid = font.glyph_index(code)
        contours, _ = font.glyph_outline(gid)
        glyph_min_x = 1e9
        glyph_max_x = -1e9
        glyph_has = False
        for c in contours:
            for p in c:
                glyph_has = True
                has_outline = True
                if ord("A") <= code <= ord("Z"):
                    has_caps = True
                    if p.y < cap_min_y:
                        cap_min_y = p.y
                    if p.y > cap_max_y:
                        cap_max_y = p.y
                if p.x < glyph_min_x:
                    glyph_min_x = p.x
                if p.x > glyph_max_x:
                    glyph_max_x = p.x
        if glyph_has and ord("A") <= code <= ord("Z"):
            cap_width_sum += (glyph_max_x - glyph_min_x)
            cap_count += 1
    if not has_outline:
        raise TtfError("no outlines found for ASCII range")
    if not has_caps:
        raise TtfError("no cap outlines found for ASCII uppercase range")

    cap_h = cap_max_y - cap_min_y
    if cap_h <= 1.0:
        raise TtfError("invalid cap height")
    if cap_count == 0:
        raise TtfError("invalid cap width sample count")
    cap_w_avg = cap_width_sum / float(cap_count)
    target = float(CELL_H) - 2.0 * PADDING
    scale = target / cap_h
    baseline = PADDING + cap_max_y * scale

    glyph_data: list[int] = []

    draw_scale = TARGET_DRAW_H / cap_h
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        gid = font.glyph_index(code)
        adv = float(font.advance_widths[gid]) if gid < len(font.advance_widths) else 0.0
        advance_px = adv * scale
        ox = (float(CELL_W) - advance_px) * 0.5
        glyph_pixels = build_glyph_msdf(font, gid, scale, ox, baseline)
        glyph_data.extend(glyph_pixels)

    metrics_from = Path(args.metrics_from) if args.metrics_from else None
    if metrics_from is not None and metrics_from.exists():
        advances, bearings_x, bearings_y, nominal_aspect, recommended_stretch_x = load_legacy_metrics(
            metrics_from
        )
    else:
        advances = []
        bearings_x = []
        bearings_y = []
        for code in range(FIRST_CHAR, LAST_CHAR + 1):
            gid = font.glyph_index(code)
            adv = float(font.advance_widths[gid]) if gid < len(font.advance_widths) else 0.0
            lsb = float(font.left_side_bearings[gid]) if gid < len(font.left_side_bearings) else 0.0
            advances.append(adv * draw_scale)
            bearings_x.append(lsb * draw_scale)
            bearings_y.append(float(font.ascender) * draw_scale)

        nominal_aspect = (float(sum(advances)) / len(advances)) / TARGET_DRAW_H
        font_aspect = cap_w_avg / cap_h
        target_aspect = TARGET_DRAW_W / TARGET_DRAW_H
        recommended_stretch_x = target_aspect / font_aspect if font_aspect > 1e-6 else 1.0

    write_inl(
        out_path,
        glyph_data,
        advances,
        bearings_x,
        bearings_y,
        nominal_aspect,
        recommended_stretch_x,
    )
    print(f"Wrote {out_path}")

    if args.debug_grid_out:
        try:
            from PIL import Image, ImageDraw
        except ModuleNotFoundError:
            print("Pillow not found; skip debug grid image.")
            return

        glyph_count = LAST_CHAR - FIRST_CHAR + 1
        cols = 16
        rows = (glyph_count + cols - 1) // cols
        img = Image.new("RGB", (cols * CELL_W, rows * CELL_H), (12, 12, 12))
        draw = ImageDraw.Draw(img)
        for idx in range(glyph_count):
            col = idx % cols
            row = idx // cols
            gx = col * CELL_W
            gy = row * CELL_H
            draw.rectangle((gx, gy, gx + CELL_W - 1, gy + CELL_H - 1), outline=(40, 40, 40))
            src = idx * CELL_W * CELL_H * 3
            for y in range(CELL_H):
                for x in range(CELL_W):
                    p = src + (y * CELL_W + x) * 3
                    img.putpixel((gx + x, gy + y), (glyph_data[p], glyph_data[p + 1], glyph_data[p + 2]))
        out_debug = Path(args.debug_grid_out)
        out_debug.parent.mkdir(parents=True, exist_ok=True)
        img.save(out_debug)
        print(f"Wrote {out_debug}")


if __name__ == "__main__":
    main()
