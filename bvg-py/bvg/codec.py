from __future__ import annotations
import zlib
from .errors import BvgCodecError

try:
    import zstandard as zstd
    HAS_ZSTD = True
except ImportError:
    HAS_ZSTD = False

try:
    import lz4.frame as lz4f
    HAS_LZ4 = True
except ImportError:
    HAS_LZ4 = False

CODEC_NONE = 0
CODEC_ZLIB = 1
CODEC_ZSTD = 2
CODEC_LZ4 = 3
CODEC_RLE = 4

CODEC_NAMES = {0: "none", 1: "zlib", 2: "zstd", 3: "lz4", 4: "rle"}
CODEC_IDS = {v: k for k, v in CODEC_NAMES.items()}

DEFAULT_LEVEL = {CODEC_NONE: 0, CODEC_ZLIB: 6, CODEC_ZSTD: 3,
                 CODEC_LZ4: 0, CODEC_RLE: 0}
LEVEL_RANGE = {CODEC_NONE: (0, 0), CODEC_ZLIB: (1, 9),
               CODEC_ZSTD: (1, 22), CODEC_LZ4: (0, 16),
               CODEC_RLE: (0, 0)}


def codec_id(name: str) -> int:
    try:
        return CODEC_IDS[name.lower()]
    except KeyError:
        raise BvgCodecError(f"unknown codec name: {name!r}")


def codec_name(cid: int) -> str:
    return CODEC_NAMES.get(cid, f"unknown({cid})")


def is_available(cid: int) -> bool:
    if cid in (CODEC_NONE, CODEC_ZLIB, CODEC_RLE):
        return True
    if cid == CODEC_ZSTD:
        return HAS_ZSTD
    if cid == CODEC_LZ4:
        return HAS_LZ4
    return False


def resolve_level(cid: int, level: int) -> int:
    if cid not in LEVEL_RANGE:
        raise BvgCodecError(f"unknown codec id: {cid}")
    lo, hi = LEVEL_RANGE[cid]
    if level == 0:
        return DEFAULT_LEVEL[cid]
    return max(lo, min(hi, level))


def rle_encode(data: bytes) -> bytes:
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        b = data[i]
        j = i + 1
        while j < n and data[j] == b and (j - i) < 130:
            j += 1
        run = j - i
        if run >= 3:
            out.append(0x80 | (run - 3))
            out.append(b)
            i = j
        else:
            start = i
            i += 1
            while i < n and (i - start) < 128:
                if i + 2 < n and data[i] == data[i + 1] == data[i + 2]:
                    break
                i += 1
            lit = i - start
            out.append(lit - 1)
            out.extend(data[start:i])
    return bytes(out)


def rle_decode(data: bytes) -> bytes:
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        h = data[i]; i += 1
        if h & 0x80:
            run = (h & 0x7F) + 3
            if i >= n:
                raise BvgCodecError("RLE: truncated run")
            b = data[i]; i += 1
            out.extend([b] * run)
        else:
            lit = h + 1
            if i + lit > n:
                raise BvgCodecError("RLE: truncated literal")
            out.extend(data[i:i + lit]); i += lit
    return bytes(out)


def compress(data: bytes, cid: int, level: int = 0, *,
             rle_prefilter: bool = False) -> bytes:
    if rle_prefilter and cid != CODEC_RLE:
        data = rle_encode(data)
    lv = resolve_level(cid, level)
    if cid == CODEC_NONE:
        return data
    if cid == CODEC_ZLIB:
        return zlib.compress(data, lv)
    if cid == CODEC_ZSTD:
        if not HAS_ZSTD:
            raise BvgCodecError("zstandard not installed")
        return zstd.ZstdCompressor(level=lv).compress(data)
    if cid == CODEC_LZ4:
        if not HAS_LZ4:
            raise BvgCodecError("lz4 not installed")
        return lz4f.compress(data, compression_level=lv)
    if cid == CODEC_RLE:
        return rle_encode(data)
    raise BvgCodecError(f"unknown codec id: {cid}")


def decompress(data: bytes, cid: int, level: int = 0, *,
               rle_prefilter: bool = False) -> bytes:
    if cid == CODEC_NONE:
        out = data
    elif cid == CODEC_ZLIB:
        try:
            out = zlib.decompress(data)
        except zlib.error as e:
            raise BvgCodecError(f"zlib: {e}") from e
    elif cid == CODEC_ZSTD:
        if not HAS_ZSTD:
            raise BvgCodecError("zstandard not installed")
        try:
            out = zstd.ZstdDecompressor().decompress(data)
        except zstd.ZstdError as e:
            raise BvgCodecError(f"zstd: {e}") from e
    elif cid == CODEC_LZ4:
        if not HAS_LZ4:
            raise BvgCodecError("lz4 not installed")
        try:
            out = lz4f.decompress(data)
        except Exception as e:
            raise BvgCodecError(f"lz4: {e}") from e
    elif cid == CODEC_RLE:
        return rle_decode(data)
    else:
        raise BvgCodecError(f"unknown codec id: {cid}")
    if rle_prefilter and cid != CODEC_RLE:
        out = rle_decode(out)
    return out


def auto_codec_level(payload: bytes,
                     allowed=None,
                     rle_prefilter: bool = False):
    if allowed is None:
        allowed = [CODEC_NONE, CODEC_RLE, CODEC_ZLIB]
        if HAS_ZSTD:
            allowed.append(CODEC_ZSTD)
        if HAS_LZ4:
            allowed.append(CODEC_LZ4)

    best = (CODEC_NONE, 0, False)
    best_len = len(payload)

    if rle_prefilter:
        n = len(compress(payload, CODEC_NONE, 0, rle_prefilter=True))
        if n < best_len:
            best, best_len = (CODEC_NONE, 0, True), n

    for cid in allowed:
        if not is_available(cid):
            continue
        if cid in (CODEC_NONE, CODEC_RLE):
            levels = [0]
        elif cid == CODEC_ZLIB:
            levels = [6, 9]
        elif cid == CODEC_ZSTD:
            levels = [3, 9, 15, 19, 22]
        else:
            levels = [0, 9, 16]

        for lv in levels:
            try:
                n = len(compress(payload, cid, lv, rle_prefilter=False))
            except BvgCodecError:
                continue
            if n < best_len:
                best, best_len = (cid, lv, False), n

            if rle_prefilter and cid != CODEC_RLE:
                try:
                    n = len(compress(payload, cid, lv, rle_prefilter=True))
                except BvgCodecError:
                    continue
                if n < best_len:
                    best, best_len = (cid, lv, True), n

    return best