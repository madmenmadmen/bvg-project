package io.bvg;

public final class CodecsCmd {
    private CodecsCmd() {}

    public static void main(String[] args) {
        System.out.printf("%3s %-6s %-10s %8s %10s%n",
                "id", "name", "available", "default", "range");
        System.out.println("-".repeat(42));
        for (int cid = 0; cid < Codec.NAMES.length; cid++) {
            String avail = "yes";
            int lv = Codec.DEFAULT_LEVEL[cid];
            int[] r = Codec.LEVEL_RANGE[cid];
            String rng = r[1] > 0 ? r[0] + ".." + r[1] : "-";
            System.out.printf("%3d %-6s %-10s %8d %10s%n",
                    cid, Codec.NAMES[cid], avail, lv, rng);
        }
    }
}