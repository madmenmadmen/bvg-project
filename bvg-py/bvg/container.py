from __future__ import annotations
import struct
from . import codec as C
from .errors import BvgFormatError, BvgVersionError, BvgChunkError

MAGIC = b"BVG\x00"
VERSION = 1

FLAG_PERCHUNK = 1 << 0
FLAG_U32XY = 1 << 1
FLAG_HAS_THUMB = 1 << 2
FLAG_HAS_META = 1 << 3

HEADER_FMT = "<4sHHIIIBB2xQ"
HEADER_SIZE = struct.calcsize(HEADER_FMT)  # 32

CHUNK_FMT_S = "<4sII"
CHUNK_FMT_P = "<4sIIBBB"
CHUNK_SIZE_S = struct.calcsize(CHUNK_FMT_S)  # 12
CHUNK_SIZE_P = struct.calcsize(CHUNK_FMT_P)  # 15

CHUNK_PRE_RLE = 1 << 0


class BvgFile:
    def __init__(self, width: int, height: int):
        self.width = int(width)
        self.height = int(height)
        self.chunks = {}
        self.chunk_order = []
        self.chunk_meta = {}
        self.meta = {}

    def set_chunk(self, ctype, payload, codec=None, level=0, use_pre=None):
        if not isinstance(ctype, (bytes, bytearray)) or len(ctype) != 4:
            raise BvgChunkError(f"chunk type must be 4 bytes, got {ctype!r}")
        ctype = bytes(ctype)
        if ctype not in self.chunks:
            self.chunk_order.append(ctype)
        self.chunks[ctype] = bytes(payload)
        if codec is not None:
            m = {"codec": int(codec), "level": int(level)}
            if use_pre is not None:
                m["use_pre"] = bool(use_pre)
            self.chunk_meta[ctype] = m

    def get_chunk(self, ctype):
        return self.chunks.get(bytes(ctype))

    def remove_chunk(self, ctype):
        ctype = bytes(ctype)
        if ctype in self.chunks:
            del self.chunks[ctype]
            self.chunk_order.remove(ctype)
            self.chunk_meta.pop(ctype, None)

    def write(self, path, *, per_chunk=True, rle_prefilter=True,
              default_codec=None, default_level=0):
        encoded = []
        g_codec_default = default_codec if default_codec is not None else 0
        g_level_default = default_level
        for ctype in self.chunk_order:
            raw = self.chunks[ctype]
            m = self.chunk_meta.get(ctype)
            if not per_chunk:
                use_pre = False
                if default_codec is not None:
                    cid = default_codec
                    lv = default_level
                else:
                    cid, lv = g_codec_default, g_level_default
            else:
                if m is not None and "codec" in m:
                    cid = m["codec"]
                    lv = m.get("level", 0)
                    use_pre = m.get("use_pre", rle_prefilter)
                elif default_codec is not None:
                    cid = default_codec
                    lv = default_level
                    use_pre = rle_prefilter
                else:
                    cid, lv, use_pre = C.auto_codec_level(
                        raw, rle_prefilter=rle_prefilter)
            comp = C.compress(raw, cid, lv, rle_prefilter=use_pre)
            encoded.append((ctype, cid, lv, comp, len(raw), use_pre))

        # SPEC 7: FLAG_U32XY is always set by writers in v1
        flags = FLAG_U32XY
        if per_chunk:
            flags |= FLAG_PERCHUNK
        if self.meta.get("u32xy", False):
            flags |= FLAG_U32XY
        if b"THMB" in self.chunks:
            flags |= FLAG_HAS_THUMB
        if b"META" in self.chunks:
            flags |= FLAG_HAS_META

        if per_chunk or not encoded:
            g_codec, g_level = 0, 0
        else:
            _, g_codec, g_level, _, _, _ = encoded[0]

        total_uncompressed = sum(e[4] for e in encoded)
        rec_size = CHUNK_SIZE_P if per_chunk else CHUNK_SIZE_S
        data_start = HEADER_SIZE + rec_size * len(encoded)
        table = bytearray()
        blob = bytearray()
        offset = 0
        for ctype, cid, lv, comp, _ul, use_pre in encoded:
            if per_chunk:
                cflags = CHUNK_PRE_RLE if use_pre else 0
                table += struct.pack(CHUNK_FMT_P, ctype, offset,
                                     len(comp), cid, lv, cflags)
            else:
                table += struct.pack(CHUNK_FMT_S, ctype, offset, len(comp))
            blob += comp
            offset += len(comp)

        header = struct.pack(
            HEADER_FMT, MAGIC, VERSION, flags,
            self.width, self.height, len(encoded),
            g_codec, g_level, total_uncompressed,
        )
        with open(path, "wb") as f:
            f.write(header)
            f.write(table)
            f.write(blob)


def read_bvg(path) -> BvgFile:
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < HEADER_SIZE:
        raise BvgFormatError("file too small for BVG header")
    (magic, ver, flags, w, h, nchunks,
     g_codec, g_level, total) = struct.unpack_from(HEADER_FMT, data, 0)
    if magic != MAGIC:
        raise BvgFormatError(f"bad magic: {magic!r}")
    if ver != VERSION:
        raise BvgVersionError(f"unsupported version: {ver}")

    # SPEC 6: reserved MUST be zero
    reserved = struct.unpack_from("<H", data, 22)[0]
    if reserved != 0:
        raise BvgFormatError("reserved must be zero")

    per_chunk = bool(flags & FLAG_PERCHUNK)
    u32xy = bool(flags & FLAG_U32XY)

    rec_size = CHUNK_SIZE_P if per_chunk else CHUNK_SIZE_S
    off_table = HEADER_SIZE
    if off_table + rec_size * nchunks > len(data):
        raise BvgFormatError("chunk table truncated")
    data_start = off_table + rec_size * nchunks

    doc = BvgFile(w, h)
    doc.meta["u32xy"] = u32xy

    for i in range(nchunks):
        base = off_table + i * rec_size
        if per_chunk:
            ctype, off, size, cid, lv, cflags = struct.unpack_from(
                CHUNK_FMT_P, data, base)
            use_pre = bool(cflags & CHUNK_PRE_RLE)
        else:
            ctype, off, size = struct.unpack_from(CHUNK_FMT_S, data, base)
            cid, lv = g_codec, g_level
            use_pre = False
        if data_start + off + size > len(data):
            raise BvgChunkError(f"chunk {ctype!r} out of bounds")
        comp = data[data_start + off:data_start + off + size]
        raw = C.decompress(comp, cid, lv, rle_prefilter=use_pre)
        doc.set_chunk(ctype, raw, codec=cid, level=lv, use_pre=use_pre)

    return doc


def describe(path) -> dict:
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < HEADER_SIZE:
        raise BvgFormatError("file too small for BVG header")
    (magic, ver, flags, w, h, nchunks,
     g_codec, g_level, total) = struct.unpack_from(HEADER_FMT, data, 0)
    if magic != MAGIC:
        raise BvgFormatError(f"bad magic: {magic!r}")
    if ver != VERSION:
        raise BvgVersionError(f"unsupported version: {ver}")

    per_chunk = bool(flags & FLAG_PERCHUNK)
    rec_size = CHUNK_SIZE_P if per_chunk else CHUNK_SIZE_S
    off_table = HEADER_SIZE
    data_start = off_table + rec_size * nchunks

    per_chunk_codecs = []
    if per_chunk:
        for i in range(nchunks):
            base = off_table + i * rec_size
            ctype, off, size, cid, lv, cflags = struct.unpack_from(
                CHUNK_FMT_P, data, base)
            per_chunk_codecs.append({
                "type": ctype.decode("ascii", "replace"),
                "codec": cid,
                "codec_name": C.codec_name(cid),
                "level": lv,
                "use_pre": bool(cflags & CHUNK_PRE_RLE),
                "size": size,
            })

    return dict(
        magic=magic, version=ver, flags=flags,
        width=w, height=h, chunk_count=nchunks,
        global_codec=g_codec, global_level=g_level,
        global_codec_name=C.codec_name(g_codec),
        total_uncompressed=total,
        per_chunk=per_chunk,
        per_chunk_codecs=per_chunk_codecs,
        has_thumb=bool(flags & FLAG_HAS_THUMB),
        has_meta=bool(flags & FLAG_HAS_META),
    )