# bvg-py/render/qt.py
from __future__ import annotations

import math

from PyQt6.QtGui import (
    QColor, QPainter, QPen, QBrush, QPixmap, QImage,
    QLinearGradient, QRadialGradient, QConicalGradient,
    QTransform, QPainterPath, QFont, QFontDatabase,
)
from PyQt6.QtCore import Qt, QPointF, QRectF
from PyQt6.QtWidgets import QWidget

from .. import primitives as P
from ..container import BvgFile


def _svg_arc_path(x1, y1, rx, ry, rot, large_arc, sweep, x2, y2) -> QPainterPath:
    """Exact SVG arc (endpoint -> center parameterization) as QPainterPath.

    Uses QPainterPath.arcTo on a unit ellipse, then QTransform to place it.
    No polyline approximation.
    """
    path = QPainterPath()
    if rx == 0 or ry == 0:
        path.moveTo(x1, y1)
        path.lineTo(x2, y2)
        return path
    rx, ry = abs(rx), abs(ry)
    cos_phi = math.cos(rot)
    sin_phi = math.sin(rot)

    # step 1: (x1', y1')
    dx = (x1 - x2) / 2.0
    dy = (y1 - y2) / 2.0
    x1p =  cos_phi * dx + sin_phi * dy
    y1p = -sin_phi * dx + cos_phi * dy

    # correct radii
    lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry)
    if lam > 1:
        s = math.sqrt(lam)
        rx *= s
        ry *= s

    # step 2: (cx', cy')
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    if den == 0:
        path.moveTo(x1, y1)
        path.lineTo(x2, y2)
        return path
    coef = math.sqrt(max(0.0, num / den))
    if large_arc == sweep:
        coef = -coef
    cxp =  coef * (rx * y1p) / ry
    cyp = -coef * (ry * x1p) / rx

    # step 3: center
    cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2.0
    cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2.0

    # step 4: angles
    def angle(ux, uy, vx, vy):
        dot = ux * vx + uy * vy
        lu = math.hypot(ux, uy)
        lv = math.hypot(vx, vy)
        if lu == 0 or lv == 0:
            return 0.0
        c = max(-1.0, min(1.0, dot / (lu * lv)))
        a = math.acos(c)
        if ux * vy - uy * vx < 0:
            a = -a
        return a

    theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry,
                   (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and dtheta > 0:
        dtheta -= 2 * math.pi
    elif sweep and dtheta < 0:
        dtheta += 2 * math.pi

    # Build arc in unit ellipse space: rect (-1, -1, 2, 2)
    # QPainterPath.arcTo uses degrees, y-down, counter-clockwise.
    a1 = math.degrees(theta1)
    a2 = math.degrees(theta1 + dtheta)
    # Qt uses CCW with y-down => sign flip
    rect = QRectF(-1, -1, 2, 2)
    ellipse_path = QPainterPath()
    ellipse_path.arcMoveTo(rect, -a1)
    ellipse_path.arcTo(rect, -a1, -(a2 - a1))

    # Transform from unit ellipse space to world: scale(rx, ry), rotate(rot),
    # translate(cx, cy)
    tf = QTransform()
    tf.translate(cx, cy)
    tf.rotateRadians(rot)
    tf.scale(rx, ry)
    world_path = tf.map(ellipse_path)

    path.moveTo(x1, y1)
    path.connectPath(world_path)
    # ensure exact end point
    path.lineTo(x2, y2)
    return path


class BvgCanvas(QWidget):
    def __init__(self, bvgfile: BvgFile, parent=None):
        super().__init__(parent)
        self.file = bvgfile
        self.palette = P.unpack_palette(
            bvgfile.get_chunk(b"COLR") or b"\x00\x00")
        raw = bvgfile.get_chunk(b"GRAD")
        self.grads = P.unpack_gradients(raw) if raw else []
        raw = bvgfile.get_chunk(b"STYL")
        self.styles = P.unpack_styles(raw) if raw else []
        raw = bvgfile.get_chunk(b"NODE")
        self.nodes = P.unpack_nodes(raw) if raw else []
        self.images = []
        raw = bvgfile.get_chunk(b"IMGS")
        if raw:
            for im in P.unpack_images(raw):
                pm = QPixmap()
                if im["format"] == P.IMG_RAW_RGBA:
                    qimg = QImage(im["data"], im["width"], im["height"],
                                  im["width"] * 4,
                                  QImage.Format.Format_RGBA8888)
                    pm = QPixmap.fromImage(qimg.copy())
                else:
                    pm.loadFromData(im["data"])
                self.images.append(pm)
        # build def index: list of (start_idx, end_idx) for each DEF
        self.defs = []
        i = 0
        n = len(self.nodes)
        while i < n:
            nd = self.nodes[i]
            if nd["type"] == P.NODE_DEF:
                cc = nd.get("child_count", 0)
                self.defs.append((i, i + 1 + cc))
                i += 1 + cc
            else:
                i += 1
        self.setMinimumSize(bvgfile.width, bvgfile.height)

    def _color(self, idx):
        if idx == 0xFFFF or idx >= len(self.palette):
            return QColor(0, 0, 0, 0)
        r, g, b, a = self.palette[idx]
        return QColor(r, g, b, a)

    def _gradient_brush(self, gi):
        if gi >= len(self.grads):
            return QBrush(Qt.BrushStyle.NoBrush)
        g = self.grads[gi]
        if g["kind"] == P.GRAD_PATTERN:
            img_idx, x0, y0, x1, y1 = g["geometry"]
            if img_idx < len(self.images):
                pm = self.images[img_idx]
                br = QBrush(pm)
                tf = QTransform().translate(x0, y0).scale(
                    (x1 - x0) / max(1, pm.width()),
                    (y1 - y0) / max(1, pm.height()))
                br.setTransform(tf)
                return br
            return QBrush(Qt.BrushStyle.NoBrush)
        stops = []
        for s in g["stops"]:
            off, ci = s[0], s[1]
            if ci == 0xFFFF:
                r, gg, b, a = s[2]
                c = QColor(r, gg, b, a)
            else:
                c = self._color(ci)
            stops.append((off, c))
        if g["kind"] == P.GRAD_LINEAR:
            x1, y1, x2, y2 = g["geometry"]
            grad = QLinearGradient(x1, y1, x2, y2)
        elif g["kind"] == P.GRAD_RADIAL:
            cx, cy, r, fx, fy = g["geometry"]
            grad = QRadialGradient(QPointF(fx, fy), r, QPointF(cx, cy))
        elif g["kind"] == P.GRAD_CONIC:
            cx, cy, sa, ea = g["geometry"]
            grad = QConicalGradient(QPointF(cx, cy), math.degrees(sa))
        else:
            return QBrush(Qt.BrushStyle.NoBrush)
        for off, c in stops:
            grad.setColorAt(off, c)
        return QBrush(grad)

    def _brush(self, style):
        fk = style.get("fill_kind", 0)
        if fk == 0:
            return QBrush(Qt.BrushStyle.NoBrush)
        if fk == 1:
            return QBrush(self._color(style.get("fill_idx", 0)))
        return self._gradient_brush(style.get("fill_idx", 0))

    def _pen(self, style):
        sk = style.get("stroke_kind", 0)
        if sk == 0:
            return QPen(Qt.PenStyle.NoPen)
        if sk == 1:
            color = self._color(style.get("stroke_idx", 0))
            pen = QPen(color)
        else:
            brush = self._gradient_brush(style.get("stroke_idx", 0))
            pen = QPen(brush, style.get("stroke_width", 1.0))
        pen.setWidthF(style.get("stroke_width", 1.0))
        cap = style.get("cap", 0)
        join = style.get("join", 0)
        pen.setCapStyle([Qt.PenCapStyle.FlatCap,
                         Qt.PenCapStyle.RoundCap,
                         Qt.PenCapStyle.SquareCap][min(cap, 2)])
        pen.setJoinStyle([Qt.PenJoinStyle.MiterJoin,
                          Qt.PenJoinStyle.RoundJoin,
                          Qt.PenJoinStyle.BevelJoin][min(join, 2)])
        pen.setMiterLimit(style.get("miter_limit", 4.0))
        dash = style.get("dash") or []
        if dash:
            pen.setDashPattern(dash)
        return pen

    def _apply_blend(self, p, style):
        bm = style.get("blend_mode", 0)
        mapping = {
            0: QPainter.CompositionMode.CompositionMode_SourceOver,
            1: QPainter.CompositionMode.CompositionMode_Multiply,
            2: QPainter.CompositionMode.CompositionMode_Screen,
            3: QPainter.CompositionMode.CompositionMode_Overlay,
        }
        p.setCompositionMode(mapping.get(bm,
            QPainter.CompositionMode.CompositionMode_SourceOver))

    def paintEvent(self, ev):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setRenderHint(QPainter.RenderHint.TextAntialiasing)
        self._draw_range(p, 0, len(self.nodes))
        p.end()

    def _draw_range(self, p, start, end):
        i = start
        while i < end:
            nd = self.nodes[i]
            t = nd["type"]
            sid = nd.get("style", 0xFFFF)
            style = self.styles[sid] if sid < len(self.styles) else {}
            if t == P.NODE_DEF:
                cc = nd.get("child_count", 0)
                i += 1 + cc
                continue
            p.save()
            if nd.get("transform"):
                a, b, c, d, e, f = nd["transform"]
                p.setTransform(p.transform() * QTransform(a, b, c, d, e, f))
            self._apply_blend(p, style)
            op = style.get("opacity", 255)
            if op < 255:
                p.setOpacity(op / 255.0)
            p.setBrush(self._brush(style))
            p.setPen(self._pen(style))
            if t == P.NODE_GROUP:
                cc = nd.get("child_count", 0)
                self._draw_range(p, i + 1, i + 1 + cc)
                i += 1 + cc
                p.restore()
                continue
            elif t == P.NODE_RECT:
                x, y, w, h = nd["rect"]
                p.drawRect(QRectF(x, y, w, h))
            elif t == P.NODE_ELLIPSE:
                cx, cy, rx, ry = nd["ellipse"]
                p.drawEllipse(QPointF(cx, cy), rx, ry)
            elif t == P.NODE_LINE:
                x1, y1, x2, y2 = nd["line"]
                p.drawLine(QPointF(x1, y1), QPointF(x2, y2))
            elif t == P.NODE_POLYLINE:
                pts = nd["points"]
                if pts:
                    p.drawPolyline([QPointF(x, y) for x, y in pts])
            elif t == P.NODE_POLYGON:
                pts = nd["points"]
                if pts:
                    p.drawPolygon([QPointF(x, y) for x, y in pts])
            elif t == P.NODE_PATH:
                path = self._build_path(nd["commands"])
                if path is not None:
                    p.drawPath(path)
            elif t == P.NODE_TEXT:
                f = QFont()
                f.setPointSizeF(max(1.0, style.get("stroke_width", 12.0)))
                p.setFont(f)
                p.drawText(QPointF(nd["x"], nd["y"]), nd["text"])
            elif t == P.NODE_IMAGE:
                x, y, w, h = nd["rect"]
                img_idx = nd["img"]
                if img_idx < len(self.images):
                    p.drawPixmap(QRectF(x, y, w, h),
                                 self.images[img_idx],
                                 QRectF(self.images[img_idx].rect()))
            elif t == P.NODE_USE:
                def_idx = nd["def_idx"]
                if def_idx < len(self.defs):
                    s, e = self.defs[def_idx]
                    p.save()
                    p.translate(nd["x"], nd["y"])
                    self._draw_range(p, s + 1, e)
                    p.restore()
            p.restore()
            i += 1

    def _build_path(self, cmds):
        path = QPainterPath()
        cur = QPointF(0, 0)
        for c in cmds:
            op = c[0]
            if op == 0:
                path.moveTo(c[1], c[2])
                cur = QPointF(c[1], c[2])
            elif op == 1:
                path.lineTo(c[1], c[2])
                cur = QPointF(c[1], c[2])
            elif op == 2:
                path.quadTo(c[1], c[2], c[3], c[4])
                cur = QPointF(c[3], c[4])
            elif op == 3:
                path.cubicTo(c[1], c[2], c[3], c[4], c[5], c[6])
                cur = QPointF(c[5], c[6])
            elif op == 4:
                rx, ry, rot, la, sw, x, y = c[1:]
                arc = _svg_arc_path(cur.x(), cur.y(), rx, ry, rot,
                                    la, sw, x, y)
                path.connectPath(arc)
                cur = QPointF(x, y)
            elif op == 5:
                path.closeSubpath()
        return path