class BvgError(Exception):
    """Base class for all BVG errors."""


class BvgFormatError(BvgError):
    """Bad magic, truncated header, truncated chunk table, bad field."""


class BvgVersionError(BvgFormatError):
    """File version is greater than the implemented version."""


class BvgCodecError(BvgError):
    """Unknown codec, codec not supported, decode failure."""


class BvgChunkError(BvgError):
    """Malformed chunk payload, out-of-bounds offset, out-of-range index."""