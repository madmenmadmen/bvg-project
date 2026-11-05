package io.bvg;

import com.github.luben.zstd.Zstd;
import net.jpountz.lz4.LZ4FrameInputStream;
import net.jpountz.lz4.LZ4FrameOutputStream;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.zip.Deflater;
import java.util.zip.Inflater;

public final class Codec {
    public static final int NONE = 0;
    public static final int ZLIB = 1;
    public static final int ZSTD = 2;
    public static final int LZ4 = 3;
    public static final int RLE = 4;

    public static final String[] NAMES = {"none", "zlib", "zstd", "lz4", "rle"};
    public static final int[] DEFAULT_LEVEL = {0, 6, 3, 0, 0};
    public static final int[][] LEVEL_RANGE = {
            {0, 0}, {1, 9}, {1, 22}, {0, 16}, {0, 0}
    };

    private Codec() {}

    public static int resolveLevel(int cid, int level) {
        if (cid < 0 || cid > 4)
            throw new IllegalArgumentException("unknown codec: " + cid);
        int[] r = LEVEL_RANGE[cid];
        if (level == 0) return DEFAULT_LEVEL[cid];
        return Math.max(r[0], Math.min(r[1], level));
    }

    public static String name(int cid) {
        return cid >= 0 && cid < NAMES.length ? NAMES[cid] : "unknown";
    }

    public static byte[] compress(byte[] data, int cid, int level,
                                  boolean rlePre) throws BvgCodecException {
        if (rlePre && cid != RLE) data = RleCodec.encode(data);
        int lv = resolveLevel(cid, level);
        switch (cid) {
            case NONE: return data;
            case ZLIB: {
                Deflater d = new Deflater(lv);
                d.setInput(data);
                d.finish();
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                while (!d.finished()) {
                    int n = d.deflate(buf);
                    out.write(buf, 0, n);
                }
                d.end();
                return out.toByteArray();
            }
            case ZSTD:
                return Zstd.compress(data, lv);
            case LZ4: {
                var bo = new java.io.ByteArrayOutputStream();
                try (var out = new net.jpountz.lz4.LZ4FrameOutputStream(bo)) {
                    out.write(data);
                } catch (IOException e) {
                    throw new BvgCodecException("lz4: " + e.getMessage(), e);
                }
                return bo.toByteArray();
            }
            case RLE:
                return RleCodec.encode(data);
            default:
                throw new BvgCodecException("unknown codec: " + cid);
        }
    }

    public static byte[] decompress(byte[] data, int cid, int level,
                                    boolean rlePre, int rawHint)
            throws BvgCodecException {
        byte[] out;
        switch (cid) {
            case NONE: out = data; break;
            case ZLIB: {
                Inflater inf = new Inflater();
                inf.setInput(data);
                ByteArrayOutputStream bo = new ByteArrayOutputStream(
                        rawHint > 0 ? rawHint : data.length * 2);
                byte[] buf = new byte[8192];
                try {
                    while (!inf.finished()) {
                        int n = inf.inflate(buf);
                        if (n == 0) {
                            if (inf.needsInput() || inf.needsDictionary()) break;
                        }
                        bo.write(buf, 0, n);
                    }
                } catch (Exception e) {
                    throw new BvgCodecException("zlib: " + e.getMessage(), e);
                } finally { inf.end(); }
                out = bo.toByteArray();
                break;
            }
            case ZSTD: {
                long sz = Zstd.getFrameContentSize(data);
                if (sz <= 0) sz = rawHint > 0 ? rawHint : data.length * 4;
                byte[] r = new byte[(int) sz];
                long n = Zstd.decompress(r, data);
                if (Zstd.isError(n))
                    throw new BvgCodecException("zstd: " + Zstd.getErrorName(n));
                if (n == r.length) {
                    out = r;
                } else {
                    byte[] trimmed = new byte[(int) n];
                    System.arraycopy(r, 0, trimmed, 0, (int) n);
                    out = trimmed;
                }
                break;
            }
            case LZ4: {
                byte[] outL;
                try (var in = new LZ4FrameInputStream(
                        new ByteArrayInputStream(data));
                     var bo = new ByteArrayOutputStream()) {
                    in.transferTo(bo);
                    outL = bo.toByteArray();
                } catch (IOException e) {
                    throw new BvgCodecException("lz4: " + e.getMessage(), e);
                }
                out = outL;
                break;
            }
            case RLE:
                return RleCodec.decode(data);
            default:
                throw new BvgCodecException("unknown codec: " + cid);
        }
        if (rlePre && cid != RLE) out = RleCodec.decode(out);
        return out;
    }
}