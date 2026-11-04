# BVG

Universal Binary Vector Graphics - binary vector container.

- Chunk-based: `COLR`, `GRAD`, `IMGS`, `STYL`, `NODE`, `FONT`, `META`, `THMB`.
- Vector geometry: rect, ellipse, line, polyline, polygon, path, text.
- Gradients: linear, radial, conic, pattern.
- Optional raster resources (PNG/JPEG/raw RGBA) via `NODE_IMAGE` or
  `GRAD_PATTERN`; the drawing itself remains vector.
- Multi-codec: `none`, `zlib`, `zstd`, `lz4`, `rle` with per-chunk level.
- Per-chunk RLE prefilter.

## Install

    pip install bvg
    pip install "bvg[zstd]"
    pip install "bvg[lz4]"
    pip install "bvg[img]"
    pip install "bvg[all]"

## Quick start

```python
import bvg

doc = bvg.new(400, 300)
doc.rect(10, 10, 100, 60, fill="#ff0000")
doc.circle(250, 80, 40, fill="#00ff00", stroke="#000", stroke_width=2)
doc.path("M 20 20 L 120 20 L 70 120 Z",
         fill=bvg.linear(0, 0, 100, 0, (0, "#f00"), (1, "#00f")))
doc.save("scene.bvg", codec="zstd", level=19)
```

## Load, edit, save

```python
doc = bvg.load("scene.bvg")
doc.circle(10, 10, 5, fill="#fff")
doc.save("scene2.bvg", codec="zstd", level=19)
```

## SVG interchange

```python
doc = bvg.from_svg(open("scene.svg", encoding="utf-8").read())
doc.save("scene.bvg")
print(doc.to_svg())
```

## CLI

    bvg info scene.bvg
    bvg list scene.bvg
    bvg recompress scene.bvg scene2.bvg --codec zstd --level 19
    bvg codecs

## Format

See `bvg-spec/SPEC.pdf` for the format specification.

## License

MIT.