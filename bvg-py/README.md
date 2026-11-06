# bvg-py

Python implementation of BVG (Binary Vector Graphics) — a compact binary format for 2D vector graphics.

## Installation

```bash
pip install bvg
```

## Usage

```python
import bvg

doc = bvg.new(800, 600)
doc.rect(10, 20, 100, 50, fill="#ff0000")
doc.save("output.bvg", codec="zstd", level=19)
```

## Features

- Read and write BVG files
- Support for all chunk types: `COLR`, `GRAD`, `IMGS`, `STYL`, `NODE`, `META`, `THMB`
- Codecs: none, zlib, zstd, lz4, rle
- RLE prefilter support
- PyQt6 renderer (`bvg.render.qt.BvgCanvas`)

## Links

- Specification: https://github.com/madmenmadmen/bvg-spec
- C++ implementation: https://github.com/madmenmadmen/bvg-project/tree/main/bvg-cpp
- Rust implementation: https://github.com/madmenmadmen/bvg-project/tree/main/bvg-rust
- Java implementation: https://github.com/madmenmadmen/bvg-project/tree/main/bvg-java

## License

MIT