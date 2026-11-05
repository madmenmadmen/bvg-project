// GENERATED FILE - DO NOT EDIT
// Source: bvg-spec/schema.yaml
package io.bvg;

public final class Node {
    public static final int GROUP = 0;
    public static final int RECT = 1;
    public static final int ELLIPSE = 2;
    public static final int LINE = 3;
    public static final int POLYLINE = 4;
    public static final int POLYGON = 5;
    public static final int PATH = 6;
    public static final int TEXT = 7;
    public static final int IMAGE = 8;
    public static final int USE = 9;
    public static final int DEF = 10;

    public static final int HAS_TRANSFORM = 1;

    public int type;
    public int flags;
    public int style = 0xFFFF;
    public int depth;
    public double[] transform;

    // payload
    public int childCount;       // GROUP, DEF
    public double[] rect;        // RECT: x,y,w,h ; IMAGE: x,y,w,h
    public double[] ellipse;     // ELLIPSE: cx,cy,rx,ry
    public double[] line;        // LINE: x1,y1,x2,y2
    public double[][] points;    // POLYLINE, POLYGON
    public PathCmd[] path;       // PATH
    public int font;             // TEXT
    public double tx, ty;        // TEXT
    public String text;          // TEXT
    public int imageIdx;         // IMAGE
    public int defIdx;           // USE

    public static final class PathCmd {
        public int op;
        public double[] args;
        public PathCmd() {}
        public PathCmd(int op, double... args) {
            this.op = op; this.args = args;
        }
    }
}