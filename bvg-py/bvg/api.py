from __future__ import annotations
from pathlib import Path
from .container import BvgFile
from . import primitives as P
from . import codec as C
from .errors import BvgError


def parse_color(c):
    if c is None:
        return (0, 0, 0, 0)
    if isinstance(c, int):
        return ((c >> 24) & 0xFF, (c >> 16) & 0xFF,
                (c >> 8) & 0xFF, c & 0xFF)
    if isinstance(c, tuple):
        if len(c) == 3:
            return (int(c[0]), int(c[1]), int(c[2]), 255)
        if len(c) == 4:
            return tuple(int(x) for x in c)
        raise ValueError(f"bad color tuple: {c!r}")
    if isinstance(c, str):
        s = c.strip()
        if s.startswith("#"):
            s = s[1:]
        if len(s) == 3:
            s = "".join(ch * 2 for ch in s)
        if len(s) == 6:
            return (int(s[0:2], 16), int(s[2:4], 16),
                    int(s[4:6], 16), 255)
        if len(s) == 8:
            return (int(s[0:2], 16), int(s[2:4], 16),
                    int(s[4:6], 16), int(s[6:8], 16))
    raise ValueError(f"bad color: {c!r}")


class _Gradient:
    __slots__ = ("kind", "stops", "geometry", "transform", "flags")

    def __init__(self, kind, stops, geometry, flags=0):
        self.kind = kind
        self.stops = stops
        self.geometry = geometry
        self.transform = None
        self.flags = flags

    def to_dict(self):
        return dict(kind=self.kind, flags=self.flags, stops=self.stops,
                    geometry=self.geometry, transform=self.transform)


def _stops(stops):
    out = []
    for item in stops:
        off, color = item[0], item[1]
        rgba = parse_color(color)
        out.append((float(off), 0xFFFF, rgba))
    return out


def linear(x1, y1, x2, y2, *stops):
    return _Gradient(P.GRAD_LINEAR, _stops(stops), (x1, y1, x2, y2))


def radial(cx, cy, r, *stops, fx=None, fy=None):
    fx = cx if fx is None else fx
    fy = cy if fy is None else fy
    return _Gradient(P.GRAD_RADIAL, _stops(stops), (cx, cy, r, fx, fy))


def conic(cx, cy, start_angle, end_angle, *stops):
    return _Gradient(P.GRAD_CONIC, _stops(stops),
                     (cx, cy, start_angle, end_angle))


def parse_path(d: str):
    from .path_parser import parse_svg_path
    return parse_svg_path(d)

class Document:
    def __init__(self, width, height):
        self.width = int(width)
        self.height = int(height)
        self._palette = []
        self._color_idx = {}
        self._gradients = []
        self._grad_idx = {}
        self._styles = []
        self._style_idx = {}
        self._nodes = []
        self._images = []
        self._depth_stack = []

    def _color(self, c):
        rgba = parse_color(c)
        if rgba not in self._color_idx:
            self._color_idx[rgba] = len(self._palette)
            self._palette.append(rgba)
        return self._color_idx[rgba]

    def _gradient(self, g):
        d = g.to_dict()
        key = (d["kind"], d["flags"],
               tuple((round(s[0], 6), s[1]) +
                     ((s[2],) if len(s) > 2 else ())
                     for s in d["stops"]),
               tuple(round(v, 6) for v in d["geometry"]),
               tuple(d["transform"]) if d["transform"] else None)
        if key not in self._grad_idx:
            self._grad_idx[key] = len(self._gradients)
            self._gradients.append(d)
        return self._grad_idx[key]

    def _style(self, fill=None, stroke=None, stroke_width=1.0,
               opacity=1.0, cap=0, join=0, dash=None, blend=0,
               miter_limit=4.0):
        if isinstance(fill, _Gradient):
            fk, fi = 2, self._gradient(fill)
        elif fill is None:
            fk, fi = 0, 0
        else:
            fk, fi = 1, self._color(fill)
        if isinstance(stroke, _Gradient):
            sk, si = 2, self._gradient(stroke)
        elif stroke is None:
            sk, si = 0, 0
        else:
            sk, si = 1, self._color(stroke)
        s = dict(fill_kind=fk, fill_idx=fi,
                 stroke_kind=sk, stroke_idx=si,
                 stroke_width=float(stroke_width) if sk else 0.0,
                 cap=int(cap), join=int(join),
                 miter_limit=float(miter_limit),
                 dash=[float(x) for x in (dash or [])],
                 opacity=int(round(opacity * 255)),
                 blend_mode=int(blend))
        key = (fk, fi, sk, si, round(s["stroke_width"], 6),
               s["cap"], s["join"], tuple(s["dash"]),
               round(s["miter_limit"], 6),
               s["opacity"], s["blend_mode"])
        if key not in self._style_idx:
            self._style_idx[key] = len(self._styles)
            self._styles.append(s)
        return self._style_idx[key]

    def _add(self, nd):
        nd.setdefault("depth",
                      self._depth_stack[-1] if self._depth_stack else 0)
        self._nodes.append(nd)
        return self

    def _current_depth(self):
        return self._depth_stack[-1] if self._depth_stack else 0

    def rect(self, x, y, w, h, *, fill=None, stroke=None,
             stroke_width=1.0, opacity=1.0, transform=None, **kw):
        sid = self._style(fill, stroke, stroke_width, opacity, **kw)
        nd = dict(type=P.NODE_RECT,
                  rect=(float(x), float(y), float(w), float(h)),
                  style=sid)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def circle(self, cx, cy, r, *, fill=None, stroke=None,
               stroke_width=1.0, opacity=1.0, **kw):
        return self.ellipse(cx, cy, r, r, fill=fill, stroke=stroke,
                            stroke_width=stroke_width,
                            opacity=opacity, **kw)

    def ellipse(self, cx, cy, rx, ry, *, fill=None, stroke=None,
                stroke_width=1.0, opacity=1.0, transform=None, **kw):
        sid = self._style(fill, stroke, stroke_width, opacity, **kw)
        nd = dict(type=P.NODE_ELLIPSE,
                  ellipse=(float(cx), float(cy),
                           float(rx), float(ry)),
                  style=sid)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def line(self, x1, y1, x2, y2, *, stroke=None, stroke_width=1.0,
             opacity=1.0, transform=None, **kw):
        sid = self._style(None, stroke, stroke_width, opacity, **kw)
        nd = dict(type=P.NODE_LINE,
                  line=(float(x1), float(y1),
                        float(x2), float(y2)),
                  style=sid)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def polyline(self, points, *, fill=None, stroke=None,
                 stroke_width=1.0, opacity=1.0, transform=None, **kw):
        sid = self._style(fill, stroke, stroke_width, opacity, **kw)
        nd = dict(type=P.NODE_POLYLINE,
                  points=[(float(x), float(y)) for x, y in points],
                  style=sid)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def polygon(self, points, *, fill=None, stroke=None,
                stroke_width=1.0, opacity=1.0, transform=None, **kw):
        sid = self._style(fill, stroke, stroke_width, opacity, **kw)
        nd = dict(type=P.NODE_POLYGON,
                  points=[(float(x), float(y)) for x, y in points],
                  style=sid)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def path(self, d, *, fill=None, stroke=None,
             stroke_width=1.0, opacity=1.0, transform=None, **kw):
        cmds = parse_path(d) if isinstance(d, str) else list(d)
        sid = self._style(fill, stroke, stroke_width, opacity, **kw)
        nd = dict(type=P.NODE_PATH, commands=cmds, style=sid)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def text(self, text, x, y, *, fill="#000", stroke=None,
             stroke_width=1.0, opacity=1.0, transform=None, **kw):
        sid = self._style(fill, stroke, stroke_width, opacity, **kw)
        nd = dict(type=P.NODE_TEXT, font=0,
                  x=float(x), y=float(y), text=text, style=sid)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def image(self, data: bytes, x, y, w, h, *,
              fmt=P.IMG_PNG, iw=None, ih=None, transform=None):
        if fmt == P.IMG_RAW_RGBA:
            if iw is None or ih is None:
                raise BvgError("raw RGBA needs iw/ih")
            iw, ih = int(iw), int(ih)
            if len(data) != iw * ih * 4:
                raise BvgError("raw RGBA data_len mismatch")
        else:
            try:
                from PIL import Image
                import io
                img = Image.open(io.BytesIO(data))
                iw, ih = img.size
            except ImportError:
                raise BvgError("Pillow required for PNG/JPEG images")
        idx = len(self._images)
        self._images.append(dict(format=fmt, width=iw, height=ih,
                                 data=bytes(data)))
        sid = self._style()
        nd = dict(type=P.NODE_IMAGE, img=idx,
                  rect=(float(x), float(y),
                        float(w), float(h)),
                  style=sid)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def pattern(self, img_idx, x0, y0, x1, y1, *, flags=0):
        return _Gradient(P.GRAD_PATTERN, [],
                         (int(img_idx), float(x0), float(y0),
                          float(x1), float(y1)), flags)

    def group(self, transform=None, **style_kw):
        return _GroupContext(self, transform=transform, **style_kw)

    def def_(self):
        return _DefContext(self)

    def use(self, def_idx, x, y, *, transform=None):
        nd = dict(type=P.NODE_USE, def_idx=int(def_idx),
                  x=float(x), y=float(y), style=0xFFFF)
        if transform:
            nd["transform"] = tuple(float(v) for v in transform)
        return self._add(nd)

    def validate(self):
        allowed = {P.NODE_GROUP, P.NODE_RECT, P.NODE_ELLIPSE,
                   P.NODE_LINE, P.NODE_POLYLINE, P.NODE_POLYGON,
                   P.NODE_PATH, P.NODE_TEXT, P.NODE_IMAGE,
                   P.NODE_USE, P.NODE_DEF}
        for i, nd in enumerate(self._nodes):
            if nd["type"] not in allowed:
                raise BvgError(f"node #{i} unknown type {nd['type']}")

    def shapes(self):
        return sum(1 for nd in self._nodes
                   if nd["type"] not in (P.NODE_GROUP, P.NODE_DEF))

    def is_vector(self):
        return True

    def bounds(self):
        xs, ys = [], []
        for nd in self._nodes:
            t = nd["type"]
            if t in (P.NODE_RECT, P.NODE_IMAGE):
                x, y, w, h = nd["rect"]
                xs += [x, x + w]
                ys += [y, y + h]
            elif t == P.NODE_ELLIPSE:
                cx, cy, rx, ry = nd["ellipse"]
                xs += [cx - rx, cx + rx]
                ys += [cy - ry, cy + ry]
            elif t == P.NODE_LINE:
                x1, y1, x2, y2 = nd["line"]
                xs += [x1, x2]
                ys += [y1, y2]
            elif t in (P.NODE_POLYLINE, P.NODE_POLYGON):
                for x, y in nd["points"]:
                    xs.append(x)
                    ys.append(y)
            elif t == P.NODE_PATH:
                for c in nd["commands"]:
                    if c[0] in (0, 1):
                        xs.append(c[1]); ys.append(c[2])
                    elif c[0] == 2:
                        xs.append(c[3]); ys.append(c[4])
                    elif c[0] == 3:
                        xs.append(c[5]); ys.append(c[6])
                    elif c[0] == 4:
                        xs.append(c[5]); ys.append(c[6])
        if not xs:
            return (0.0, 0.0, 0.0, 0.0)
        return (min(xs), min(ys), max(xs), max(ys))

    def to_bvgfile(self) -> BvgFile:
        doc = BvgFile(self.width, self.height)
        if self._palette:
            doc.set_chunk(b"COLR", P.pack_palette(self._palette))
        if self._gradients:
            doc.set_chunk(b"GRAD", P.pack_gradients(self._gradients))
        if self._images:
            doc.set_chunk(b"IMGS", P.pack_images(self._images))
        if self._styles:
            doc.set_chunk(b"STYL", P.pack_styles(self._styles))
        # SPEC 3.2: NODE is always present (possibly empty)
        doc.set_chunk(b"NODE", P.pack_nodes(self._nodes))
        return doc

    def __len__(self):
        return len(self._nodes)

    def save(self, path, *, codec=None, level=0,
             per_chunk=True, rle_prefilter=True, safe=False):
        cid = None
        if codec is not None:
            cid = C.codec_id(codec) if isinstance(codec, str) else int(codec)
            if not C.is_available(cid):
                raise BvgError(f"codec {codec!r} not installed")
        self.validate()
        doc = self.to_bvgfile()
        p = Path(path)
        doc.write(p, per_chunk=per_chunk, rle_prefilter=rle_prefilter,
                  default_codec=cid, default_level=level)
        return p

class _GroupContext:
    def __init__(self, doc, transform=None, **style_kw):
        self.doc = doc
        self.transform = transform
        self.style_kw = style_kw
        self._start = 0

    def __enter__(self):
        sid = self.doc._style(**self.style_kw)
        depth = self.doc._current_depth()
        self._start = len(self.doc._nodes)
        nd = dict(type=P.NODE_GROUP, child_count=0,
                  style=sid, depth=depth)
        if self.transform:
            nd["transform"] = tuple(float(v) for v in self.transform)
        self.doc._nodes.append(nd)
        self.doc._depth_stack.append(depth + 1)
        return self.doc

    def __exit__(self, *exc):
        self.doc._depth_stack.pop()
        g = self.doc._nodes[self._start]
        g["child_count"] = len(self.doc._nodes) - self._start - 1
        return False


class _DefContext:
    def __init__(self, doc):
        self.doc = doc
        self._start = 0

    def __enter__(self):
        depth = self.doc._current_depth()
        self._start = len(self.doc._nodes)
        self.doc._nodes.append(dict(type=P.NODE_DEF, child_count=0,
                                    style=0xFFFF, depth=depth))
        self.doc._depth_stack.append(depth + 1)
        return self.doc

    def __exit__(self, *exc):
        self.doc._depth_stack.pop()
        d = self.doc._nodes[self._start]
        d["child_count"] = len(self.doc._nodes) - self._start - 1
        return False


def new(width, height) -> Document:
    return Document(width, height)


def from_bvgfile(src: BvgFile) -> Document:
    d = Document(src.width, src.height)
    raw = src.get_chunk(b"COLR")
    if raw:
        d._palette = P.unpack_palette(raw)
        for i, c in enumerate(d._palette):
            d._color_idx[c] = i
    raw = src.get_chunk(b"GRAD")
    if raw:
        d._gradients = P.unpack_gradients(raw)
    raw = src.get_chunk(b"IMGS")
    if raw:
        d._images = P.unpack_images(raw)
    raw = src.get_chunk(b"STYL")
    if raw:
        d._styles = P.unpack_styles(raw)
    raw = src.get_chunk(b"NODE")
    if raw:
        d._nodes = P.unpack_nodes(raw)
    return d


def load(path) -> Document:
    from .container import read_bvg
    return from_bvgfile(read_bvg(path))