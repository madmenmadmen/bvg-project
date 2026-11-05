#include "BvgCanvas.hpp"
#include "bvg/Generated.hpp"

#include <QPainter>
#include <QImage>
#include <QPaintEvent>
#include <QFont>
#include <QTransform>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QConicalGradient>
#include <cmath>

static QPainterPath svgArcPath(double x1, double y1,
                               double rx, double ry, double rot,
                               double large_arc, double sweep,
                               double x2, double y2)
{
    QPainterPath path;
    if (rx == 0.0 || ry == 0.0) {
        path.moveTo(x1, y1);
        path.lineTo(x2, y2);
        return path;
    }
    rx = std::abs(rx);
    ry = std::abs(ry);
    double cos_phi = std::cos(rot);
    double sin_phi = std::sin(rot);

    double dx = (x1 - x2) / 2.0;
    double dy = (y1 - y2) / 2.0;
    double x1p =  cos_phi * dx + sin_phi * dy;
    double y1p = -sin_phi * dx + cos_phi * dy;

    double lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lam > 1.0) {
        double s = std::sqrt(lam);
        rx *= s;
        ry *= s;
    }

    double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    if (den == 0.0) {
        path.moveTo(x1, y1);
        path.lineTo(x2, y2);
        return path;
    }
    double coef = std::sqrt(std::max(0.0, num / den));
    if (large_arc == sweep) coef = -coef;
    double cxp =  coef * (rx * y1p) / ry;
    double cyp = -coef * (ry * x1p) / rx;

    double cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) / 2.0;
    double cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) / 2.0;

    auto angle = [](double ux, double uy, double vx, double vy) -> double {
        double dot = ux * vx + uy * vy;
        double lu = std::hypot(ux, uy);
        double lv = std::hypot(vx, vy);
        if (lu == 0.0 || lv == 0.0) return 0.0;
        double c = std::clamp(dot / (lu * lv), -1.0, 1.0);
        double a = std::acos(c);
        if (ux * vy - uy * vx < 0.0) a = -a;
        return a;
    };

    double theta1 = angle(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    double dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry,
                          (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (!sweep && dtheta > 0.0) dtheta -= 2.0 * M_PI;
    if ( sweep && dtheta < 0.0) dtheta += 2.0 * M_PI;

    int n = std::max(16, static_cast<int>(std::ceil(std::abs(dtheta) * 64.0)));
    path.moveTo(x1, y1);
    for (int k = 1; k <= n; ++k) {
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

BvgCanvas::BvgCanvas(std::shared_ptr<bvg::BvgFile> f, QWidget* parent)
    : QWidget(parent), file_(std::move(f))
{
    setMinimumSize(file_->width, file_->height);

    if (auto* raw = file_->getChunk("COLR")) {
        auto cols = bvg::generated::unpackColr(*raw);
        for (auto& c : cols) palette_.push_back({c[0], c[1], c[2], c[3]});
    }
    if (auto* raw = file_->getChunk("GRAD")) {
        auto gs = bvg::generated::unpackGrad(*raw);
        for (auto& g : gs) grads_.push_back(g);
    }
    if (auto* raw = file_->getChunk("STYL")) {
        auto ss = bvg::generated::unpackStyl(*raw);
        for (auto& s : ss) styles_.push_back(s);
    }
    if (auto* raw = file_->getChunk("NODE")) {
        auto ns = bvg::generated::unpackNode(*raw);
        for (auto& n : ns) nodes_.push_back(n);
    }
    if (auto* raw = file_->getChunk("IMGS")) {
        auto imgs = bvg::generated::unpackImages(*raw);
        for (auto& im : imgs) {
            QImage img;
            if (im.format == bvg::generated::IMG_RAW_RGBA) {
                img = QImage(im.data.data(), im.width, im.height,
                             im.width * 4, QImage::Format_RGBA8888).copy();
            } else {
                img.loadFromData(im.data.data(),
                                 static_cast<int>(im.data.size()));
            }
            images_.push_back(img);
        }
    }
}

QSize BvgCanvas::sizeHint() const {
    return QSize(file_->width, file_->height);
}

QColor BvgCanvas::colorAt(int idx) const {
    if (idx == 0xFFFF || idx >= palette_.size()) return QColor(0, 0, 0, 0);
    auto& c = palette_[idx];
    return QColor(c[0], c[1], c[2], c[3]);
}

QBrush BvgCanvas::brushFor(const bvg::Style& s) const {
    if (s.fillKind == 0) return Qt::NoBrush;
    if (s.fillKind == 1) return QBrush(colorAt(s.fillIdx));
    if (s.fillIdx >= grads_.size()) return Qt::NoBrush;
    auto& g = grads_[s.fillIdx];
    if (g.kind == bvg::Gradient::Pattern) {
        int img_idx = static_cast<int>(g.geometry[0]);
        if (img_idx < images_.size() && !images_[img_idx].isNull()) {
            QBrush br(images_[img_idx]);
            QTransform tf;
            tf.translate(g.geometry[1], g.geometry[2]);
            double sx = (g.geometry[3] - g.geometry[1]) /
                        std::max(1, images_[img_idx].width());
            double sy = (g.geometry[4] - g.geometry[2]) /
                        std::max(1, images_[img_idx].height());
            tf.scale(sx, sy);
            br.setTransform(tf);
            return br;
        }
        return Qt::NoBrush;
    }
    QGradientStops stops;
    for (auto& st : g.stops) {
        QColor c = (st.colorIdx == 0xFFFF)
            ? QColor(st.rgba[0], st.rgba[1], st.rgba[2], st.rgba[3])
            : colorAt(st.colorIdx);
        stops.append({st.offset, c});
    }
    if (g.kind == bvg::Gradient::Linear) {
        QLinearGradient lg(g.geometry[0], g.geometry[1],
                           g.geometry[2], g.geometry[3]);
        lg.setStops(stops);
        return QBrush(lg);
    } else if (g.kind == bvg::Gradient::Radial) {
        QRadialGradient rg(QPointF(g.geometry[3], g.geometry[4]), 0.0,
                           QPointF(g.geometry[0], g.geometry[1]), g.geometry[2]);
        rg.setStops(stops);
        return QBrush(rg);
    } else if (g.kind == bvg::Gradient::Conic) {
        QConicalGradient cg(QPointF(g.geometry[0], g.geometry[1]),
                            g.geometry[2] * 180.0 / M_PI);
        cg.setStops(stops);
        return QBrush(cg);
    }
    return Qt::NoBrush;
}

QPen BvgCanvas::penFor(const bvg::Style& s) const {
    if (s.strokeKind == 0) return Qt::NoPen;
    QPen p;
    if (s.strokeKind == 1) {
        p = QPen(colorAt(s.strokeIdx));
    } else {
        p = QPen(brushFor(s), s.strokeWidth);
    }
    p.setWidthF(s.strokeWidth);
    p.setCapStyle(s.cap == 0 ? Qt::FlatCap :
                  s.cap == 1 ? Qt::RoundCap : Qt::SquareCap);
    p.setJoinStyle(s.join == 0 ? Qt::MiterJoin :
                   s.join == 1 ? Qt::RoundJoin : Qt::BevelJoin);
    p.setMiterLimit(s.miterLimit);
    if (!s.dash.empty()) {
        QVector<qreal> dash;
        for (float d : s.dash) dash.push_back(d);
        p.setDashPattern(dash);
    }
    return p;
}

QPainterPath BvgCanvas::buildPath(const std::vector<bvg::PathCmd>& cmds) const {
    QPainterPath path;
    double cur_x = 0, cur_y = 0;
    for (auto& c : cmds) {
        switch (c.op) {
            case 0: path.moveTo(c.args[0], c.args[1]);
                    cur_x = c.args[0]; cur_y = c.args[1]; break;
            case 1: path.lineTo(c.args[0], c.args[1]);
                    cur_x = c.args[0]; cur_y = c.args[1]; break;
            case 2: path.quadTo(c.args[0], c.args[1], c.args[2], c.args[3]);
                    cur_x = c.args[2]; cur_y = c.args[3]; break;
            case 3: path.cubicTo(c.args[0], c.args[1], c.args[2], c.args[3],
                                 c.args[4], c.args[5]);
                    cur_x = c.args[4]; cur_y = c.args[5]; break;
            case 4: {
                auto arc = svgArcPath(cur_x, cur_y,
                                      c.args[0], c.args[1], c.args[2],
                                      c.args[3], c.args[4], c.args[5], c.args[6]);
                path.connectPath(arc);
                cur_x = c.args[5]; cur_y = c.args[6];
                break;
            }
            case 5: path.closeSubpath(); break;
            default: break;
        }
    }
    return path;
}

void BvgCanvas::drawRange(QPainter& p, size_t start, size_t end) {
    size_t i = start;
    while (i < end) {
        auto& nd = nodes_[i];
        if (nd.type == bvg::Node::Def) {
            i += 1 + nd.childCount;
            continue;
        }
        const bvg::Style* st = (nd.style < styles_.size())
            ? &styles_[nd.style] : nullptr;

        p.save();
        if (nd.hasTransform) {
            QTransform tf(nd.transform[0], nd.transform[1], nd.transform[2],
                          nd.transform[3], nd.transform[4], nd.transform[5]);
            p.setTransform(tf, true);
        }
        if (st) {
            switch (st->blendMode) {
                case 1: p.setCompositionMode(QPainter::CompositionMode_Multiply); break;
                case 2: p.setCompositionMode(QPainter::CompositionMode_Screen); break;
                case 3: p.setCompositionMode(QPainter::CompositionMode_Overlay); break;
                default: p.setCompositionMode(QPainter::CompositionMode_SourceOver); break;
            }
            if (st->opacity < 255) p.setOpacity(st->opacity / 255.0);
            p.setBrush(brushFor(*st));
            p.setPen(penFor(*st));
        } else {
            p.setBrush(Qt::NoBrush);
            p.setPen(Qt::NoPen);
        }

        switch (nd.type) {
            case bvg::Node::Group: {
                size_t cc = nd.childCount;
                drawRange(p, i + 1, i + 1 + cc);
                i += 1 + cc;
                p.restore();
                continue;
            }
            case bvg::Node::Rect:
                p.drawRect(QRectF(nd.rect[0], nd.rect[1], nd.rect[2], nd.rect[3]));
                break;
            case bvg::Node::Ellipse:
                p.drawEllipse(QPointF(nd.ellipse[0], nd.ellipse[1]),
                              nd.ellipse[2], nd.ellipse[3]);
                break;
            case bvg::Node::Line:
                p.drawLine(QPointF(nd.line[0], nd.line[1]),
                           QPointF(nd.line[2], nd.line[3]));
                break;
            case bvg::Node::Polyline: {
                QPolygonF poly;
                for (auto& pt : nd.points) poly << QPointF(pt[0], pt[1]);
                p.drawPolyline(poly);
                break;
            }
            case bvg::Node::Polygon: {
                QPolygonF poly;
                for (auto& pt : nd.points) poly << QPointF(pt[0], pt[1]);
                p.drawPolygon(poly);
                break;
            }
            case bvg::Node::Path:
                p.drawPath(buildPath(nd.path));
                break;
            case bvg::Node::Text: {
                QFont f;
                f.setPointSizeF(std::max(1.0, st ? st->strokeWidth : 12.0));
                p.setFont(f);
                p.drawText(QPointF(nd.tx, nd.ty), QString::fromStdString(nd.text));
                break;
            }
            case bvg::Node::Image: {
                int idx = nd.imageIdx;
                if (idx < images_.size() && !images_[idx].isNull()) {
                    p.drawImage(QRectF(nd.rect[0], nd.rect[1],
                                       nd.rect[2], nd.rect[3]), images_[idx]);
                }
                break;
            }
            case bvg::Node::Use: {
                int def_idx = nd.defIdx;
                int di = 0;
                int def_start = -1;
                for (size_t j = 0; j < nodes_.size(); ++j) {
                    if (nodes_[j].type == bvg::Node::Def) {
                        if (di == def_idx) { def_start = static_cast<int>(j); break; }
                        di++;
                    }
                }
                if (def_start >= 0) {
                    size_t cc = nodes_[def_start].childCount;
                    p.save();
                    p.translate(nd.tx, nd.ty);
                    drawRange(p, def_start + 1, def_start + 1 + cc);
                    p.restore();
                }
                break;
            }
            default: break;
        }
        p.restore();
        i++;
    }
}

void BvgCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    drawRange(p, 0, nodes_.size());
}