package io.bvg;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public final class RecompressCmd {
    private RecompressCmd() {}

    public static void main(String[] args) throws Exception {
        String input = null, output = null;
        String codecName = "auto";
        int level = 0;
        boolean perChunk = true;
        boolean rlePre = true;

        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "--codec" -> codecName = args[++i];
                case "--level" -> level = Integer.parseInt(args[++i]);
                case "--global-codec" -> perChunk = false;
                case "--per-chunk" -> perChunk = true;
                case "--no-rle-pre" -> rlePre = false;
                default -> {
                    if (input == null) input = args[i];
                    else if (output == null) output = args[i];
                }
            }
        }
        if (input == null || output == null) {
            System.err.println("usage: bvg recompress <in> <out> [--codec X] [--level N]");
            System.exit(2);
        }

        BvgFile f = BvgFile.read(Paths.get(input));
        int cid = -1;
        if (!codecName.equals("auto")) {
            cid = Codec.NAMES.length;
            for (int i = 0; i < Codec.NAMES.length; i++) {
                if (Codec.NAMES[i].equals(codecName)) { cid = i; break; }
            }
            if (cid == Codec.NAMES.length) {
                System.err.println("unknown codec: " + codecName);
                System.exit(2);
            }
        }
        for (String t : f.chunks.keySet()) {
            f.meta.put(t, new BvgFile.ChunkMeta(cid >= 0 ? cid : 0, level, rlePre));
        }
        f.write(Paths.get(output), perChunk, rlePre, cid >= 0 ? cid : 0, level);
        long inSize = Files.size(Paths.get(input));
        long outSize = Files.size(Paths.get(output));
        System.out.printf("%s  %d B%n%s  %d B  (%.2fx smaller)%n",
                input, inSize, output, outSize,
                outSize > 0 ? (double) inSize / outSize : 0);
    }
}