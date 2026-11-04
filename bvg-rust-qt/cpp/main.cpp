 #include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QScrollArea>
#include "BvgCanvas.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QMainWindow w;
    w.setWindowTitle("BVG Viewer (Rust + Qt6)");
    w.resize(1200, 800);

    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);
    auto* top = new QHBoxLayout;
    auto* openBtn = new QPushButton("Open BVG");
    auto* info = new QLabel("-");
    top->addWidget(openBtn);
    top->addWidget(info);
    root->addLayout(top);

    auto* canvas = new BvgCanvas;
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(canvas);
    root->addWidget(scroll);
    w.setCentralWidget(central);

    QObject::connect(openBtn, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(&w, "Open BVG", "", "*.bvg");
        if (path.isEmpty()) return;
        if (canvas->open(path)) {
            info->setText(path);
        } else {
            info->setText("Error: " + path);
        }
    });

    w.show();
    return app.exec();
}