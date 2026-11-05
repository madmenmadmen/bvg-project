package io.bvg;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;

public final class ListCmd {
    private ListCmd() {}

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.err.println("usage: bvg list <file>");
            System.exit(2);
        }
        Path p = Paths.get(args[0]);
        byte[] data = Files.readAllBytes(p);
        var h = Generated.unpackHeader(data, 0);
        boolean perChunk = (h.flags & Generated.FLAG_PERCHUNK) != 0;
        List<Generated.ChunkEntry> entries = Generated.unpackChunkTable(
                data, Generated.HEADER_SIZE, h.chunkCount, perChunk);
        int dataStart = Generated.HEADER_SIZE
                + (perChunk ? Generated.CHUNK_SIZE_P : Generated.CHUNK_SIZE_S)
                * h.chunkCount;

        System.out.println("BVG v" + h.version + "  " + p + "  "
                + h.width + "x" + h.height + "  chunks=" + h.chunkCount
                + "  per_chunk=" + perChunk);
        System.out.println();
        System.out.printf("%-6s %-6s %4s %4s %10s %10s %9s%n",
                "type", "codec", "lvl", "pre", "size", "raw", "comp/raw");
        System.out.println("-".repeat(60));

        long totalC = 0, totalR = 0;
        for (var e : entries) {
            String t = new String(e.type,
                    java.nio.charset.StandardCharsets.US_ASCII);
            int cid = perChunk ? e.codecId : h.codecId;
            int lv = perChunk ? e.level : h.level;
            boolean pre = perChunk
                    && (e.flags & Generated.CHUNK_PRE_RLE) != 0;
            byte[] comp = new byte[e.size];
            System.arraycopy(data, dataStart + e.offset, comp, 0, e.size);
            long rawSize = -1;
            try {
                rawSize = Codec.decompress(comp, cid, lv, pre, 0).length;
            } catch (Exception ex) {
                // ignore
            }
            double ratio = rawSize > 0 ? (double) e.size / rawSize : 0;
            System.out.printf("%-6s %-6s %4d %4s %10d %10s %8.2fx%n",
                    t, Codec.name(cid), lv, pre ? "yes" : "no",
                    e.size, rawSize > 0 ? String.valueOf(rawSize) : "?",
                    ratio);
            totalC += e.size;
            if (rawSize > 0) totalR += rawSize;
        }
        System.out.println("-".repeat(60));
        System.out.printf("%-6s %-6s %4s %4s %10d %10d %8.2fx%n",
                "TOTAL", "", "", "", totalC, totalR,
                totalR > 0 ? (double) totalC / totalR : 0);
    }
}