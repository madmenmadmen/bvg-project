#include "BvgCanvas.hpp"
#include "bvg_ffi.h"
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QTransform>
#include <cmath>
#include <vector>

static QPainterPath svgArcPath(double x1, double y1,
                               double rx, double ry, double rot,
                               double large_arc, double sweep,
                               double x2, double y2)
{
    QPainterPath path;
    if (rx == 0 || ry == 0) { path.moveTo(x1, y1); path.lineTo(x2, y2); return path; }
    rx = std::abs(rx); ry = std::abs(ry);
    double cos_phi = std::cos(rot), sin_phi = std::sin(rot);
    double dx = (x1 - x2) / 2, dy = (y1 - y2) / 2;
    double x1p =  cos_phi * dx + sin_phi * dy;
    double y1p = -sin_phi * dx + cos_phi * dy;
    double lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lam > 1) { double s = std::sqrt(lam); rx *= s; ry *= s; }
    double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    if (den == 0) { path.moveTo(x1, y1); path.lineTo(x2, y2); return path; }
    double coef = std::sqrt(std::max(0.0, num / den));
    if (large_arc == sweep) coef = -coef;
    double cxp =  coef * (rx * y1p) / ry;
    double cyp = -coef * (ry * x1p) / rx;
    double cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2;
    double cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2;
    auto angle = [](double ux, double uy, double vx, double vy) {
        double dot = ux * vx + uy * vy;
        double lu = std::hypot(ux, uy);
        double lv = std::hypot(vx, vy);
        if (lu == 0 || lv == 0) return 0.0;
        double c = std::clamp(dot / (lu * lv), -1.0, 1.0);
        double a = std::acos(c);
        if (ux * vy - uy * vx < 0) a = -a;
        return a;
    };
    double theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    double dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry,
                          (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (sweep == 0 && dtheta > 0) dtheta -= 2 * M_PI;
    if (sweep != 0 && dtheta < 0) dtheta += 2 * M_PI;
    int n = std::max(16, (int)std::ceil(std::abs(dtheta) * 64));
    path.moveTo(x1, y1);
    for (int k = 1; k <= n; k++) {
        double t = theta1 + dtheta * k / n;
        double ex = rx * std::cos(t);
        double ey = ry * std::sin(t);
        double px = cos_phi * ex - sin_phi * ey + cx;
        double py = sin_phi * ex + cos_phi * ey + cy;
        if (k == n) path.lineTo(x2, y2);
        else path.lineTo(px, py);
    }
    return path;
}

BvgCanvas::BvgCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 300);
}

BvgCanvas::~BvgCanvas() { clear(); }

void BvgCanvas::clear() {
    if (h_) { bvg_free(h_); h_ = nullptr; }
    palette_.clear();
    images_.clear();
    w_ = 0; canvasH_ = 0;
}

bool BvgCanvas::open(const QString& path) {
    clear();
    QByteArray ba = path.toUtf8();
    h_ = bvg_open(ba.constData());
    if (!h_) return false;
    w_ = bvg_width(h_);
    canvasH_ = bvg_height(h_);
    setMinimumSize(w_, canvasH_);
    resize(w_, canvasH_);

    size_t pn = bvg_palette_size(h_);
    palette_.reserve((int) pn);
    for (size_t i = 0; i < pn; i++) {
        uint8_t c[4] = {0,0,0,0};
        if (bvg_palette_get(h_, i, c))
            palette_.push_back({c[0], c[1], c[2], c[3]});
    }

    size_t in = bvg_images_count(h_);
    for (size_t i = 0; i < in; i++) {
        int fmt = 0;
        uint16_t iw = 0, ih = 0;
        bvg_images_get(h_, i, &fmt, &iw, &ih, nullptr, 0);
        if (fmt == 2) {
            size_t len = (size_t)iw * ih * 4;
            std::vector<uint8_t> buf(len);
            int got = bvg_images_get(h_, i, &fmt, &iw, &ih, buf.data(), (int)len);
            if (got <= 0) {
                images_.push_back(QImage());
                continue;
            }
            QImage img(buf.data(), iw, ih, iw * 4, QImage::Format_RGBA8888);
            images_.push_back(img.copy());
        } else {
            size_t len = 16 * 1024 * 1024;
            std::vector<uint8_t> buf(len);
            int got = bvg_images_get(h_, i, &fmt, &iw, &ih, buf.data(), (int)len);
            if (got <= 0) {
                images_.push_back(QImage());
                continue;
            }
            buf.resize(got);
            QImage img;
            img.loadFromData(buf.data(), got);
            images_.push_back(img);
        }
    }

    update();
    return true;
}

QColor BvgCanvas::colorAt(int idx) const {
    if (idx < 0 || idx >= palette_.size()) return QColor(0, 0, 0, 0);
    auto& c = palette_[idx];
    return QColor(c[0], c[1], c[2], c[3]);
}

QBrush BvgCanvas::brushFor(int style_idx) const {
    if (style_idx < 0) return Qt::NoBrush;
    int fill_kind = 0; uint16_t fill_idx = 0;
    int stroke_kind = 0; uint16_t stroke_idx = 0;
    double sw = 0, miter = 0;
    int cap = 0, join = 0, blend = 0;
    uint8_t opacity = 0;
    size_t dash_count = 0;
    double dash[16] = {0};
    if (!bvg_style_get(h_, (size_t)style_idx, &fill_kind, &fill_idx,
                       &stroke_kind, &stroke_idx, &sw, &cap, &join,
                       &miter, &opacity, &blend, &dash_count, dash))
        return Qt::NoBrush;
    if (fill_kind == 0) return Qt::NoBrush;
    if (fill_kind == 1) return QBrush(colorAt(fill_idx));
    int kind = 0; size_t sc = 0;
    double offs[32] = {0};
    uint16_t ci[32] = {0};
    uint8_t rgba[32 * 4] = {0};
    double geo[5] = {0};
    if (!bvg_gradient_get(h_, fill_idx, &kind, &sc, offs, ci, rgba, geo))
        return Qt::NoBrush;
    QGradientStops stops;
    for (size_t i = 0; i < sc; i++) {
        QColor c = (ci[i] == 0xFFFF)
            ? QColor(rgba[i*4], rgba[i*4+1], rgba[i*4+2], rgba[i*4+3])
            : colorAt(ci[i]);
        stops.append({offs[i], c});
    }
    if (kind == 0) {
        QLinearGradient lg(geo[0], geo[1], geo[2], geo[3]);
        lg.setStops(stops);
        return QBrush(lg);
    } else if (kind == 1) {
        QRadialGradient rg(QPointF(geo[0], geo[1]), geo[2]);
        rg.setStops(stops);
        return QBrush(rg);
    } else if (kind == 2) {
        QConicalGradient cg(QPointF(geo[0], geo[1]), geo[2] * 180.0 / M_PI);
        cg.setStops(stops);
        return QBrush(cg);
    }
    return Qt::NoBrush;
}

QPen BvgCanvas::penFor(int style_idx) const {
    if (style_idx < 0) return Qt::NoPen;
    int fill_kind = 0, stroke_kind = 0, cap = 0, join = 0, blend = 0;
    uint16_t fill_idx = 0, stroke_idx = 0;
    double sw = 0, miter = 0;
    uint8_t opacity = 0;
    size_t dash_count = 0;
    double dash[16] = {0};
    if (!bvg_style_get(h_, (size_t)style_idx, &fill_kind, &fill_idx,
                       &stroke_kind, &stroke_idx, &sw, &cap, &join,
                       &miter, &opacity, &blend, &dash_count, dash))
        return Qt::NoPen;
    if (stroke_kind == 0) return Qt::NoPen;
    QPen p;
    if (stroke_kind == 1) p = QPen(colorAt(stroke_idx));
    else p = QPen(brushFor(style_idx), sw);
    p.setWidthF(sw);
    p.setCapStyle(cap == 0 ? Qt::FlatCap : cap == 1 ? Qt::RoundCap : Qt::SquareCap);
    p.setJoinStyle(join == 0 ? Qt::MiterJoin : join == 1 ? Qt::RoundJoin : Qt::BevelJoin);
    p.setMiterLimit(miter);
    if (dash_count > 0) {
        QVector<qreal> d;
        for (size_t i = 0; i < dash_count && i < 16; i++) d.push_back(dash[i]);
        p.setDashPattern(d);
    }
    return p;
}

QPainterPath BvgCanvas::buildPath(size_t idx) const {
    QPainterPath path;
    size_t n = bvg_node_path_count(h_, idx);
    if (n == 0) return path;
    std::vector<uint8_t> ops(n);
    std::vector<double> args(n * 8);
    if (!bvg_node_path(h_, idx, ops.data(), args.data())) return path;
    double cur_x = 0, cur_y = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t op = ops[i];
        double* a = &args[i * 8];
        switch (op) {
            case 0: path.moveTo(a[0], a[1]); cur_x = a[0]; cur_y = a[1]; break;
            case 1: path.lineTo(a[0], a[1]); cur_x = a[0]; cur_y = a[1]; break;
            case 2: path.quadTo(a[0], a[1], a[2], a[3]); cur_x = a[2]; cur_y = a[3]; break;
            case 3: path.cubicTo(a[0], a[1], a[2], a[3], a[4], a[5]);
                    cur_x = a[4]; cur_y = a[5]; break;
            case 4: {
                auto arc = svgArcPath(cur_x, cur_y, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
                path.connectPath(arc);
                cur_x = a[5]; cur_y = a[6];
                break;
            }
            case 5: path.closeSubpath(); break;
        }
    }
    return path;
}

void BvgCanvas::drawRange(QPainter& p, size_t start, size_t end) {
    size_t i = start;
    while (i < end) {
        int t = bvg_node_type(h_, i);
        if (t == 10) {  // Def
            uint16_t cc = bvg_node_child_count(h_, i);
            i += 1 + cc;
            continue;
        }
        int style_idx = bvg_node_style(h_, i);
        if (style_idx == 0xFFFF) style_idx = -1;

        p.save();
        if (bvg_node_has_transform(h_, i)) {
            double tf[6];
            if (bvg_node_transform(h_, i, tf)) {
                QTransform q(tf[0], tf[1], tf[2], tf[3], tf[4], tf[5]);
                p.setTransform(q, true);
            }
        }
        int blend = 0, cap = 0, join = 0;
        int fk = 0, sk = 0;
        uint16_t fi = 0, si = 0;
        double sw = 0, miter = 0;
        uint8_t op = 255;
        size_t dc = 0;
        double dash[16] = {0};
        if (style_idx >= 0) {
            bvg_style_get(h_, (size_t)style_idx, &fk, &fi, &sk, &si,
                          &sw, &cap, &join, &miter, &op, &blend, &dc, dash);
        }
        if (op < 255) p.setOpacity(op / 255.0);
        if (style_idx >= 0) {
            p.setBrush(brushFor(style_idx));
            p.setPen(penFor(style_idx));
        } else {
            p.setBrush(Qt::NoBrush);
            p.setPen(Qt::NoPen);
        }

        switch (t) {
            case 0: { // Group
                uint16_t cc = bvg_node_child_count(h_, i);
                drawRange(p, i + 1, i + 1 + cc);
                i += 1 + cc;
                p.restore();
                continue;
            }
            case 1: { // Rect
                double r[4];
                if (bvg_node_rect(h_, i, r)) {
                    if (fk != 0) p.fillRect(QRectF(r[0], r[1], r[2], r[3]), p.brush());
                    if (sk != 0) p.drawRect(QRectF(r[0], r[1], r[2], r[3]));
                }
                break;
            }
            case 2: { // Ellipse
                double e[4];
                if (bvg_node_ellipse(h_, i, e)) {
                    if (fk != 0) p.drawEllipse(QPointF(e[0], e[1]), e[2], e[3]);
                    if (sk != 0) p.drawEllipse(QPointF(e[0], e[1]), e[2], e[3]);
                }
                break;
            }
            case 3: { // Line
                double l[4];
                if (bvg_node_line(h_, i, l)) {
                    p.drawLine(QPointF(l[0], l[1]), QPointF(l[2], l[3]));
                }
                break;
            }
            case 4: case 5: { // Polyline / Polygon
                size_t n = bvg_node_point_count(h_, i);
                if (n > 0) {
                    std::vector<double> pts(n * 2);
                    if (bvg_node_points(h_, i, pts.data())) {
                        QPolygonF poly;
                        for (size_t k = 0; k < n; k++)
                            poly << QPointF(pts[k*2], pts[k*2+1]);
                        if (t == 5) {
                            if (fk != 0) p.drawPolygon(poly);
                            if (sk != 0) p.drawPolygon(poly);
                        } else {
                            if (sk != 0) p.drawPolyline(poly);
                        }
                    }
                }
                break;
            }
            case 6: { // Path
                QPainterPath pp = buildPath(i);
                if (fk != 0) p.fillPath(pp, p.brush());
                if (sk != 0) p.drawPath(pp);
                break;
            }
            case 7: { // Text
                double xy[2];
                char buf[4096] = {0};
                int got = bvg_node_text(h_, i, xy, (uint8_t*)buf, sizeof(buf) - 1);
                if (got > 0) {
                    QFont f;
                    f.setPointSizeF(std::max(1.0, sw > 0 ? sw : 12.0));
                    p.setFont(f);
                    if (fk != 0) p.setPen(QPen(colorAt(fi)));
                    p.drawText(QPointF(xy[0], xy[1]), QString::fromUtf8(buf, got));
                }
                break;
            }
            case 8: { // Image
                uint16_t img_idx = 0;
                double r[4];
                if (bvg_node_image(h_, i, &img_idx) && bvg_node_rect(h_, i, r)) {
                    if (img_idx < images_.size() && !images_[img_idx].isNull())
                        p.drawImage(QRectF(r[0], r[1], r[2], r[3]), images_[img_idx]);
                }
                break;
            }
            case 9: { // Use
                uint16_t def_idx = 0;
                double xy[2];
                if (bvg_node_use(h_, i, &def_idx, xy)) {
                    int di = 0;
                    int def_start = -1;
                    size_t j = 0;
                    while (j < (size_t)bvg_node_count(h_)) {
                        if (bvg_node_type(h_, j) == 10) {
                            if (di == def_idx) { def_start = (int)j; break; }
                            di++;
                            uint16_t cc = bvg_node_child_count(h_, j);
                            j += 1 + cc;
                        } else {
                            j++;
                        }
                    }
                    if (def_start >= 0) {
                        uint16_t cc = bvg_node_child_count(h_, (size_t)def_start);
                        p.save();
                        p.translate(xy[0], xy[1]);
                        drawRange(p, def_start + 1, def_start + 1 + cc);
                        p.restore();
                    }
                }
                break;
            }
        }
        p.restore();
        i++;
    }
}

void BvgCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    if (!h_) {
        p.fillRect(rect(), QColor(30, 30, 30));
        p.setPen(QColor(200, 200, 200));
        p.drawText(rect(), Qt::AlignCenter, "No file");
        return;
    }
    drawRange(p, 0, bvg_node_count(h_));
}