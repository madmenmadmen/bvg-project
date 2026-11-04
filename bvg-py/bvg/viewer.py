from __future__ import annotations
import sys
import os
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QFileDialog, QScrollArea, QMessageBox,
)
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QAction
from .container import read_bvg, describe

class BvgViewer(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("BVG Viewer")
        self.resize(1200, 800)
        self.doc = None
        self.path = None
        self.canvas = None

        c = QWidget()
        self.setCentralWidget(c)
        lay = QVBoxLayout(c)

        top = QHBoxLayout()
        self.btn_open = QPushButton("Open BVG")
        self.btn_open.clicked.connect(self.open_file)
        top.addWidget(self.btn_open)

        self.lbl_info = QLabel("-")
        self.lbl_info.setTextInteractionFlags(
            Qt.TextInteractionFlag.TextSelectableByMouse)
        top.addWidget(self.lbl_info, stretch=1)
        lay.addLayout(top)

        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        lay.addWidget(self.scroll, stretch=1)

        act = QAction("Open", self)
        act.triggered.connect(self.open_file)
        self.menuBar().addAction(act)

    def open_file(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Open BVG", "", "BVG (*.bvg);;All files (*)")
        if not path:
            return
        try:
            self.doc = read_bvg(path)
        except Exception as e:
            QMessageBox.critical(self, "Error", str(e))
            return
        self.path = path
        try:
            from .render.qt import BvgCanvas
            self.canvas = BvgCanvas(self.doc)
            self.scroll.setWidget(self.canvas)
        except ImportError:
            QMessageBox.critical(self, "Error",
                                 "PyQt6 not installed")
            return

        info = describe(path)
        size = os.path.getsize(path)
        ratio = info["total_uncompressed"] / max(1, size)
        self.lbl_info.setText(
            f"{os.path.basename(path)} - {info['width']}x{info['height']}, "
            f"chunks={info['chunk_count']}, "
            f"{size} B (raw {info['total_uncompressed']} B, "
            f"{ratio:.2f}x)")

def main():
    app = QApplication(sys.argv)
    w = BvgViewer()
    w.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()