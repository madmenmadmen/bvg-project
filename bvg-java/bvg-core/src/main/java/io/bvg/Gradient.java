// GENERATED FILE - DO NOT EDIT
// Source: bvg-spec/schema.yaml
package io.bvg;

import java.util.ArrayList;
import java.util.List;

public final class Gradient {
    public static final int LINEAR = 0;
    public static final int RADIAL = 1;
    public static final int CONIC = 2;
    public static final int PATTERN = 3;

    public static final class Stop {
        public float offset;
        public int colorIdx;      // 0xFFFF = inline
        public int[] rgba;        // null unless colorIdx == 0xFFFF

        public Stop() {}
        public Stop(float off, int ci) { this.offset = off; this.colorIdx = ci; }
        public Stop(float off, int[] rgba) {
            this.offset = off; this.colorIdx = 0xFFFF; this.rgba = rgba;
        }
    }

    public int kind;
    public int flags;
    public List<Stop> stops = new ArrayList<>();
    public double[] geometry;     // length depends on kind
    public double[] transform;    // null if not set

    public Gradient() {}
    public Gradient(int kind, List<Stop> stops, double[] geo, double[] tr) {
        this.kind = kind;
        this.stops = stops;
        this.geometry = geo;
        this.transform = tr;
    }
}