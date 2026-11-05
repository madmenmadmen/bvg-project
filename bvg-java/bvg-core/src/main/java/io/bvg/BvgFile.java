package io.bvg;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class BvgFile {
    public static final String VERSION = "1.0.0";

    public int width;
    public int height;

    public final Map<String, byte[]> chunks = new LinkedHashMap<>();
    public final Map<String, ChunkMeta> meta = new HashMap<>();

    public static final class ChunkMeta {
        public int codec;
        public int level;
        public boolean usePre;
        public ChunkMeta() {}
        public ChunkMeta(int c, int l, boolean p) {
            codec = c; level = l; usePre = p;
        }
    }

    public BvgFile(int w, int h) {
        this.width = w;
        this.height = h;
    }

    public void setChunk(String type, byte[] payload,
                         int codec, int level, boolean usePre) {
        chunks.put(type, payload);
        meta.put(type, new ChunkMeta(codec, level, usePre));
    }

    public byte[] getChunk(String type) {
        return chunks.get(type);
    }

    public void write(Path path, boolean perChunk, boolean rlePre,
                      int defaultCodec, int defaultLevel)
            throws IOException, BvgCodecException {

        List<String> order = new ArrayList<>(chunks.keySet());
        List<byte[]> comps = new ArrayList<>();
        List<Integer> cids = new ArrayList<>();
        List<Integer> lvs = new ArrayList<>();
        List<Boolean> pres = new ArrayList<>();
        long totalRaw = 0;

        for (String t : order) {
            byte[] raw = chunks.get(t);
            int cid, lv;
            boolean usePre;
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
            byte[] comp = Codec.compress(raw, cid, lv, usePre);
            comps.add(comp);
            cids.add(cid);
            lvs.add(lv);
            pres.add(usePre);
            totalRaw += raw.length;
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
        int dataStart = Generated.HEADER_SIZE + rec * order.size();

        List<Generated.ChunkEntry> entries = new ArrayList<>();
        int off = 0;
        for (int i = 0; i < order.size(); i++) {
            Generated.ChunkEntry e = new Generated.ChunkEntry();
            byte[] t = order.get(i).getBytes(
                    java.nio.charset.StandardCharsets.US_ASCII);
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

        byte[] all = new byte[dataStart + blob.length];
        System.arraycopy(header, 0, all, 0, Generated.HEADER_SIZE);
        System.arraycopy(table, 0, all, Generated.HEADER_SIZE, table.length);
        System.arraycopy(blob, 0, all, dataStart, blob.length);
        Files.write(path, all);
    }

    public static BvgFile read(Path path) throws
            IOException, BvgException {
        byte[] data = Files.readAllBytes(path);
        Generated.Header h = Generated.unpackHeader(data, 0);

        boolean perChunk = (h.flags & Generated.FLAG_PERCHUNK) != 0;
        int rec = perChunk ? Generated.CHUNK_SIZE_P : Generated.CHUNK_SIZE_S;
        int tableOff = Generated.HEADER_SIZE;
        int dataStart = tableOff + rec * h.chunkCount;

        List<Generated.ChunkEntry> entries =
                Generated.unpackChunkTable(data, tableOff,
                        h.chunkCount, perChunk);

        BvgFile f = new BvgFile(h.width, h.height);
        for (Generated.ChunkEntry e : entries) {
            String t = new String(e.type,
                    java.nio.charset.StandardCharsets.US_ASCII);
            int cid = perChunk ? e.codecId : h.codecId;
            int lv = perChunk ? e.level : h.level;
            boolean usePre = perChunk
                    ? (e.flags & Generated.CHUNK_PRE_RLE) != 0
                    : false;
            int abs = dataStart + e.offset;
            byte[] comp = new byte[e.size];
            System.arraycopy(data, abs, comp, 0, e.size);
            byte[] raw = Codec.decompress(comp, cid, lv, usePre, 0);
            f.setChunk(t, raw, cid, lv, usePre);
        }
        return f;
    }
}