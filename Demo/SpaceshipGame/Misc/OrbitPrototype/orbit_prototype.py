#!/usr/bin/env python3
"""
2D orbit prototype for SpaceshipGame.

- Bodies are static.
- Orbits lie on the XZ plane; we visualize (x, z).
- States: EarthOrbit / Transfer / MoonOrbit
- State switches are allowed only near join points (distance threshold).

Controls:
  2: request EarthOrbit (only from Transfer at JoinEarth)
  3: request Transfer   (only from EarthOrbit at JoinEarth, or from MoonOrbit at JoinMoon)
  4: request MoonOrbit  (only from Transfer at JoinMoon)
  Q/E: increase/decrease time scale
  Space: pause/resume
  Esc: close
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from enum import Enum

import tkinter as tk

import bisect


class ShipState(Enum):
    EARTH_ORBIT = 0
    TRANSFER = 1
    MOON_ORBIT = 2


@dataclass
class Constants:
    # Match the demo scale (1 unit = 10,000 km).
    earth_moon_distance: float = 38.44
    sun_earth_distance: float = 400.0
    earth_orbit_radius: float = 1.2
    moon_orbit_radius: float = 0.55

    orbit_switch_epsilon: float = 0.15

    # Angular speeds (rad/sec), tuned for readability (match demo defaults).
    earth_orbit_omega: float = 0.9
    moon_orbit_omega: float = 1.8
    transfer_omega: float = 0.35


def normalize2(v: tuple[float, float]) -> tuple[float, float]:
    n = math.hypot(v[0], v[1])
    if n < 1e-8:
        return (1.0, 0.0)
    return (v[0] / n, v[1] / n)


def perp_left(v: tuple[float, float]) -> tuple[float, float]:
    return (-v[1], v[0])


def add(a: tuple[float, float], b: tuple[float, float]) -> tuple[float, float]:
    return (a[0] + b[0], a[1] + b[1])


def sub(a: tuple[float, float], b: tuple[float, float]) -> tuple[float, float]:
    return (a[0] - b[0], a[1] - b[1])


def mul(v: tuple[float, float], s: float) -> tuple[float, float]:
    return (v[0] * s, v[1] * s)


def dist(a: tuple[float, float], b: tuple[float, float]) -> float:
    d = sub(a, b)
    return math.hypot(d[0], d[1])


def eval_transfer_pos(
    earth: tuple[float, float],
    axis_x: tuple[float, float],
    axis_z: tuple[float, float],
    theta: float,
    r1: float,
    r2: float,
) -> tuple[float, float]:
    a = 0.5 * (r1 + r2)
    e = (r2 - r1) / (r2 + r1)
    p = a * (1.0 - e * e)
    r = p / (1.0 + e * math.cos(theta))

    phi = theta + math.pi
    c = math.cos(phi)
    s = math.sin(phi)
    return add(earth, add(mul(axis_x, r * c), mul(axis_z, r * s)))


def eval_circle_pos(
    center: tuple[float, float],
    axis_x: tuple[float, float],
    axis_z: tuple[float, float],
    radius: float,
    phase: float,
) -> tuple[float, float]:
    c = math.cos(phase)
    s = math.sin(phase)
    return add(center, add(mul(axis_x, radius * c), mul(axis_z, radius * s)))


class Sim:
    def __init__(self, c: Constants) -> None:
        self.c = c
        self.state = ShipState.EARTH_ORBIT
        self.phase = 0.0
        self.theta = 0.0
        self.transfer_s01 = 0.0  # normalized arc-length progress on transfer [0,1)
        self.time_scale = 1.0
        self.paused = False

        self.earth = (c.sun_earth_distance, 0.0)
        self.moon = add(self.earth, (c.earth_moon_distance, 0.0))

        # Precompute arc-length reparameterization for transfer so the ship moves at (approx)
        # constant speed along the ellipse in screen space.
        self._lut_theta: list[float] = []
        self._lut_s01: list[float] = []
        self._s_at_theta0: float = 0.0
        self._s_at_thetapi: float = 0.5
        self._build_transfer_lut()

    def axis_basis(self) -> tuple[tuple[float, float], tuple[float, float]]:
        axis_x = normalize2(sub(self.moon, self.earth))
        axis_z = perp_left(axis_x)
        return axis_x, axis_z

    def join_points(self) -> tuple[tuple[float, float], tuple[float, float]]:
        axis_x, _axis_z = self.axis_basis()
        join_earth = add(self.earth, mul(axis_x, -self.c.earth_orbit_radius))
        join_moon = add(self.moon, mul(axis_x, -self.c.moon_orbit_radius))
        return join_earth, join_moon

    def ship_pos(self) -> tuple[float, float]:
        axis_x, axis_z = self.axis_basis()
        if self.state == ShipState.EARTH_ORBIT:
            return eval_circle_pos(self.earth, axis_x, axis_z, self.c.earth_orbit_radius, self.phase)
        if self.state == ShipState.MOON_ORBIT:
            return eval_circle_pos(self.moon, axis_x, axis_z, self.c.moon_orbit_radius, self.phase)
        # TRANSFER
        r1 = self.c.earth_orbit_radius
        r2 = self.c.earth_moon_distance - self.c.moon_orbit_radius
        return eval_transfer_pos(self.earth, axis_x, axis_z, self.theta, r1, r2)

    def can_switch_earth(self, ship: tuple[float, float]) -> bool:
        join_earth, _join_moon = self.join_points()
        return dist(ship, join_earth) <= self.c.orbit_switch_epsilon

    def can_switch_moon(self, ship: tuple[float, float]) -> bool:
        _join_earth, join_moon = self.join_points()
        return dist(ship, join_moon) <= self.c.orbit_switch_epsilon

    def request_state(self, desired: ShipState) -> None:
        ship = self.ship_pos()
        if desired == ShipState.EARTH_ORBIT:
            if self.state == ShipState.TRANSFER and self.can_switch_earth(ship):
                self.state = ShipState.EARTH_ORBIT
                self.phase = math.pi  # JoinEarth.
        elif desired == ShipState.TRANSFER:
            if self.state == ShipState.EARTH_ORBIT and self.can_switch_earth(ship):
                self.state = ShipState.TRANSFER
                self.transfer_s01 = self._s_at_theta0
                self.theta = 0.0  # JoinEarth.
            elif self.state == ShipState.MOON_ORBIT and self.can_switch_moon(ship):
                self.state = ShipState.TRANSFER
                self.transfer_s01 = self._s_at_thetapi
                self.theta = math.pi  # JoinMoon.
        elif desired == ShipState.MOON_ORBIT:
            if self.state == ShipState.TRANSFER and self.can_switch_moon(ship):
                self.state = ShipState.MOON_ORBIT
                self.phase = math.pi  # JoinMoon.

    def step(self, dt: float) -> None:
        if self.paused:
            return
        sdt = dt * self.time_scale
        if self.state == ShipState.EARTH_ORBIT:
            self.phase = (self.phase + sdt * self.c.earth_orbit_omega + math.pi) % (2.0 * math.pi) - math.pi
        elif self.state == ShipState.MOON_ORBIT:
            # Reverse direction for tangent continuity at JoinMoon.
            self.phase = (self.phase - sdt * self.c.moon_orbit_omega + math.pi) % (2.0 * math.pi) - math.pi
        else:
            # Advance by normalized arc-length fraction so screen-space motion looks uniform.
            frac_per_sec = self.c.transfer_omega / (2.0 * math.pi)
            self.transfer_s01 = (self.transfer_s01 + sdt * frac_per_sec) % 1.0
            self.theta = self._theta_from_s01(self.transfer_s01)

    def _theta_from_s01(self, s01: float) -> float:
        s01 = s01 % 1.0
        i = bisect.bisect_left(self._lut_s01, s01)
        if i <= 0:
            return self._lut_theta[0]
        if i >= len(self._lut_s01):
            return self._lut_theta[-1]

        s0 = self._lut_s01[i - 1]
        s1 = self._lut_s01[i]
        t0 = self._lut_theta[i - 1]
        t1 = self._lut_theta[i]
        if s1 - s0 < 1e-8:
            return t1
        a = (s01 - s0) / (s1 - s0)
        return t0 + (t1 - t0) * a

    def _build_transfer_lut(self) -> None:
        axis_x, axis_z = self.axis_basis()
        r1 = self.c.earth_orbit_radius
        r2 = self.c.earth_moon_distance - self.c.moon_orbit_radius

        n = 4096
        thetas: list[float] = []
        pts: list[tuple[float, float]] = []
        for i in range(n + 1):
            theta = (2.0 * math.pi) * (i / n)
            thetas.append(theta)
            pts.append(eval_transfer_pos(self.earth, axis_x, axis_z, theta, r1, r2))

        cum: list[float] = [0.0]
        total = 0.0
        for i in range(1, len(pts)):
            total += dist(pts[i], pts[i - 1])
            cum.append(total)

        if total < 1e-6:
            self._lut_theta = thetas
            self._lut_s01 = [0.0 for _ in thetas]
            self._s_at_theta0 = 0.0
            self._s_at_thetapi = 0.5
            return

        self._lut_theta = thetas
        self._lut_s01 = [v / total for v in cum]

        # Cache exact-ish s positions for theta=0 and theta=pi for switching alignment.
        # theta=0 is the first element by construction.
        self._s_at_theta0 = self._lut_s01[0]
        # Find the sample closest to pi.
        idx_pi = int(round((n / 2)))
        self._s_at_thetapi = self._lut_s01[idx_pi]


class View2D:
    def __init__(self, root: tk.Tk, sim: Sim) -> None:
        self.sim = sim

        self.w = 1100
        self.h = 560
        self.pad = 30
        self.canvas = tk.Canvas(root, width=self.w, height=self.h, bg="#101215")
        self.canvas.pack(fill=tk.BOTH, expand=False)

        # Pre-sample orbits for drawing and robust bounds (transfer ellipse can have larger Z
        # extent than the small circular orbits).
        axis_x, axis_z = self.sim.axis_basis()
        c = self.sim.c

        self._earth_orbit_pts: list[tuple[float, float]] = []
        self._moon_orbit_pts: list[tuple[float, float]] = []
        self._transfer_pts: list[tuple[float, float]] = []

        for i in range(720):
            phase = (2.0 * math.pi) * (i / 719.0)
            self._earth_orbit_pts.append(
                eval_circle_pos(self.sim.earth, axis_x, axis_z, c.earth_orbit_radius, phase)
            )
            self._moon_orbit_pts.append(
                eval_circle_pos(self.sim.moon, axis_x, axis_z, c.moon_orbit_radius, phase)
            )

        r1 = c.earth_orbit_radius
        r2 = c.earth_moon_distance - c.moon_orbit_radius
        for i in range(1200):
            theta = -math.pi + (2.0 * math.pi) * (i / 1199.0)
            self._transfer_pts.append(eval_transfer_pos(self.sim.earth, axis_x, axis_z, theta, r1, r2))

        all_pts = (
            self._earth_orbit_pts
            + self._moon_orbit_pts
            + self._transfer_pts
            + [self.sim.earth, self.sim.moon]
        )
        xs = [p[0] for p in all_pts]
        zs = [p[1] for p in all_pts]
        margin = 1.0
        self.world_min = (min(xs) - margin, min(zs) - margin)
        self.world_max = (max(xs) + margin, max(zs) + margin)

        self._static_drawn = False
        self._ship_item = 0
        self._join_e_item = 0
        self._join_m_item = 0
        self._text_item = 0

    def _world_to_canvas(self, p: tuple[float, float]) -> tuple[float, float]:
        # Map world (x,z) -> canvas (cx,cy), with Y down.
        x0, z0 = self.world_min
        x1, z1 = self.world_max

        sx = (self.w - 2 * self.pad) / max(1e-6, (x1 - x0))
        sz = (self.h - 2 * self.pad) / max(1e-6, (z1 - z0))
        s = min(sx, sz)  # keep geometry undistorted

        # Center the smaller axis so the view uses the full canvas area.
        view_w = (x1 - x0) * s
        view_h = (z1 - z0) * s
        off_x = (self.w - 2 * self.pad - view_w) * 0.5
        off_y = (self.h - 2 * self.pad - view_h) * 0.5

        cx = self.pad + off_x + (p[0] - x0) * s
        cy = self.h - self.pad - off_y - (p[1] - z0) * s
        return (cx, cy)

    def _draw_circle(self, center: tuple[float, float], radius: float, outline: str, width: int) -> None:
        c = self._world_to_canvas(center)

        x0, _z0 = self.world_min
        x1, _z1 = self.world_max
        sx = (self.w - 2 * self.pad) / max(1e-6, (x1 - x0))
        sz = (self.h - 2 * self.pad) / max(1e-6, (self.world_max[1] - self.world_min[1]))
        s = min(sx, sz)
        rr = radius * s

        self.canvas.create_oval(c[0] - rr, c[1] - rr, c[0] + rr, c[1] + rr, outline=outline, width=width)

    def _draw_polyline(self, pts: list[tuple[float, float]], color: str, width: int) -> None:
        if len(pts) < 2:
            return
        flat: list[float] = []
        for p in pts:
            c = self._world_to_canvas(p)
            flat.append(c[0])
            flat.append(c[1])
        self.canvas.create_line(*flat, fill=color, width=width)

    def draw_static(self) -> None:
        if self._static_drawn:
            return
        self._static_drawn = True

        join_e, join_m = self.sim.join_points()
        c = self.sim.c

        # Orbits.
        self._draw_polyline(self._earth_orbit_pts, color="#4C78A8", width=2)
        self._draw_polyline(self._transfer_pts, color="#F58518", width=2)
        self._draw_polyline(self._moon_orbit_pts, color="#54A24B", width=2)

        # Bodies.
        earth_c = self._world_to_canvas(self.sim.earth)
        moon_c = self._world_to_canvas(self.sim.moon)
        self.canvas.create_oval(earth_c[0] - 8, earth_c[1] - 8, earth_c[0] + 8, earth_c[1] + 8, fill="#1F77B4", outline="")
        self.canvas.create_oval(moon_c[0] - 6, moon_c[1] - 6, moon_c[0] + 6, moon_c[1] + 6, fill="#888888", outline="")

        # Join points (dynamic color updated per-frame).
        je = self._world_to_canvas(join_e)
        jm = self._world_to_canvas(join_m)
        self._join_e_item = self.canvas.create_oval(je[0] - 6, je[1] - 6, je[0] + 6, je[1] + 6, fill="#AA3377", outline="")
        self._join_m_item = self.canvas.create_oval(jm[0] - 6, jm[1] - 6, jm[0] + 6, jm[1] + 6, fill="#AA3377", outline="")

        # Ship (dynamic).
        ship = self.sim.ship_pos()
        sc = self._world_to_canvas(ship)
        self._ship_item = self.canvas.create_oval(sc[0] - 5, sc[1] - 5, sc[0] + 5, sc[1] + 5, fill="white", outline="black")

        self._text_item = self.canvas.create_text(
            12,
            12,
            anchor="nw",
            fill="#DDDDDD",
            font=("Consolas", 12),
            text="",
        )

    def draw_frame(self) -> None:
        self.draw_static()

        ship = self.sim.ship_pos()
        sc = self._world_to_canvas(ship)
        self.canvas.coords(self._ship_item, sc[0] - 5, sc[1] - 5, sc[0] + 5, sc[1] + 5)

        can_e = self.sim.can_switch_earth(ship)
        can_m = self.sim.can_switch_moon(ship)
        self.canvas.itemconfig(self._join_e_item, fill=("#22AA22" if can_e else "#AA3377"))
        self.canvas.itemconfig(self._join_m_item, fill=("#22AA22" if can_m else "#AA3377"))

        self.canvas.itemconfig(
            self._text_item,
            text=(
                f"state={self.sim.state.name}\n"
                f"timeScale={self.sim.time_scale:.2f} paused={self.sim.paused}\n"
                f"canSwitch@JoinEarth={can_e}  canSwitch@JoinMoon={can_m}\n"
                "keys: 2/3/4 switch, Q/E timeScale, Space pause, Esc quit"
            ),
        )


def main() -> None:
    c = Constants()
    sim = Sim(c)

    root = tk.Tk()
    root.title("Altina Spaceship Orbits Prototype (XZ plane)")

    view = View2D(root, sim)

    def on_key(event) -> None:
        k = event.keysym
        if k == "2":
            sim.request_state(ShipState.EARTH_ORBIT)
        elif k == "3":
            sim.request_state(ShipState.TRANSFER)
        elif k == "4":
            sim.request_state(ShipState.MOON_ORBIT)
        elif k in ("q", "Q"):
            sim.time_scale = min(6.0, sim.time_scale + 0.2)
        elif k in ("e", "E"):
            sim.time_scale = max(0.1, sim.time_scale - 0.2)
        elif k == "space":
            sim.paused = not sim.paused
        elif k == "Escape":
            root.destroy()

    root.bind("<KeyPress>", on_key)

    def tick() -> None:
        sim.step(1.0 / 60.0)
        view.draw_frame()
        root.after(16, tick)

    tick()
    root.mainloop()


if __name__ == "__main__":
    main()
