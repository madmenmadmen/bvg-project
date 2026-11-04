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

struct BvgHandle;

class BvgCanvas : public QWidget {
    Q_OBJECT
public:
    explicit BvgCanvas(QWidget* parent = nullptr);
    ~BvgCanvas() override;
    bool open(const QString& path);
    void clear();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QColor colorAt(int idx) const;
    QBrush brushFor(int style_idx) const;
    QPen penFor(int style_idx) const;
    QPainterPath buildPath(size_t idx) const;
    void drawRange(QPainter& p, size_t start, size_t end);

    BvgHandle* h_ = nullptr;
    int w_ = 0, canvasH_ = 0;
    QVector<std::array<int, 4>> palette_;
    QVector<QImage> images_;
};