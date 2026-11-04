"""Minimal SVG path parser -> BVG PathCmd tuples.

Returns a list of tuples compatible with primitives._pack_path_cmd:

    (0, x, y)                       Move
    (1, x, y)                       Line
    (2, cx, cy, x, y)               Quad
    (3, c1x, c1y, c2x, c2y, x, y)   Cubic
    (4, rx, ry, rot, la, sw, x, y)  Arc
    (5,)                            Close

Supports absolute and relative commands (M m L l H h V v C c S s Q q T t A a Z z).
Smooth curves (S, s, T, t) are expanded into explicit C/Q with control points.
"""
from __future__ import annotations

import re

_TOKEN_RE = re.compile(r"[MmLlHhVvCcSsQqTtAaZz]|[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")

_CMD_ARGC = {
    "M": 2, "m": 2,
    "L": 2, "l": 2,
    "H": 1, "h": 1,
    "V": 1, "v": 1,
    "C": 6, "c": 6,
    "S": 4, "s": 4,
    "Q": 4, "q": 4,
    "T": 2, "t": 2,
    "A": 7, "a": 7,
    "Z": 0, "z": 0,
}


def _tokenize(d: str) -> list[str]:
    out = []
    i = 0
    n = len(d)
    while i < n:
        c = d[i]
        if c in " \t\r\n,":
            i += 1
            continue
        m = _TOKEN_RE.match(d, i)
        if not m:
            raise ValueError(f"bad SVG path at {i}: {d[i:i+10]!r}")
        out.append(m.group(0))
        i = m.end()
    return out


def parse_svg_path(d: str) -> list[tuple]:
    tokens = _tokenize(d)
    if not tokens:
        return []

    cmds: list[tuple] = []
    i = 0
    n = len(tokens)

    cur_x = cur_y = 0.0
    start_x = start_y = 0.0
    prev_cmd = ""
    # For smooth curves we need the previous control point.
    prev_c2x = prev_c2y = None   # for S/s
    prev_qx = prev_qy = None     # for T/t

    def num():
        nonlocal i
        v = float(tokens[i])
        i += 1
        return v

    def flush_move(rel_x, rel_y, is_rel):
        nonlocal cur_x, cur_y, start_x, start_y
        if is_rel:
            cur_x += rel_x
            cur_y += rel_y
        else:
            cur_x = rel_x
            cur_y = rel_y
        start_x, start_y = cur_x, cur_y

    while i < n:
        tok = tokens[i]
        if tok in _CMD_ARGC:
            cmd = tok
            i += 1
        else:
            # implicit repeat of previous command
            if not prev_cmd:
                raise ValueError(f"path must start with a command, got {tok!r}")
            cmd = prev_cmd

        if cmd in "Zz":
            cmds.append((5,))
            cur_x, cur_y = start_x, start_y
            prev_c2x = prev_c2y = None
            prev_qx = prev_qy = None
            prev_cmd = cmd
            continue

        argc = _CMD_ARGC[cmd]
        if i + argc > n:
            raise ValueError(f"truncated args for {cmd!r}")

        if cmd in "Mm":
            x = num()
            y = num()
            is_rel = cmd == "m"
            flush_move(x, y, is_rel)
            cmds.append((0, cur_x, cur_y))
            # subsequent pairs are implicit L/l
            prev_cmd = "L" if not is_rel else "l"
            prev_c2x = prev_c2y = None
            prev_qx = prev_qy = None
        elif cmd in "Ll":
            x = num()
            y = num()
            if cmd == "l":
                cur_x += x; cur_y += y
            else:
                cur_x, cur_y = x, y
            cmds.append((1, cur_x, cur_y))
            prev_c2x = prev_c2y = None
            prev_qx = prev_qy = None
            prev_cmd = cmd
        elif cmd in "Hh":
            x = num()
            if cmd == "h":
                cur_x += x
            else:
                cur_x = x
            cmds.append((1, cur_x, cur_y))
            prev_c2x = prev_c2y = None
            prev_qx = prev_qy = None
            prev_cmd = cmd
        elif cmd in "Vv":
            y = num()
            if cmd == "v":
                cur_y += y
            else:
                cur_y = y
            cmds.append((1, cur_x, cur_y))
            prev_c2x = prev_c2y = None
            prev_qx = prev_qy = None
            prev_cmd = cmd
        elif cmd in "Cc":
            c1x = num(); c1y = num()
            c2x = num(); c2y = num()
            x = num(); y = num()
            if cmd == "c":
                c1x += cur_x; c1y += cur_y
                c2x += cur_x; c2y += cur_y
                x += cur_x; y += cur_y
            cmds.append((3, c1x, c1y, c2x, c2y, x, y))
            prev_c2x, prev_c2y = c2x, c2y
            prev_qx = prev_qy = None
            cur_x, cur_y = x, y
            prev_cmd = cmd
        elif cmd in "Ss":
            c2x = num(); c2y = num()
            x = num(); y = num()
            if cmd == "s":
                c2x += cur_x; c2y += cur_y
                x += cur_x; y += cur_y
            if prev_c2x is not None:
                c1x = 2 * cur_x - prev_c2x
                c1y = 2 * cur_y - prev_c2y
            else:
                c1x, c1y = cur_x, cur_y
            cmds.append((3, c1x, c1y, c2x, c2y, x, y))
            prev_c2x, prev_c2y = c2x, c2y
            prev_qx = prev_qy = None
            cur_x, cur_y = x, y
            prev_cmd = cmd
        elif cmd in "Qq":
            qx = num(); qy = num()
            x = num(); y = num()
            if cmd == "q":
                qx += cur_x; qy += cur_y
                x += cur_x; y += cur_y
            cmds.append((2, qx, qy, x, y))
            prev_qx, prev_qy = qx, qy
            prev_c2x = prev_c2y = None
            cur_x, cur_y = x, y
            prev_cmd = cmd
        elif cmd in "Tt":
            x = num(); y = num()
            if cmd == "t":
                x += cur_x; y += cur_y
            if prev_qx is not None:
                qx = 2 * cur_x - prev_qx
                qy = 2 * cur_y - prev_qy
            else:
                qx, qy = cur_x, cur_y
            cmds.append((2, qx, qy, x, y))
            prev_qx, prev_qy = qx, qy
            prev_c2x = prev_c2y = None
            cur_x, cur_y = x, y
            prev_cmd = cmd
        elif cmd in "Aa":
            rx = num(); ry = num()
            rot_deg = num()
            la = int(num())
            sw = int(num())
            x = num(); y = num()
            if la not in (0, 1) or sw not in (0, 1):
                raise ValueError(f"arc flags must be 0 or 1, got {la},{sw}")
            if cmd == "a":
                x += cur_x; y += cur_y
            # BVG stores rot in radians (SPEC 12.6)
            rot = rot_deg * 3.141592653589793 / 180.0
            cmds.append((4, rx, ry, rot, la, sw, x, y))
            prev_c2x = prev_c2y = None
            prev_qx = prev_qy = None
            cur_x, cur_y = x, y
            prev_cmd = cmd
        else:
            raise ValueError(f"unsupported command {cmd!r}")

    return cmds