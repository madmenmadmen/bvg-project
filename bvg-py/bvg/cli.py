from __future__ import annotations
import argparse
import binascii
import json
import os
import struct
import sys

from . import codec as C
from .container import (
    read_bvg, describe,
    HEADER_FMT, HEADER_SIZE, MAGIC, VERSION,
    CHUNK_FMT_S, CHUNK_FMT_P, CHUNK_SIZE_S, CHUNK_SIZE_P,
    FLAG_PERCHUNK, CHUNK_PRE_RLE,
)
from .errors import BvgError
from . import __version__

def _fmt_size(n):
    for unit in ("B", "KiB", "MiB", "GiB"):
        if n < 1024 or unit == "GiB":
            return f"{n} B" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} GiB"


def cmd_info(args):
    info = describe(args.file)
    print(f"BVG v{info['version']}  {args.file}")
    print(f"  {'format kind':<20} vector")
    print(f"  {'magic':<20} {info['magic']!r}")
    print(f"  {'canvas':<20} {info['width']} x {info['height']}")
    print(f"  {'chunks':<20} {info['chunk_count']}")
    print(f"  {'global codec':<20} {info['global_codec_name']} "
          f"(id={info['global_codec']}, level={info['global_level']})")
    if info["per_chunk"]:
        print(f"  {'per-chunk codecs':<20}")
        for c in info.get("per_chunk_codecs", []):
            pre = " +rle" if c["use_pre"] else ""
            print(f"    {c['type']:<6} {c['codec_name']:<6} "
                  f"level={c['level']:<3}{pre}  {c['size']} B")
    print(f"  {'per-chunk':<20} {info['per_chunk']}")
    print(f"  {'has thumb':<20} {info['has_thumb']}")
    print(f"  {'has meta':<20} {info['has_meta']}")
    print(f"  {'total uncompressed':<20} "
          f"{_fmt_size(info['total_uncompressed'])}")
    size = os.path.getsize(args.file)
    if info["total_uncompressed"]:
        ratio = info["total_uncompressed"] / max(1, size)
        if size < info["total_uncompressed"]:
            note = f"{ratio:.2f}x smaller"
        else:
            note = f"{size / max(1, info['total_uncompressed']):.2f}x larger"
        print(f"  {'file size':<20} {_fmt_size(size)} "
              f"(uncompressed {_fmt_size(info['total_uncompressed'])}, "
              f"{note})")
    else:
        print(f"  {'file size':<20} {_fmt_size(size)}")
    return 0


def cmd_list(args):
    with open(args.file, "rb") as f:
        data = f.read()
    if len(data) < HEADER_SIZE:
        print("file too small", file=sys.stderr)
        return 2
    (magic, ver, flags, w, h, nchunks,
     g_codec, g_level, total) = struct.unpack_from(HEADER_FMT, data, 0)
    if magic != MAGIC:
        print(f"bad magic: {magic!r}", file=sys.stderr)
        return 2

    per_chunk = bool(flags & FLAG_PERCHUNK)
    rec = CHUNK_SIZE_P if per_chunk else CHUNK_SIZE_S
    off_table = HEADER_SIZE
    data_start = off_table + rec * nchunks

    print(f"BVG v{ver}  {args.file}  {w}x{h}  chunks={nchunks}  "
          f"per_chunk={per_chunk}")
    print()
    hdr = (f"{'type':<6} {'codec':<6} {'lvl':>4} {'pre':>4} "
           f"{'size':>10} {'raw':>10} {'comp/raw':>9}")
    print(hdr)
    print("-" * len(hdr))
    total_c = 0
    total_r = 0
    for i in range(nchunks):
        base = off_table + i * rec
        if per_chunk:
            ctype, off, size, cid, lv, cflags = struct.unpack_from(
                CHUNK_FMT_P, data, base)
            use_pre = bool(cflags & CHUNK_PRE_RLE)
        else:
            ctype, off, size = struct.unpack_from(CHUNK_FMT_S, data, base)
            cid, lv = g_codec, g_level
            use_pre = False
        comp = data[data_start + off:data_start + off + size]
        try:
            raw = C.decompress(comp, cid, lv, rle_prefilter=use_pre)
            raw_size = len(raw)
        except BvgError:
            raw_size = -1
        ratio = (size / raw_size) if size and raw_size > 0 else 0
        pre = "yes" if use_pre else "no"
        print(f"{ctype.decode('ascii','replace'):<6} "
              f"{C.codec_name(cid):<6} {lv:>4} {pre:>4} "
              f"{_fmt_size(size):>10} {_fmt_size(max(0,raw_size)):>10} "
              f"{ratio:>8.2f}x")
        total_c += size
        if raw_size > 0:
            total_r += raw_size
    print("-" * len(hdr))
    ratio = total_c / total_r if total_r else 0
    print(f"{'TOTAL':<6} {'':<6} {'':>4} {'':>4} "
          f"{_fmt_size(total_c):>10} {_fmt_size(total_r):>10} "
          f"{ratio:>8.2f}x")
    return 0


def cmd_dump(args):
    """Dump file contents as JSON (for conformance testing)."""
    f = read_bvg(args.file)
    chunks = []
    # копия порядка, чтобы не зависеть от ленивой загрузки
    order = list(f.chunk_order)
    for t in order:
        raw = f.get_chunk(t)
        if raw is None:
            continue
        m = f.chunk_meta.get(t, {})
        chunks.append({
            "type": t.decode("ascii", "replace"),
            "hex": binascii.hexlify(raw).decode("ascii"),
            "meta": {
                "codec": m.get("codec", 0),
                "level": m.get("level", 0),
                "use_pre": m.get("use_pre", False),
            },
        })
    out = {
        "file": os.path.basename(args.file),
        "width": f.width,
        "height": f.height,
        "chunks": chunks,
    }
    text = json.dumps(out, indent=2, sort_keys=True)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as fh:
            fh.write(text)
            fh.write("\n")
        print(f"wrote {args.output}", file=sys.stderr)
    else:
        print(text)
    return 0


def _parse_codec_arg(s):
    if s == "auto":
        return None
    try:
        return C.codec_id(s)
    except BvgError:
        raise SystemExit(f"unknown codec: {s}")


def cmd_recompress(args):
    cid = _parse_codec_arg(args.codec)
    if cid is not None and not C.is_available(cid):
        raise SystemExit(f"codec {args.codec!r} not installed")
    doc = read_bvg(args.input)
    per_chunk = not args.global_codec
    if args.per_chunk:
        per_chunk = True
    if cid is not None:
        for ct in doc.chunk_order:
            doc.chunk_meta[ct] = {"codec": cid, "level": args.level,
                                  "use_pre": not args.no_rle_pre}
    else:
        doc.chunk_meta.clear()
    doc.write(args.output, per_chunk=per_chunk,
              rle_prefilter=not args.no_rle_pre,
              default_codec=cid, default_level=args.level)
    in_size = os.path.getsize(args.input)
    out_size = os.path.getsize(args.output)
    ratio = in_size / out_size if out_size else 0
    print(f"{args.input}  {_fmt_size(in_size)}")
    print(f"{args.output}  {_fmt_size(out_size)}  ({ratio:.2f}x smaller)")
    return 0


def cmd_codecs(args):
    print(f"{'id':>3} {'name':<6} {'available':<10} "
          f"{'default':>8} {'range':>10}")
    print("-" * 42)
    for cid, name in C.CODEC_NAMES.items():
        avail = "yes" if C.is_available(cid) else "no"
        lv = C.DEFAULT_LEVEL.get(cid, 0)
        lo, hi = C.LEVEL_RANGE.get(cid, (0, 0))
        rng = f"{lo}..{hi}" if hi else "-"
        print(f"{cid:>3} {name:<6} {avail:<10} {lv:>8} {rng:>10}")
    if not C.HAS_ZSTD:
        print("\nzstandard not installed - pip install zstandard")
    if not C.HAS_LZ4:
        print("lz4 not installed - pip install lz4")
    return 0


def build_parser():
    p = argparse.ArgumentParser(
        prog="bvg",
        description="BVG - Universal Binary Vector Graphics tool")
    p.add_argument("--version", action="version",
                   version=f"bvg {__version__}")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("info", help="print header info")
    sp.add_argument("file")
    sp.set_defaults(func=cmd_info)

    sp = sub.add_parser("list", help="list chunks")
    sp.add_argument("file")
    sp.set_defaults(func=cmd_list)

    sp = sub.add_parser("dump", help="dump file contents as JSON")
    sp.add_argument("file")
    sp.add_argument("-o", "--output", default=None,
                    help="write JSON to file (default: stdout)")
    sp.set_defaults(func=cmd_dump)

    sp = sub.add_parser("recompress", help="re-encode a BVG file")
    sp.add_argument("input")
    sp.add_argument("output")
    sp.add_argument("--codec", default="auto",
                    help="none|zlib|zstd|lz4|rle|auto")
    sp.add_argument("--level", type=int, default=0)
    g = sp.add_mutually_exclusive_group()
    g.add_argument("--per-chunk", action="store_true")
    g.add_argument("--global-codec", action="store_true")
    sp.add_argument("--no-rle-pre", action="store_true")
    sp.set_defaults(func=cmd_recompress)

    sp = sub.add_parser("codecs", help="list codecs")
    sp.set_defaults(func=cmd_codecs)

    return p


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except BvgError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except FileNotFoundError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())