# bvg/__init__.py
"""BVG - Universal Binary Vector Graphics.

Binary chunk-based container for 2D vector graphics.
See bvg-spec/README.md for the format specification.
"""

from .container import (
    BvgFile, read_bvg, describe,
    MAGIC, VERSION,
    FLAG_PERCHUNK, FLAG_U32XY, FLAG_HAS_THUMB, FLAG_HAS_META,
    CHUNK_PRE_RLE,
)
from . import codec, primitives
from .errors import (
    BvgError, BvgFormatError, BvgVersionError,
    BvgCodecError, BvgChunkError,
)
from .api import (
    Document, new, load, from_bvgfile,
    parse_color, parse_path,
    linear, radial, conic,
)

__version__ = "1.0.0"

__all__ = [
    "BvgFile", "read_bvg", "describe",
    "MAGIC", "VERSION",
    "FLAG_PERCHUNK", "FLAG_U32XY", "FLAG_HAS_THUMB", "FLAG_HAS_META",
    "CHUNK_PRE_RLE",
    "codec", "primitives",
    "BvgError", "BvgFormatError", "BvgVersionError",
    "BvgCodecError", "BvgChunkError",
    "Document", "new", "load", "from_bvgfile",
    "parse_color", "parse_path",
    "linear", "radial", "conic",
]