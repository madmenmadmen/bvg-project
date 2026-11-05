#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QStatusBar>
#include <QAction>
#include <QIcon>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>

#include "BvgCanvas.hpp"
#include "bvg/BvgFile.hpp"

// Modern dark theme QSS
static const char* QSS = R"(
QMainWindow, QWidget {
    background-color: #1e1e1e;
    color: #cccccc;
    font-family: "Segoe UI", system-ui, sans-serif;
}
QToolBar {
    background: #252526;
    border: none;
    padding: 4px 8px;
    spacing: 8px;
}
QToolButton, QPushButton {
    background: transparent;
    border: 1px solid #3c3c3c;
    border-radius: 4px;
    padding: 4px 10px;
    color: #cccccc;
}
QToolButton:hover, QPushButton:hover {
    background: #2d2d2d;
    border-color: #007acc;
}
QToolButton:pressed, QPushButton:pressed {
    background: #1a1a1a;
}
QStatusBar {
    background: #252526;
    border-top: 1px solid #3c3c3c;
    color: #808080;
}
QScrollBar:vertical {
    background: #1e1e1e; width: 12px; margin: 0;
}
QScrollBar::handle:vertical {
    background: #3c3c3c; border-radius: 6px; min-height: 20px;
}
QScrollBar::handle:vertical:hover { background: #007acc; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
    background: #1e1e1e; height: 12px; margin: 0;
}
QScrollBar::handle:horizontal {
    background: #3c3c3c; border-radius: 6px; min-width: 20px;
}
QScrollBar::handle:horizontal:hover { background: #007acc; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
)";

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setStyleSheet(QSS);

    QMainWindow w;
    w.setWindowTitle("BVG Viewer");
    w.resize(1200, 800);

    // Toolbar
    auto* tb = w.addToolBar("Main");
    tb->setMovable(false);

    auto* actOpen = new QAction("Open .bvg", &w);
    tb->addAction(actOpen);

    tb->addSeparator();

    auto* btnZoomOut = new QPushButton("−");
    auto* btnZoomIn = new QPushButton("+");
    auto* btnFit = new QPushButton("Fit");
    auto* btnReset = new QPushButton("Reset");
    btnZoomOut->setFixedWidth(32);
    btnZoomIn->setFixedWidth(32);

    tb->addWidget(btnZoomOut);
    tb->addWidget(btnZoomIn);
    tb->addWidget(btnFit);
    tb->addWidget(btnReset);

    auto* zoomLabel = new QLabel("100%");
    zoomLabel->setStyleSheet("padding: 0 8px; color: #808080;");
    tb->addWidget(zoomLabel);

    // Status bar
    auto* statusFile = new QLabel("-");
    auto* statusSize = new QLabel("-");
    auto* statusZoom = new QLabel("-");
    w.statusBar()->addWidget(statusFile);
    w.statusBar()->addWidget(statusSize);
    w.statusBar()->addPermanentWidget(statusZoom);

    // Graphics view (zoom/pan)
    auto* scene = new QGraphicsScene(&w);
    auto* view = new QGraphicsView(scene, &w);
    view->setRenderHint(QPainter::Antialiasing);
    view->setRenderHint(QPainter::SmoothPixmapTransform);
    view->setDragMode(QGraphicsView::ScrollHandDrag);
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view->setBackgroundBrush(QColor(30, 30, 30));
    view->setFrameShape(QFrame::NoFrame);
    w.setCentralWidget(view);

    auto updateZoomLabel = [zoomLabel, view]() {
        double z = view->transform().m11();
        zoomLabel->setText(QString::number(int(z * 100)) + "%");
    };

    QObject::connect(btnZoomIn, &QPushButton::clicked, [view, updateZoomLabel]() {
        view->scale(1.25, 1.25);
        updateZoomLabel();
    });
    QObject::connect(btnZoomOut, &QPushButton::clicked, [view, updateZoomLabel]() {
        view->scale(0.8, 0.8);
        updateZoomLabel();
    });
    QObject::connect(btnFit, &QPushButton::clicked, [view, scene, updateZoomLabel]() {
        if (scene->items().isEmpty()) return;
        view->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
        updateZoomLabel();
    });
    QObject::connect(btnReset, &QPushButton::clicked, [view, updateZoomLabel]() {
        view->resetTransform();
        updateZoomLabel();
    });

    QObject::connect(actOpen, &QAction::triggered, [&]() {
        QString path = QFileDialog::getOpenFileName(&w, "Open BVG", "", "*.bvg");
        if (path.isEmpty()) return;
        try {
            auto f = std::make_shared<bvg::BvgFile>(
                bvg::BvgFile::read(std::filesystem::path(path.toStdWString())));
            auto* canvas = new BvgCanvas(f);
            scene->clear();
            scene->addWidget(canvas);
            statusFile->setText(QFileInfo(path).fileName());
            statusSize->setText(QString("%1×%2").arg(f->width).arg(f->height));
            statusZoom->setText("Zoom: 100%");
            view->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
            updateZoomLabel();
        } catch (std::exception& e) {
            statusFile->setText(QString("Error: ") + e.what());
        }
    });

    w.show();
    return app.exec();
}