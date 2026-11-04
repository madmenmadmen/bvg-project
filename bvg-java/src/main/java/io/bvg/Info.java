package io.bvg;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public final class Info {
    private Info() {}

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.err.println("usage: bvg info <file>");
            System.exit(2);
        }
        Path p = Paths.get(args[0]);
        long fileSize = Files.size(p);
        BvgFile f = BvgFile.read(p);
        var h = Generated.unpackHeader(Files.readAllBytes(p), 0);

        boolean perChunk = (h.flags & Generated.FLAG_PERCHUNK) != 0;
        boolean hasThumb = (h.flags & Generated.FLAG_HAS_THUMB) != 0;
        boolean hasMeta = (h.flags & Generated.FLAG_HAS_META) != 0;

        System.out.println("BVG v" + h.version + "  " + p);
        System.out.println("  format kind          vector");
        System.out.println("  magic                b'BVG\\x00'");
        System.out.println("  canvas               " + h.width + " x " + h.height);
        System.out.println("  chunks               " + h.chunkCount);
        System.out.println("  global codec         " + Codec.name(h.codecId)
                + " (id=" + h.codecId + ", level=" + h.level + ")");
        System.out.println("  per-chunk            " + perChunk);
        System.out.println("  has thumb            " + hasThumb);
        System.out.println("  has meta             " + hasMeta);
        System.out.println("  total uncompressed   " + h.totalUncompressed + " B");
        System.out.println("  file size            " + fileSize + " B");
    }
}