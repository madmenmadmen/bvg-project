package io.bvg;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class BvgFile {
    public static final String VERSION = "1.0.0";

    public int width;
    public int height;

    public final Map<String, byte[]> chunks = new LinkedHashMap<>();
    public final Map<String, ChunkMeta> meta = new HashMap<>();
    public final List<String> order = new ArrayList<>();

    public static final class ChunkMeta {
        public int codec;
        public int level;
        public boolean usePre;
        public ChunkMeta() {}
        public ChunkMeta(int c, int l, boolean p) {
            codec = c; level = l; usePre = p;
        }
    }

    // --- lazy state ---
    private byte[] raw;
    private List<Generated.ChunkEntry> entries = new ArrayList<>();
    private int dataStart;
    private boolean perChunk;
    private int headerCodec;
    private int headerLevel;
    private final Set<String> dirty = new HashSet<>();

    public BvgFile(int w, int h) {
        this.width = w;
        this.height = h;
    }

    public static BvgFile read(Path path) throws IOException, BvgException {
        byte[] data = Files.readAllBytes(path);
        Generated.Header h = Generated.unpackHeader(data, 0);

        boolean pc = (h.flags & Generated.FLAG_PERCHUNK) != 0;
        int rec = pc ? Generated.CHUNK_SIZE_P : Generated.CHUNK_SIZE_S;
        int tableOff = Generated.HEADER_SIZE;
        int ds = tableOff + rec * h.chunkCount;

        if (ds > data.length) {
            throw new BvgFormatException("chunk table truncated");
        }

        List<Generated.ChunkEntry> es = Generated.unpackChunkTable(
                data, tableOff, h.chunkCount, pc);

        BvgFile f = new BvgFile(h.width, h.height);
        f.raw = data;
        f.entries = es;
        f.dataStart = ds;
        f.perChunk = pc;
        f.headerCodec = h.codecId;
        f.headerLevel = h.level;

        for (Generated.ChunkEntry e : es) {
            String t = new String(e.type, StandardCharsets.US_ASCII);
            int cid = pc ? e.codecId : h.codecId;
            int lv  = pc ? e.level   : h.level;
            boolean pre = pc && (e.flags & Generated.CHUNK_PRE_RLE) != 0;
            f.meta.put(t, new ChunkMeta(cid, lv, pre));
            f.order.add(t);
        }
        return f;
    }

    public byte[] getChunk(String type) throws BvgException {
        if (chunks.containsKey(type)) {
            return chunks.get(type);
        }
        if (raw == null) {
            return null;
        }
        for (Generated.ChunkEntry e : entries) {
            String t = new String(e.type, StandardCharsets.US_ASCII);
            if (!t.equals(type)) continue;
            int cid = perChunk ? e.codecId : headerCodec;
            int lv  = perChunk ? e.level   : headerLevel;
            boolean pre = perChunk && (e.flags & Generated.CHUNK_PRE_RLE) != 0;
            int abs = dataStart + e.offset;
            if (abs + e.size > raw.length) {
                throw new BvgChunkException("chunk out of bounds");
            }
            byte[] comp = Arrays.copyOfRange(raw, abs, abs + e.size);
            byte[] r;
            try {
                r = Codec.decompress(comp, cid, lv, pre, 0);
            } catch (BvgCodecException ex) {
                throw new RuntimeException(ex);
            }
            chunks.put(type, r);
            return r;
        }
        return null;
    }

    public void setChunk(String type, byte[] payload,
                     int codec, int level, boolean usePre) {
        if (!chunks.containsKey(type) && !order.contains(type)) {
            order.add(type);
        }
        chunks.put(type, payload);
        meta.put(type, new ChunkMeta(codec, level, usePre));
        dirty.add(type);
    }

    public void write(Path path, boolean perChunk, boolean rlePre,
                      int defaultCodec, int defaultLevel)
            throws IOException, BvgCodecException {

        List<String> order = new ArrayList<>(chunks.keySet());

        List<byte[]> comps = new ArrayList<>();
        List<Integer> cids = new ArrayList<>();
        List<Integer> lvs  = new ArrayList<>();
        List<Boolean> pres = new ArrayList<>();
        long totalRaw = 0;

        for (String t : order) {
            byte[] comp;
            int cid, lv;
            boolean usePre;

            if (raw != null && !dirty.contains(t)) {
                // Копируем как есть из исходного файла.
                Generated.ChunkEntry ent = findEntry(t);
                if (ent == null) continue;
                int abs = dataStart + ent.offset;
                comp = Arrays.copyOfRange(raw, abs, abs + ent.size);
                cid = perChunk ? ent.codecId : headerCodec;
                lv  = perChunk ? ent.level   : headerLevel;
                usePre = perChunk && (ent.flags & Generated.CHUNK_PRE_RLE) != 0;
                // rawLen нужен для total_uncompressed — декомпрессим.
                byte[] decoded;
                try {
                    decoded = Codec.decompress(comp, cid, lv, usePre, 0);
                } catch (BvgCodecException ex) {
                    throw ex;
                }
                totalRaw += decoded.length;
            } else {
                // Кодируем из chunks[t].
                byte[] rawPayload = chunks.get(t);
                if (rawPayload == null) continue;
                ChunkMeta m = meta.get(t);
                if (!perChunk) {
                    usePre = false;
                    cid = defaultCodec >= 0 ? defaultCodec : Codec.NONE;
                    lv = defaultLevel;
                } else if (m != null) {
                    cid = m.codec;
                    lv = m.level;
                    usePre = m.usePre;
                } else {
                    cid = defaultCodec >= 0 ? defaultCodec : Codec.NONE;
                    lv = defaultLevel;
                    usePre = rlePre;
                }
                comp = Codec.compress(rawPayload, cid, lv, usePre);
                totalRaw += rawPayload.length;
            }

            comps.add(comp);
            cids.add(cid);
            lvs.add(lv);
            pres.add(usePre);
        }

        // SPEC 7: FLAG_U32XY is always set by writers in v1
        int flags = Generated.FLAG_U32XY;
        if (perChunk) flags |= Generated.FLAG_PERCHUNK;
        if (chunks.containsKey("THMB")) flags |= Generated.FLAG_HAS_THUMB;
        if (chunks.containsKey("META")) flags |= Generated.FLAG_HAS_META;

        int globalCodec = perChunk || cids.isEmpty() ? 0 : cids.get(0);
        int globalLevel = perChunk || lvs.isEmpty() ? 0 : lvs.get(0);

        Generated.Header h = new Generated.Header();
        h.flags = flags;
        h.width = width;
        h.height = height;
        h.chunkCount = order.size();
        h.codecId = globalCodec;
        h.level = globalLevel;
        h.totalUncompressed = totalRaw;
        byte[] header = Generated.packHeader(h);

        int rec = perChunk ? Generated.CHUNK_SIZE_P : Generated.CHUNK_SIZE_S;
        int ds = Generated.HEADER_SIZE + rec * order.size();

        List<Generated.ChunkEntry> entries = new ArrayList<>();
        int off = 0;
        for (int i = 0; i < order.size(); i++) {
            Generated.ChunkEntry e = new Generated.ChunkEntry();
            byte[] t = order.get(i).getBytes(StandardCharsets.US_ASCII);
            System.arraycopy(t, 0, e.type, 0, 4);
            e.offset = off;
            e.size = comps.get(i).length;
            e.codecId = cids.get(i);
            e.level = lvs.get(i);
            e.flags = pres.get(i) ? Generated.CHUNK_PRE_RLE : 0;
            entries.add(e);
            off += comps.get(i).length;
        }

        byte[] table = Generated.packChunkTable(entries, perChunk);
        byte[] blob = new byte[off];
        int p = 0;
        for (byte[] c : comps) {
            System.arraycopy(c, 0, blob, p, c.length);
            p += c.length;
        }

        byte[] all = new byte[ds + blob.length];
        System.arraycopy(header, 0, all, 0, Generated.HEADER_SIZE);
        System.arraycopy(table, 0, all, Generated.HEADER_SIZE, table.length);
        System.arraycopy(blob, 0, all, ds, blob.length);
        Files.write(path, all);
    }

    private Generated.ChunkEntry findEntry(String type) {
        for (Generated.ChunkEntry e : entries) {
            String t = new String(e.type, StandardCharsets.US_ASCII);
            if (t.equals(type)) return e;
        }
        return null;
    }
}