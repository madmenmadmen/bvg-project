// GENERATED FILE - DO NOT EDIT
// Source: bvg-spec/schema.yaml
package io.bvg;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public final class Generated {
    private Generated() {}

    public static final ByteOrder LE = ByteOrder.LITTLE_ENDIAN;
    public static final byte[] MAGIC = new byte[]{ 'B', 'V', 'G', 0 };
    public static final int VERSION = 1;
    public static final int HEADER_SIZE = 32;
    public static final int CHUNK_SIZE_S = 12;
    public static final int CHUNK_SIZE_P = 15;

    public static final int FLAG_PERCHUNK = 1 << 0;
    public static final int FLAG_U32XY = 1 << 1;
    public static final int FLAG_HAS_THUMB = 1 << 2;
    public static final int FLAG_HAS_META = 1 << 3;

    public static final int CHUNK_PRE_RLE = 1 << 0;

    // ---- Header ----

    public static final class Header {
        public byte[] magic = MAGIC.clone();
        public int version = VERSION;
        public int flags;
        public int width;
        public int height;
        public int chunkCount;
        public int codecId;
        public int level;
        public int reserved;
        public long totalUncompressed;
    }

    public static byte[] packHeader(Header h) {
        ByteBuffer b = ByteBuffer.allocate(HEADER_SIZE).order(LE);
        b.put(h.magic, 0, 4);
        b.putShort((short) h.version);
        b.putShort((short) h.flags);
        b.putInt(h.width);
        b.putInt(h.height);
        b.putInt(h.chunkCount);
        b.put((byte) h.codecId);
        b.put((byte) h.level);
        b.putShort((short) h.reserved);
        b.putLong(h.totalUncompressed);
        return b.array();
    }

    public static Header unpackHeader(byte[] data, int off)
            throws BvgFormatException, BvgVersionException {
        if (data.length - off < HEADER_SIZE)
            throw new BvgFormatException("file too small for BVG header");
        ByteBuffer b = ByteBuffer.wrap(data, off, HEADER_SIZE).order(LE);
        Header h = new Header();
        h.magic = new byte[4];
        b.get(h.magic);
        if (h.magic[0] != 'B' || h.magic[1] != 'V'
                || h.magic[2] != 'G' || h.magic[3] != 0)
            throw new BvgFormatException("bad magic");
        h.version = b.getShort() & 0xFFFF;
        if (h.version != VERSION)
            throw new BvgVersionException("unsupported version: " + h.version);
        h.flags = b.getShort() & 0xFFFF;
        h.width = b.getInt();
        h.height = b.getInt();
        h.chunkCount = b.getInt();
        h.codecId = b.get() & 0xFF;
        h.level = b.get() & 0xFF;
        h.reserved = b.getShort() & 0xFFFF;
        if (h.reserved != 0)
            throw new BvgFormatException("reserved must be zero");
        h.totalUncompressed = b.getLong();
        return h;
    }

    // ---- Chunk table ----

    public static final class ChunkEntry {
        public byte[] type = new byte[4];
        public int offset;
        public int size;
        public int codecId;
        public int level;
        public int flags;
    }

    public static byte[] packChunkTable(List<ChunkEntry> entries, boolean perChunk) {
        int rec = perChunk ? CHUNK_SIZE_P : CHUNK_SIZE_S;
        ByteBuffer b = ByteBuffer.allocate(entries.size() * rec).order(LE);
        for (ChunkEntry e : entries) {
            b.put(e.type, 0, 4);
            b.putInt(e.offset);
            b.putInt(e.size);
            if (perChunk) {
                b.put((byte) e.codecId);
                b.put((byte) e.level);
                b.put((byte) e.flags);
            }
        }
        return b.array();
    }

    public static List<ChunkEntry> unpackChunkTable(byte[] data, int off,
            int count, boolean perChunk) throws BvgFormatException {
        int rec = perChunk ? CHUNK_SIZE_P : CHUNK_SIZE_S;
        if (off + (long) count * rec > data.length)
            throw new BvgFormatException("chunk table truncated");
        ByteBuffer b = ByteBuffer.wrap(data, off, count * rec).order(LE);
        List<ChunkEntry> out = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            ChunkEntry e = new ChunkEntry();
            b.get(e.type);
            e.offset = b.getInt();
            e.size = b.getInt();
            if (perChunk) {
                e.codecId = b.get() & 0xFF;
                e.level = b.get() & 0xFF;
                e.flags = b.get() & 0xFF;
            }
            out.add(e);
        }
        return out;
    }

    // ---- COLR ----

    public static byte[] packColr(List<int[]> colors) {
        ByteBuffer b = ByteBuffer.allocate(2 + colors.size() * 4).order(LE);
        b.putShort((short) colors.size());
        for (int[] c : colors) {
            b.put((byte) c[0]); b.put((byte) c[1]);
            b.put((byte) c[2]); b.put((byte) c[3]);
        }
        return b.array();
    }

    public static List<int[]> unpackColr(byte[] raw) throws BvgChunkException {
        if (raw.length < 2) throw new BvgChunkException("COLR: truncated");
        ByteBuffer b = ByteBuffer.wrap(raw).order(LE);
        int n = b.getShort() & 0xFFFF;
        if (2 + n * 4 > raw.length) throw new BvgChunkException("COLR: truncated");
        List<int[]> out = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            out.add(new int[]{
                b.get() & 0xFF, b.get() & 0xFF,
                b.get() & 0xFF, b.get() & 0xFF
            });
        }
        return out;
    }

    // ---- GRAD ----

    public static byte[] packGrad(List<Gradient> grads) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        ByteBuffer h = ByteBuffer.allocate(2).order(LE);
        h.putShort((short) grads.size());
        out.writeBytes(h.array());
        for (Gradient g : grads) {
            int flags = (g.transform != null) ? 1 : 0;
            ByteBuffer hdr = ByteBuffer.allocate(4).order(LE);
            hdr.put((byte) g.kind).put((byte) flags)
               .putShort((short) g.stops.size());
            out.writeBytes(hdr.array());
            for (Gradient.Stop s : g.stops) {
                ByteBuffer sb = ByteBuffer.allocate(6).order(LE);
                sb.putFloat(s.offset).putShort((short) s.colorIdx);
                out.writeBytes(sb.array());
                if (s.colorIdx == 0xFFFF) {
                    out.write((byte) s.rgba[0]);
                    out.write((byte) s.rgba[1]);
                    out.write((byte) s.rgba[2]);
                    out.write((byte) s.rgba[3]);
                }
            }
            ByteBuffer geo = ByteBuffer.allocate(24).order(LE);
            if (g.kind == Gradient.LINEAR) {
                for (int i = 0; i < 4; i++) geo.putFloat((float) g.geometry[i]);
                out.write(geo.array(), 0, 16);
            } else if (g.kind == Gradient.RADIAL) {
                for (int i = 0; i < 5; i++) geo.putFloat((float) g.geometry[i]);
                out.write(geo.array(), 0, 20);
            } else if (g.kind == Gradient.CONIC) {
                for (int i = 0; i < 4; i++) geo.putFloat((float) g.geometry[i]);
                out.write(geo.array(), 0, 16);
            } else if (g.kind == Gradient.PATTERN) {
                geo.putShort((short) ((int) g.geometry[0]));
                for (int i = 1; i < 5; i++) geo.putFloat((float) g.geometry[i]);
                out.write(geo.array(), 0, 18);
            } else {
                throw new IllegalArgumentException("bad gradient kind");
            }
            if (flags == 1) {
                ByteBuffer tr = ByteBuffer.allocate(24).order(LE);
                for (double v : g.transform) tr.putFloat((float) v);
                out.writeBytes(tr.array());
            }
        }
        return out.toByteArray();
    }

    public static List<Gradient> unpackGrad(byte[] raw) throws BvgChunkException {
        if (raw.length < 2) throw new BvgChunkException("GRAD: truncated");
        ByteBuffer b = ByteBuffer.wrap(raw).order(LE);
        int n = b.getShort() & 0xFFFF;
        List<Gradient> out = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            if (b.remaining() < 4) throw new BvgChunkException("GRAD: truncated");
            int kind = b.get() & 0xFF;
            int flags = b.get() & 0xFF;
            int sc = b.getShort() & 0xFFFF;
            List<Gradient.Stop> stops = new ArrayList<>(sc);
            for (int j = 0; j < sc; j++) {
                if (b.remaining() < 6) throw new BvgChunkException("GRAD: truncated stop");
                float off = b.getFloat();
                int ci = b.getShort() & 0xFFFF;
                Gradient.Stop st = new Gradient.Stop();
                st.offset = off;
                st.colorIdx = ci;
                if (ci == 0xFFFF) {
                    if (b.remaining() < 4) throw new BvgChunkException("GRAD: truncated inline");
                    st.rgba = new int[]{
                        b.get() & 0xFF, b.get() & 0xFF,
                        b.get() & 0xFF, b.get() & 0xFF
                    };
                }
                stops.add(st);
            }
            double[] geo;
            if (kind == Gradient.LINEAR || kind == Gradient.CONIC) {
                if (b.remaining() < 16) throw new BvgChunkException("GRAD: truncated geom");
                geo = new double[4];
                for (int j = 0; j < 4; j++) geo[j] = b.getFloat();
            } else if (kind == Gradient.RADIAL) {
                if (b.remaining() < 20) throw new BvgChunkException("GRAD: truncated geom");
                geo = new double[5];
                for (int j = 0; j < 5; j++) geo[j] = b.getFloat();
            } else if (kind == Gradient.PATTERN) {
                if (b.remaining() < 18) throw new BvgChunkException("GRAD: truncated geom");
                geo = new double[5];
                geo[0] = b.getShort() & 0xFFFF;
                for (int j = 1; j < 5; j++) geo[j] = b.getFloat();
            } else {
                throw new BvgChunkException("GRAD: unknown kind " + kind);
            }
            double[] tr = null;
            if ((flags & 1) != 0) {
                if (b.remaining() < 24) throw new BvgChunkException("GRAD: truncated transform");
                tr = new double[6];
                for (int j = 0; j < 6; j++) tr[j] = b.getFloat();
            }
            out.add(new Gradient(kind, stops, geo, tr));
        }
        return out;
    }

    // ---- STYL ----

    public static byte[] packStyl(List<Style> styles) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        ByteBuffer h = ByteBuffer.allocate(2).order(LE);
        h.putShort((short) styles.size());
        out.writeBytes(h.array());
        for (Style s : styles) {
            int sz = 2 + 2 + 2 + 4 + 1 + 1 + 4 + 1 + s.dash.size() * 4 + 1 + 1;
            ByteBuffer b = ByteBuffer.allocate(sz).order(LE);
            b.put((byte) s.fillKind);
            b.putShort((short) s.fillIdx);
            b.put((byte) s.strokeKind);
            b.putShort((short) s.strokeIdx);
            b.putFloat(s.strokeWidth);
            b.put((byte) s.cap);
            b.put((byte) s.join);
            b.putFloat(s.miterLimit);
            b.put((byte) s.dash.size());
            for (double d : s.dash) b.putFloat((float) d);
            b.put((byte) s.opacity);
            b.put((byte) s.blendMode);
            out.writeBytes(b.array());
        }
        return out.toByteArray();
    }

    public static List<Style> unpackStyl(byte[] raw) throws BvgChunkException {
        if (raw.length < 2) throw new BvgChunkException("STYL: truncated");
        ByteBuffer b = ByteBuffer.wrap(raw).order(LE);
        int n = b.getShort() & 0xFFFF;
        List<Style> out = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            if (b.remaining() < 16) throw new BvgChunkException("STYL: truncated");
            Style s = new Style();
            s.fillKind = b.get() & 0xFF;
            s.fillIdx = b.getShort() & 0xFFFF;
            s.strokeKind = b.get() & 0xFF;
            s.strokeIdx = b.getShort() & 0xFFFF;
            s.strokeWidth = b.getFloat();
            s.cap = b.get() & 0xFF;
            s.join = b.get() & 0xFF;
            s.miterLimit = b.getFloat();
            int dc = b.get() & 0xFF;
            if (b.remaining() < dc * 4 + 2) throw new BvgChunkException("STYL: truncated dash");
            for (int j = 0; j < dc; j++) s.dash.add((double) b.getFloat());
            s.opacity = b.get() & 0xFF;
            s.blendMode = b.get() & 0xFF;
            out.add(s);
        }
        return out;
    }

    // ---- NODE ----

    public static byte[] packNode(List<Node> nodes) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (Node nd : nodes) {
            int flags = (nd.transform != null) ? Node.HAS_TRANSFORM : 0;
            ByteBuffer h = ByteBuffer.allocate(6).order(LE);
            h.put((byte) nd.type).put((byte) flags)
             .putShort((short) nd.style).putShort((short) nd.depth);
            out.writeBytes(h.array());
            if (flags == Node.HAS_TRANSFORM) {
                if (nd.transform.length != 6)
                    throw new IllegalArgumentException("transform must have 6 values");
                ByteBuffer tr = ByteBuffer.allocate(24).order(LE);
                for (double v : nd.transform) tr.putFloat((float) v);
                out.writeBytes(tr.array());
            }
            switch (nd.type) {
                case Node.GROUP:
                case Node.DEF: {
                    ByteBuffer b = ByteBuffer.allocate(2).order(LE);
                    b.putShort((short) nd.childCount);
                    out.writeBytes(b.array());
                    break;
                }
                case Node.RECT: {
                    ByteBuffer b = ByteBuffer.allocate(16).order(LE);
                    for (double v : nd.rect) b.putFloat((float) v);
                    out.writeBytes(b.array());
                    break;
                }
                case Node.ELLIPSE: {
                    ByteBuffer b = ByteBuffer.allocate(16).order(LE);
                    for (double v : nd.ellipse) b.putFloat((float) v);
                    out.writeBytes(b.array());
                    break;
                }
                case Node.LINE: {
                    ByteBuffer b = ByteBuffer.allocate(16).order(LE);
                    for (double v : nd.line) b.putFloat((float) v);
                    out.writeBytes(b.array());
                    break;
                }
                case Node.POLYLINE:
                case Node.POLYGON: {
                    ByteBuffer hb = ByteBuffer.allocate(2).order(LE);
                    hb.putShort((short) nd.points.length);
                    out.writeBytes(hb.array());
                    for (double[] p : nd.points) {
                        ByteBuffer pb = ByteBuffer.allocate(8).order(LE);
                        pb.putFloat((float) p[0]).putFloat((float) p[1]);
                        out.writeBytes(pb.array());
                    }
                    break;
                }
                case Node.PATH: {
                    ByteBuffer hb = ByteBuffer.allocate(2).order(LE);
                    hb.putShort((short) nd.path.length);
                    out.writeBytes(hb.array());
                    for (Node.PathCmd c : nd.path) {
                        out.write((byte) c.op);
                        if (c.op == 4) {
                            if (c.args.length != 7)
                                throw new IllegalArgumentException("arc args must be 7");
                            ByteBuffer ab = ByteBuffer.allocate(22).order(LE);
                            ab.putFloat((float) c.args[0]);
                            ab.putFloat((float) c.args[1]);
                            ab.putFloat((float) c.args[2]);
                            ab.put((byte) c.args[3]);
                            ab.put((byte) c.args[4]);
                            ab.putFloat((float) c.args[5]);
                            ab.putFloat((float) c.args[6]);
                            out.writeBytes(ab.array());
                        } else {
                            ByteBuffer ab = ByteBuffer.allocate(c.args.length * 4)
                                .order(LE);
                            for (double v : c.args) ab.putFloat((float) v);
                            out.writeBytes(ab.array());
                        }
                    }
                    break;
                }
                case Node.TEXT: {
                    byte[] s = nd.text.getBytes(StandardCharsets.UTF_8);
                    ByteBuffer hb = ByteBuffer.allocate(12).order(LE);
                    hb.putShort((short) nd.font).putFloat((float) nd.tx)
                      .putFloat((float) nd.ty).putShort((short) s.length);
                    out.writeBytes(hb.array());
                    out.writeBytes(s);
                    break;
                }
                case Node.IMAGE: {
                    ByteBuffer b = ByteBuffer.allocate(18).order(LE);
                    b.putShort((short) nd.imageIdx);
                    for (double v : nd.rect) b.putFloat((float) v);
                    out.writeBytes(b.array());
                    break;
                }
                case Node.USE: {
                    ByteBuffer b = ByteBuffer.allocate(10).order(LE);
                    b.putShort((short) nd.defIdx)
                     .putFloat((float) nd.tx).putFloat((float) nd.ty);
                    out.writeBytes(b.array());
                    break;
                }
                default:
                    throw new IllegalArgumentException("bad node type");
            }
        }
        return out.toByteArray();
    }

    public static List<Node> unpackNode(byte[] raw) throws BvgChunkException {
        List<Node> out = new ArrayList<>();
        ByteBuffer b = ByteBuffer.wrap(raw).order(LE);
        while (b.remaining() > 0) {
            if (b.remaining() < 6) throw new BvgChunkException("NODE: truncated header");
            Node nd = new Node();
            nd.type = b.get() & 0xFF;
            nd.flags = b.get() & 0xFF;
            nd.style = b.getShort() & 0xFFFF;
            nd.depth = b.getShort() & 0xFFFF;
            if ((nd.flags & Node.HAS_TRANSFORM) != 0) {
                if (b.remaining() < 24) throw new BvgChunkException("NODE: truncated transform");
                nd.transform = new double[6];
                for (int i = 0; i < 6; i++) nd.transform[i] = b.getFloat();
            }
            switch (nd.type) {
                case Node.GROUP:
                case Node.DEF:
                    if (b.remaining() < 2) throw new BvgChunkException("NODE: truncated group");
                    nd.childCount = b.getShort() & 0xFFFF;
                    break;
                case Node.RECT:
                    if (b.remaining() < 16) throw new BvgChunkException("NODE: truncated rect");
                    nd.rect = new double[4];
                    for (int i = 0; i < 4; i++) nd.rect[i] = b.getFloat();
                    break;
                case Node.ELLIPSE:
                    if (b.remaining() < 16) throw new BvgChunkException("NODE: truncated ellipse");
                    nd.ellipse = new double[4];
                    for (int i = 0; i < 4; i++) nd.ellipse[i] = b.getFloat();
                    break;
                case Node.LINE:
                    if (b.remaining() < 16) throw new BvgChunkException("NODE: truncated line");
                    nd.line = new double[4];
                    for (int i = 0; i < 4; i++) nd.line[i] = b.getFloat();
                    break;
                case Node.POLYLINE:
                case Node.POLYGON: {
                    if (b.remaining() < 2) throw new BvgChunkException("NODE: truncated poly");
                    int n = b.getShort() & 0xFFFF;
                    if (b.remaining() < n * 8) throw new BvgChunkException("NODE: truncated points");
                    nd.points = new double[n][2];
                    for (int i = 0; i < n; i++) {
                        nd.points[i][0] = b.getFloat();
                        nd.points[i][1] = b.getFloat();
                    }
                    break;
                }
                case Node.PATH: {
                    if (b.remaining() < 2) throw new BvgChunkException("NODE: truncated path");
                    int n = b.getShort() & 0xFFFF;
                    nd.path = new Node.PathCmd[n];
                    for (int i = 0; i < n; i++) {
                        if (b.remaining() < 1) throw new BvgChunkException("NODE: truncated op");
                        int op = b.get() & 0xFF;
                        int argc = pathArgCount(op);
                        double[] args = new double[argc];
                        if (op == 4) {
                            if (b.remaining() < 22) throw new BvgChunkException("NODE: truncated arc");
                            for (int j = 0; j < 3; j++) args[j] = b.getFloat();
                            args[3] = b.get() & 0xFF;
                            args[4] = b.get() & 0xFF;
                            args[5] = b.getFloat();
                            args[6] = b.getFloat();
                        } else {
                            if (b.remaining() < argc * 4) throw new BvgChunkException("NODE: truncated args");
                            for (int j = 0; j < argc; j++)
                                args[j] = b.getFloat();
                        }
                        nd.path[i] = new Node.PathCmd(op, args);
                    }
                    break;
                }
                case Node.TEXT: {
                    if (b.remaining() < 12) throw new BvgChunkException("NODE: truncated text");
                    nd.font = b.getShort() & 0xFFFF;
                    nd.tx = b.getFloat();
                    nd.ty = b.getFloat();
                    int len = b.getShort() & 0xFFFF;
                    if (b.remaining() < len) throw new BvgChunkException("NODE: truncated text body");
                    byte[] s = new byte[len];
                    b.get(s);
                    nd.text = new String(s, StandardCharsets.UTF_8);
                    break;
                }
                case Node.IMAGE: {
                    if (b.remaining() < 18) throw new BvgChunkException("NODE: truncated image");
                    nd.imageIdx = b.getShort() & 0xFFFF;
                    nd.rect = new double[4];
                    for (int i = 0; i < 4; i++) nd.rect[i] = b.getFloat();
                    break;
                }
                case Node.USE:
                    if (b.remaining() < 10) throw new BvgChunkException("NODE: truncated use");
                    nd.defIdx = b.getShort() & 0xFFFF;
                    nd.tx = b.getFloat();
                    nd.ty = b.getFloat();
                    break;
                default:
                    throw new BvgChunkException(
                        "NODE: unknown type " + nd.type);
            }
            out.add(nd);
        }
        return out;
    }

    // ---- IMGS ----
    public static final int IMG_PNG = 0;
    public static final int IMG_JPEG = 1;
    public static final int IMG_RAW_RGBA = 2;

    public static final class Image {
        public int format;
        public int width;
        public int height;
        public byte[] data;
    }

    public static List<Image> unpackImages(byte[] raw) throws BvgChunkException {
        if (raw.length < 2) throw new BvgChunkException("IMGS: truncated");
        java.nio.ByteBuffer b = java.nio.ByteBuffer.wrap(raw).order(LE);
        int n = b.getShort() & 0xFFFF;
        List<Image> out = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            if (b.remaining() < 9) throw new BvgChunkException("IMGS: truncated header");
            Image im = new Image();
            im.format = b.get() & 0xFF;
            im.width = b.getShort() & 0xFFFF;
            im.height = b.getShort() & 0xFFFF;
            int dlen = b.getInt();
            if (b.remaining() < dlen) throw new BvgChunkException("IMGS: truncated data");
            im.data = new byte[dlen];
            b.get(im.data);
            out.add(im);
        }
        return out;
    }

    private static int pathArgCount(int op) {
        switch (op) {
            case 0: case 1: return 2;
            case 2: return 4;
            case 3: return 6;
            case 4: return 7;
            case 5: return 0;
            default: return 0;
        }
    }
}