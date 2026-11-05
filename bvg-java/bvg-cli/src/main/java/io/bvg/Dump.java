package io.bvg;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class Dump {
    private Dump() {}

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.err.println("usage: bvg dump <file> [-o out.json]");
            System.exit(2);
        }
        Path in = Paths.get(args[0]);
        Path out = null;
        for (int i = 1; i < args.length; i++) {
            if (args[i].equals("-o") && i + 1 < args.length) {
                out = Paths.get(args[++i]);
            }
        }

        BvgFile f = BvgFile.read(in);
        String json = toJson(in.getFileName().toString(), f);

        if (out != null) {
            Files.writeString(out, json);
            System.err.println("wrote " + out);
        } else {
            System.out.println(json);
        }
    }

    static String toJson(String fileName, BvgFile f) throws BvgException {
        StringBuilder sb = new StringBuilder();
        sb.append("{\n");
        sb.append("  \"chunks\": [\n");

        List<String> order = new ArrayList<>(f.order);
        if (order.isEmpty()) {
            order = new ArrayList<>(f.chunks.keySet());
            if (order.isEmpty() && f.meta != null && !f.meta.isEmpty()) {
                order = new ArrayList<>(f.meta.keySet());
            }
        }

        boolean first = true;
        for (String t : order) {
            byte[] raw = f.getChunk(t);   // lazy, throws BvgException
            if (raw == null) continue;

            BvgFile.ChunkMeta m = f.meta.get(t);
            int codec = m != null ? m.codec : 0;
            int level = m != null ? m.level : 0;
            boolean usePre = m != null && m.usePre;

            if (!first) sb.append(",\n");
            first = false;
            sb.append("    {\n");
            sb.append("      \"hex\": \"").append(hex(raw)).append("\",\n");
            sb.append("      \"meta\": {\n");
            sb.append("        \"codec\": ").append(codec).append(",\n");
            sb.append("        \"level\": ").append(level).append(",\n");
            sb.append("        \"use_pre\": ").append(usePre).append("\n");
            sb.append("      },\n");
            sb.append("      \"type\": \"").append(t).append("\"\n");
            sb.append("    }");
        }
        sb.append("\n  ],\n");
        sb.append("  \"file\": \"").append(fileName).append("\",\n");
        sb.append("  \"height\": ").append(f.height).append(",\n");
        sb.append("  \"width\": ").append(f.width).append("\n");
        sb.append("}\n");
        return sb.toString();
    }

    private static String hex(byte[] data) {
        StringBuilder sb = new StringBuilder(data.length * 2);
        for (byte b : data) {
            sb.append(String.format(Locale.ROOT, "%02x", b & 0xFF));
        }
        return sb.toString();
    }
}