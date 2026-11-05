#pragma once

#include <QWidget>
#include <QImage>
#include <QVector>
#include <QColor>
#include <QBrush>
#include <QPen>
#include <QPainterPath>
#include <QPolygonF>
#include <array>
#include <memory>

#include "bvg/BvgFile.hpp"
#include "bvg/Generated.hpp"

class BvgCanvas : public QWidget {
    Q_OBJECT
public:
    explicit BvgCanvas(std::shared_ptr<bvg::BvgFile> f,
                       QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QColor colorAt(int idx) const;
    QBrush brushFor(const bvg::Style& s) const;
    QPen penFor(const bvg::Style& s) const;
    QPainterPath buildPath(const std::vector<bvg::PathCmd>& cmds) const;
    void drawRange(QPainter& p, size_t start, size_t end);

    std::shared_ptr<bvg::BvgFile> file_;
    QVector<std::array<int, 4>> palette_;
    QVector<bvg::Gradient> grads_;
    QVector<bvg::Style> styles_;
    QVector<bvg::Node> nodes_;
    QVector<QImage> images_;
};