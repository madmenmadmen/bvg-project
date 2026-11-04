package io.bvg;

import java.util.Arrays;

public final class Main {
    private Main() {}

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            usage();
            System.exit(2);
        }
        String cmd = args[0];
        String[] rest = Arrays.copyOfRange(args, 1, args.length);
        switch (cmd) {
            case "info":
                Info.main(rest);
                break;
            case "list":
                ListCmd.main(rest);
                break;
            case "dump":
                Dump.main(rest);
                break;
            case "codecs":
                CodecsCmd.main(rest);
                break;
            case "recompress":
                RecompressCmd.main(rest);
                break;
            case "--version":
            case "-V":
                System.out.println("bvg " + BvgFile.VERSION);
                break;
            case "--help":
            case "-h":
                usage();
                break;
            default:
                System.err.println("unknown command: " + cmd);
                usage();
                System.exit(2);
        }
    }

    private static void usage() {
        System.err.println("usage: bvg <command> [args]");
        System.err.println("commands:");
        System.err.println("  info <file>              print header info");
        System.err.println("  list <file>              list chunks");
        System.err.println("  dump <file> [-o out]     dump contents as JSON");
        System.err.println("  codecs                   list codecs");
        System.err.println("  recompress <in> <out>   re-encode BVG file");
        System.err.println("  --version                print version");
    }
}