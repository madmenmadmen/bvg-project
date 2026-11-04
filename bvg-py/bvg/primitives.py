from __future__ import annotations
import struct
from .errors import BvgChunkError


# ---------- palette ----------

def pack_palette(colors):
    out = bytearray(struct.pack("<H", len(colors)))
    for r, g, b, a in colors:
        out += struct.pack("<BBBB", r, g, b, a)
    return bytes(out)


def unpack_palette(raw: bytes):
    if len(raw) < 2:
        raise BvgChunkError("COLR: truncated")
    n, = struct.unpack_from("<H", raw, 0)
    if 2 + n * 4 > len(raw):
        raise BvgChunkError("COLR: truncated palette")
    out = []
    p = 2
    for _ in range(n):
        r, g, b, a = raw[p:p + 4]
        p += 4
        out.append((r, g, b, a))
    return out


# ---------- images ----------

IMG_PNG = 0
IMG_JPEG = 1
IMG_RAW_RGBA = 2


def pack_images(images):
    out = bytearray(struct.pack("<H", len(images)))
    for im in images:
        fmt = int(im["format"])
        w = int(im["width"])
        h = int(im["height"])
        data = bytes(im["data"])
        out += struct.pack("<BHHI", fmt, w, h, len(data))
        out += data
    return bytes(out)


def unpack_images(raw: bytes):
    if len(raw) < 2:
        raise BvgChunkError("IMGS: truncated")
    n, = struct.unpack_from("<H", raw, 0)
    p = 2
    out = []
    for _ in range(n):
        if p + 9 > len(raw):
            raise BvgChunkError("IMGS: truncated header")
        fmt, w, h, dlen = struct.unpack_from("<BHHI", raw, p)
        p += 9
        if p + dlen > len(raw):
            raise BvgChunkError("IMGS: truncated data")
        data = raw[p:p + dlen]
        p += dlen
        out.append(dict(format=fmt, width=w, height=h, data=data))
    return out


# ---------- gradients ----------

GRAD_LINEAR = 0
GRAD_RADIAL = 1
GRAD_CONIC = 2
GRAD_PATTERN = 3

GRAD_NAMES = {0: "linear", 1: "radial", 2: "conic", 3: "pattern"}


def pack_gradients(grads):
    out = bytearray(struct.pack("<H", len(grads)))
    for g in grads:
        kind = g["kind"]
        flags = g.get("flags", 0)
        if g.get("transform"):
            flags |= 1
        out += struct.pack("<BB", kind, flags)

        stops = g["stops"]
        out += struct.pack("<H", len(stops))
        for st in stops:
            off, ci = st[0], st[1]
            out += struct.pack("<fH", off, ci)
            if ci == 0xFFFF:
                r, gg, b, a = st[2]
                out += struct.pack("<BBBB", r, gg, b, a)

        geo = g["geometry"]
        if kind == GRAD_LINEAR:
            out += struct.pack("<ffff", *geo)
        elif kind == GRAD_RADIAL:
            out += struct.pack("<fffff", *geo)
        elif kind == GRAD_CONIC:
            out += struct.pack("<ffff", *geo)
        elif kind == GRAD_PATTERN:
            img_idx = geo[0]
            out += struct.pack("<H", int(img_idx))
            out += struct.pack("<ffff", *geo[1:])
        else:
            raise BvgChunkError(f"unknown gradient kind {kind}")

        if flags & 1:
            out += struct.pack("<ffffff", *g["transform"])
    return bytes(out)


def unpack_gradients(raw: bytes):
    if len(raw) < 2:
        raise BvgChunkError("GRAD: truncated")
    n, = struct.unpack_from("<H", raw, 0)
    p = 2
    out = []
    for _ in range(n):
        if p + 4 > len(raw):
            raise BvgChunkError("GRAD: truncated header")
        kind, flags = struct.unpack_from("<BB", raw, p)
        p += 2
        sc, = struct.unpack_from("<H", raw, p)
        p += 2
        stops = []
        for _ in range(sc):
            if p + 6 > len(raw):
                raise BvgChunkError("GRAD: truncated stop")
            off, ci = struct.unpack_from("<fH", raw, p)
            p += 6
            if ci == 0xFFFF:
                if p + 4 > len(raw):
                    raise BvgChunkError("GRAD: truncated inline")
                r, gg, b, a = raw[p:p + 4]
                p += 4
                stops.append((off, ci, (r, gg, b, a)))
            else:
                stops.append((off, ci))
        if kind == GRAD_LINEAR:
            if p + 16 > len(raw):
                raise BvgChunkError("GRAD: truncated geom")
            geo = struct.unpack_from("<ffff", raw, p)
            p += 16
        elif kind == GRAD_RADIAL:
            if p + 20 > len(raw):
                raise BvgChunkError("GRAD: truncated geom")
            geo = struct.unpack_from("<fffff", raw, p)
            p += 20
        elif kind == GRAD_CONIC:
            if p + 16 > len(raw):
                raise BvgChunkError("GRAD: truncated geom")
            geo = struct.unpack_from("<ffff", raw, p)
            p += 16
        elif kind == GRAD_PATTERN:
            if p + 18 > len(raw):
                raise BvgChunkError("GRAD: truncated geom")
            img_idx, = struct.unpack_from("<H", raw, p)
            p += 2
            rect = struct.unpack_from("<ffff", raw, p)
            p += 16
            geo = (img_idx,) + rect
        else:
            raise BvgChunkError(f"unknown gradient kind {kind}")
        tr = None
        if flags & 1:
            if p + 24 > len(raw):
                raise BvgChunkError("GRAD: truncated transform")
            tr = struct.unpack_from("<ffffff", raw, p)
            p += 24
        out.append(dict(kind=kind, flags=flags, stops=stops,
                        geometry=geo, transform=tr))
    return out


# ---------- styles ----------

def pack_styles(styles):
    out = bytearray(struct.pack("<H", len(styles)))
    for s in styles:
        out += struct.pack("<BHBH",
                           s.get("fill_kind", 0), s.get("fill_idx", 0),
                           s.get("stroke_kind", 0), s.get("stroke_idx", 0))
        out += struct.pack("<fBBf",
                           s.get("stroke_width", 0.0),
                           s.get("cap", 0), s.get("join", 0),
                           s.get("miter_limit", 4.0))
        dash = s.get("dash", [])
        out += struct.pack("<B", len(dash))
        for d in dash:
            out += struct.pack("<f", d)
        out += struct.pack("<BB", s.get("opacity", 255),
                           s.get("blend_mode", 0))
    return bytes(out)


def unpack_styles(raw: bytes):
    if len(raw) < 2:
        raise BvgChunkError("STYL: truncated")
    n, = struct.unpack_from("<H", raw, 0)
    p = 2
    out = []
    for _ in range(n):
        if p + 16 > len(raw):
            raise BvgChunkError("STYL: truncated")
        fk, fi, sk, si = struct.unpack_from("<BHBH", raw, p)
        p += 6
        sw, cap, join, ml = struct.unpack_from("<fBBf", raw, p)
        p += 10
        dc, = struct.unpack_from("<B", raw, p)
        p += 1
        if p + dc * 4 + 2 > len(raw):
            raise BvgChunkError("STYL: truncated dash")
        dash = []
        for _ in range(dc):
            d, = struct.unpack_from("<f", raw, p)
            p += 4
            dash.append(d)
        op, bm = struct.unpack_from("<BB", raw, p)
        p += 2
        out.append(dict(fill_kind=fk, fill_idx=fi,
                        stroke_kind=sk, stroke_idx=si,
                        stroke_width=sw, cap=cap, join=join,
                        miter_limit=ml, dash=dash,
                        opacity=op, blend_mode=bm))
    return out


# ---------- nodes ----------

NODE_GROUP = 0
NODE_RECT = 1
NODE_ELLIPSE = 2
NODE_LINE = 3
NODE_POLYLINE = 4
NODE_POLYGON = 5
NODE_PATH = 6
NODE_TEXT = 7
NODE_IMAGE = 8
NODE_USE = 9
NODE_DEF = 10

NODE_NAMES = {0: "group", 1: "rect", 2: "ellipse", 3: "line",
              4: "polyline", 5: "polygon", 6: "path", 7: "text",
              8: "image", 9: "use", 10: "def"}

NODE_HAS_TRANSFORM = 1


def pack_nodes(nodes):
    out = bytearray()
    for nd in nodes:
        t = nd["type"]
        flags = 0
        if nd.get("transform"):
            flags |= NODE_HAS_TRANSFORM
        style_id = nd.get("style", 0xFFFF)
        depth = nd.get("depth", 0)
        out += struct.pack("<BBHH", t, flags, style_id, depth)
        if flags & NODE_HAS_TRANSFORM:
            out += struct.pack("<ffffff", *nd["transform"])

        if t in (NODE_GROUP, NODE_DEF):
            out += struct.pack("<H", nd.get("child_count", 0))
        elif t == NODE_RECT:
            x, y, w, h = nd["rect"]
            out += struct.pack("<ffff", x, y, w, h)
        elif t == NODE_ELLIPSE:
            out += struct.pack("<ffff", *nd["ellipse"])
        elif t == NODE_LINE:
            out += struct.pack("<ffff", *nd["line"])
        elif t in (NODE_POLYLINE, NODE_POLYGON):
            pts = nd["points"]
            out += struct.pack("<H", len(pts))
            for x, y in pts:
                out += struct.pack("<ff", x, y)
        elif t == NODE_PATH:
            cmds = nd["commands"]
            out += struct.pack("<H", len(cmds))
            for c in cmds:
                out += _pack_path_cmd(c)
        elif t == NODE_TEXT:
            s = nd["text"].encode("utf-8")
            out += struct.pack("<HffH", nd["font"], nd["x"], nd["y"], len(s))
            out += s
        elif t == NODE_IMAGE:
            out += struct.pack("<Hffff", nd["img"], *nd["rect"])
        elif t == NODE_USE:
            out += struct.pack("<Hff", nd["def_idx"], nd["x"], nd["y"])
        else:
            raise BvgChunkError(f"unknown node type {t}")
    return bytes(out)


def unpack_nodes(raw: bytes):
    out = []
    p = 0
    n = len(raw)
    while p < n:
        if p + 6 > n:
            raise BvgChunkError("NODE: truncated header")
        t, flags, style_id, depth = struct.unpack_from("<BBHH", raw, p)
        p += 6
        nd = {"type": t, "style": style_id, "depth": depth}
        if flags & NODE_HAS_TRANSFORM:
            if p + 24 > n:
                raise BvgChunkError("NODE: truncated transform")
            nd["transform"] = struct.unpack_from("<ffffff", raw, p)
            p += 24

        if t in (NODE_GROUP, NODE_DEF):
            if p + 2 > n:
                raise BvgChunkError("NODE: truncated group")
            cc, = struct.unpack_from("<H", raw, p)
            p += 2
            nd["child_count"] = cc
        elif t == NODE_RECT:
            if p + 16 > n:
                raise BvgChunkError("NODE: truncated rect")
            nd["rect"] = struct.unpack_from("<ffff", raw, p)
            p += 16
        elif t == NODE_ELLIPSE:
            if p + 16 > n:
                raise BvgChunkError("NODE: truncated ellipse")
            nd["ellipse"] = struct.unpack_from("<ffff", raw, p)
            p += 16
        elif t == NODE_LINE:
            if p + 16 > n:
                raise BvgChunkError("NODE: truncated line")
            nd["line"] = struct.unpack_from("<ffff", raw, p)
            p += 16
        elif t in (NODE_POLYLINE, NODE_POLYGON):
            if p + 2 > n:
                raise BvgChunkError("NODE: truncated poly header")
            cnt, = struct.unpack_from("<H", raw, p)
            p += 2
            if p + cnt * 8 > n:
                raise BvgChunkError("NODE: truncated points")
            pts = []
            for _ in range(cnt):
                x, y = struct.unpack_from("<ff", raw, p)
                p += 8
                pts.append((x, y))
            nd["points"] = pts
        elif t == NODE_PATH:
            if p + 2 > n:
                raise BvgChunkError("NODE: truncated path header")
            cnt, = struct.unpack_from("<H", raw, p)
            p += 2
            cmds = []
            for _ in range(cnt):
                c, p = _unpack_path_cmd(raw, p)
                cmds.append(c)
            nd["commands"] = cmds
        elif t == NODE_TEXT:
            hdr = struct.calcsize("<HffH")
            if p + hdr > n:
                raise BvgChunkError("NODE: truncated text header")
            font, x, y, slen = struct.unpack_from("<HffH", raw, p)
            p += hdr
            if p + slen > n:
                raise BvgChunkError("NODE: truncated text body")
            nd["font"] = font
            nd["x"] = x
            nd["y"] = y
            nd["text"] = raw[p:p + slen].decode("utf-8")
            p += slen
        elif t == NODE_IMAGE:
            hdr = struct.calcsize("<Hffff")
            if p + hdr > n:
                raise BvgChunkError("NODE: truncated image")
            img, x, y, w, h = struct.unpack_from("<Hffff", raw, p)
            p += hdr
            nd["img"] = img
            nd["rect"] = (x, y, w, h)
        elif t == NODE_USE:
            hdr = struct.calcsize("<Hff")
            if p + hdr > n:
                raise BvgChunkError("NODE: truncated use")
            d, x, y = struct.unpack_from("<Hff", raw, p)
            p += hdr
            nd["def_idx"] = d
            nd["x"] = x
            nd["y"] = y
        else:
            raise BvgChunkError(f"unknown node type {t}")

        out.append(nd)
    return out


def _pack_path_cmd(c):
    op = c[0]
    if op == 0:
        return struct.pack("<Bff", 0, c[1], c[2])
    if op == 1:
        return struct.pack("<Bff", 1, c[1], c[2])
    if op == 2:
        return struct.pack("<Bffff", 2, *c[1:5])
    if op == 3:
        return struct.pack("<Bffffff", 3, *c[1:7])
    if op == 4:
        # arc: rx, ry, rot (float), large_arc, sweep (u8), x, y (float)
        rx, ry, rot, la, sw, x, y = c[1:]
        return struct.pack("<BfffBBff", 4, rx, ry, rot, la, sw, x, y)
    if op == 5:
        return struct.pack("<B", 5)
    raise BvgChunkError(f"unknown path cmd {op}")


def _unpack_path_cmd(raw, p):
    if p + 1 > len(raw):
        raise BvgChunkError("PATH: truncated cmd")
    op, = struct.unpack_from("<B", raw, p)
    p += 1
    if op == 0:
        if p + 8 > len(raw):
            raise BvgChunkError("PATH: truncated move")
        return (0,) + struct.unpack_from("<ff", raw, p), p + 8
    if op == 1:
        if p + 8 > len(raw):
            raise BvgChunkError("PATH: truncated line")
        return (1,) + struct.unpack_from("<ff", raw, p), p + 8
    if op == 2:
        if p + 16 > len(raw):
            raise BvgChunkError("PATH: truncated quad")
        return (2,) + struct.unpack_from("<ffff", raw, p), p + 16
    if op == 3:
        if p + 24 > len(raw):
            raise BvgChunkError("PATH: truncated cubic")
        return (3,) + struct.unpack_from("<ffffff", raw, p), p + 24
    if op == 4:
        if p + 22 > len(raw):
            raise BvgChunkError("PATH: truncated arc")
        rx, ry, rot, la, sw, x, y = struct.unpack_from("<fffBBff", raw, p)
        return (4, rx, ry, rot, la, sw, x, y), p + 22
    if op == 5:
        return (5,), p
    raise BvgChunkError(f"unknown path cmd {op}")