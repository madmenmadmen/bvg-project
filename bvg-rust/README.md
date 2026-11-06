# bvg-rust

Rust implementation of BVG (Binary Vector Graphics) — a compact binary format for 2D vector graphics.

## Installation

```bash
cargo install bvg
```

## Usage

```rust
use bvg::BvgFile;

let data = std::fs::read("input.bvg")?;
let file = BvgFile::read_from_bytes(&data)?;

println!("Canvas: {}x{}", file.width, file.height);
for chunk in file.chunks() {
    println!("Chunk: {}", chunk.name());
}
```

## CLI

```bash
bvg-rust info file.bvg
bvg-rust list file.bvg
bvg-rust dump file.bvg -o output.json
bvg-rust recompress input.bvg output.bvg
bvg-rust codecs
```

## Features

- Read and write BVG files
- Support for all chunk types: `COLR`, `GRAD`, `IMGS`, `STYL`, `NODE`, `META`, `THMB`
- Codecs: none, zlib, zstd, lz4, rle
- RLE prefilter support
- CLI for inspection and recompression

## Links

- Specification: https://github.com/madmenmadmen/bvg-spec
- Python implementation: https://github.com/madmenmadmen/bvg-project/tree/main/bvg-py
- C++ implementation: https://github.com/madmenmadmen/bvg-project/tree/main/bvg-cpp
- Java implementation: https://github.com/madmenmadmen/bvg-project/tree/main/bvg-java

## License

MIT