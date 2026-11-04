# tools/gen_testvectors.py
"""Generate reference BVG files for bvg-spec/testvectors/."""
from __future__ import annotations
import binascii
import json
import pathlib
import sys
import io
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = pathlib.Path("testvectors")
SPEC.mkdir(parents=True, exist_ok=True)
(SPEC / "expected").mkdir(parents=True, exist_ok=True)

sys.path.insert(0, str(ROOT))

import bvg
from bvg import primitives as P
from bvg import codec as C


def _save(doc, name, **kw):
    p = SPEC / name
    doc.save(p, **kw)
    print(f"  {name:<20} {p.stat().st_size:>8} B")
    _dump_expected(p)


def _dump_expected(p: pathlib.Path):
    """Write a JSON dump + (optional) SVG for conformance testing."""
    from bvg.container import read_bvg
    f = read_bvg(p)
    dump = {
        "file": p.name,
        "width": f.width,
        "height": f.height,
        "chunks": [
            {
                "type": t.decode("ascii"),
                "hex": binascii.hexlify(f.chunks[t]).decode("ascii"),
                "meta": f.chunk_meta.get(t, {}),
            }
            for t in f.chunk_order
        ],
    }
    out = SPEC / "expected" / (p.stem + ".json")
    out.write_text(json.dumps(dump, indent=2), encoding="utf-8")

    # SVG preview (best-effort)
    try:
        from bvg.api import from_bvgfile
        doc = from_bvgfile(f)
        svg = doc.to_svg()
        (SPEC / "expected" / (p.stem + ".svg")).write_text(
            svg, encoding="utf-8")
    except Exception:
        pass


def _skip(name, reason):
    print(f"  skip {name:<20} {reason}")


# ---------- generators ----------

def gen_empty():
    _save(bvg.new(1, 1), "empty.bvg", codec="zlib", level=9)


def gen_rect():
    d = bvg.new(100, 100)
    d.rect(10, 20, 50, 30, fill="#ff0000",
           stroke="#000", stroke_width=2)
    _save(d, "rect.bvg", codec="zlib", level=9)


def gen_gradient():
    if not C.is_available(C.CODEC_ZSTD):
        _skip("gradient.bvg", "zstd not installed")
        return
    d = bvg.new(100, 100)
    g = bvg.linear(0, 0, 100, 0,
                   (0.0, "#ff0000"), (1.0, "#0000ff"))
    d.rect(0, 0, 100, 100, fill=g)
    _save(d, "gradient.bvg", codec="zstd", level=19)

def gen_image_png():
    """PNG 1×1 red, stretched to 10×10 (сгенерирован через Pillow)."""
    d = bvg.new(10, 10)
    img = Image.new('RGB', (1, 1), (255, 0, 0))
    buf = io.BytesIO()
    img.save(buf, format='PNG')
    png = buf.getvalue()
    d.image(png, 0, 0, 10, 10, fmt=P.IMG_PNG)
    _save(d, "image_png.bvg", codec="zlib", level=9)

def gen_image_jpeg():
    """JPEG 1×1 red, stretched to 10×10 (сгенерирован через Pillow)."""
    d = bvg.new(10, 10)
    img = Image.new('RGB', (1, 1), (255, 0, 0))
    buf = io.BytesIO()
    img.save(buf, format='JPEG')
    jpeg = buf.getvalue()
    d.image(jpeg, 0, 0, 10, 10, fmt=P.IMG_JPEG)
    _save(d, "image_jpeg.bvg", codec="zlib", level=9)

def gen_path_arc():
    d = bvg.new(100, 100)
    d.path("M 10 10 A 20 20 0 0 1 50 50", fill="#000", stroke="#f00")
    _save(d, "path_arc.bvg", codec="zlib", level=9)


def gen_rle_runs():
    d = bvg.new(10, 10)
    for i in range(10):
        d.rect(0, i, 10, 1, fill="#123456")
    _save(d, "rle_runs.bvg", codec="rle")


def gen_transform():
    d = bvg.new(100, 100)
    with d.group(transform=(1, 0, 0, 1, 10, 10)):
        d.rect(0, 0, 20, 20, fill="#0f0")
    _save(d, "transform.bvg", codec="zlib", level=9)


def gen_image():
    d = bvg.new(10, 10)
    d.image(bytes([255, 0, 0, 255]), 0, 0, 10, 10,
            fmt=P.IMG_RAW_RGBA, iw=1, ih=1)
    _save(d, "image.bvg", codec="zlib", level=9)


def gen_zstd19():
    if not C.is_available(C.CODEC_ZSTD):
        _skip("zstd19.bvg", "zstd not installed")
        return
    d = bvg.new(200, 200)
    for i in range(50):
        d.rect(i, i, 10, 10, fill="#00ff00")
    _save(d, "zstd19.bvg", codec="zstd", level=19)


def gen_lz4():
    if not C.is_available(C.CODEC_LZ4):
        _skip("lz4.bvg", "lz4 not installed")
        return
    d = bvg.new(200, 200)
    for i in range(50):
        d.circle(i, i, 5, fill="#0000ff")
    _save(d, "lz4.bvg", codec="lz4", level=16)


def gen_rle():
    d = bvg.new(200, 200)
    for i in range(50):
        d.rect(0, i, 200, 1, fill="#ff00ff")
    _save(d, "rle.bvg", codec="rle")


def gen_def_use():
    d = bvg.new(100, 100)
    with d.def_():
        d.rect(0, 0, 10, 10, fill="#f00")
    d.use(0, 50, 50)
    _save(d, "def_use.bvg", codec="zlib", level=9)


def gen_all_nodes():
    d = bvg.new(200, 200)
    d.rect(10, 10, 20, 20, fill="#f00")
    d.ellipse(50, 50, 10, 5, fill="#0f0")
    d.line(0, 0, 100, 100, stroke="#00f", stroke_width=2)
    d.polyline([(0, 0), (10, 0), (10, 10)], stroke="#000")
    d.polygon([(20, 20), (30, 20), (30, 30)], fill="#ff0")
    d.path("M 0 0 L 10 10 Q 20 0 30 10", fill="#0ff")
    d.text("hello", 50, 50, fill="#000")
    d.image(bytes([255, 0, 0, 255]), 100, 100, 10, 10,
            fmt=P.IMG_RAW_RGBA, iw=1, ih=1)
    _save(d, "all_nodes.bvg", codec="zlib", level=9)


def gen_rle_edge():
    """Payload that starts with a run of >=3 equal bytes.

    Forces the RLE encoder to emit a run *before* any literal,
    which used to underflow (lit = 0 -> 255).
    """
    d = bvg.new(1, 1)
    d.image(bytes([0, 0, 0, 0]) * 100, 0, 0, 1, 1,
            fmt=P.IMG_RAW_RGBA, iw=10, ih=10)
    _save(d, "rle_edge.bvg", codec="rle")

def gen_path_arc_open():
    d = bvg.new(100, 100)
    d.path("M 10 10 A 20 20 0 0 1 50 50", fill=None, stroke="#f00", stroke_width=2)
    _save(d, "path_arc_open.bvg", codec="zlib", level=9)

# ---------- main ----------

def main():
    print(f"spec dir: {SPEC}")
    print("generating testvectors")
    gen_empty()
    gen_rect()
    gen_gradient()
    gen_path_arc()
    gen_path_arc_open()
    gen_rle_runs()
    gen_transform()
    gen_image()
    gen_image_png()
    gen_image_jpeg()
    gen_zstd19()
    gen_lz4()
    gen_rle()
    gen_def_use()
    gen_all_nodes()
    gen_rle_edge()
    print("done")


if __name__ == "__main__":
    main()