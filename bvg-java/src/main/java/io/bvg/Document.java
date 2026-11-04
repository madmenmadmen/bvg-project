package io.bvg;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class Document {
    public int width;
    public int height;

    final List<int[]> palette = new ArrayList<>();
    final Map<Long, Integer> colorIdx = new HashMap<>();

    final List<Gradient> gradients = new ArrayList<>();
    final Map<String, Integer> gradIdx = new HashMap<>();

    final List<Style> styles = new ArrayList<>();
    final Map<Long, Integer> styleIdx = new HashMap<>();

    final List<Node> nodes = new ArrayList<>();

    public Document(int w, int h) {
        this.width = w;
        this.height = h;
    }

    int color(int r, int g, int b, int a) {
        long key = ((long) r << 24) | ((long) g << 16) | ((long) b << 8) | a;
        Integer i = colorIdx.get(key);
        if (i != null) return i;
        int idx = palette.size();
        palette.add(new int[]{r, g, b, a});
        colorIdx.put(key, idx);
        return idx;
    }

    int color(String hex) {
        String s = hex.startsWith("#") ? hex.substring(1) : hex;
        if (s.length() == 3) s = "" + s.charAt(0) + s.charAt(0)
                + s.charAt(1) + s.charAt(1) + s.charAt(2) + s.charAt(2);
        int r = Integer.parseInt(s.substring(0, 2), 16);
        int g = Integer.parseInt(s.substring(2, 4), 16);
        int b = Integer.parseInt(s.substring(4, 6), 16);
        return color(r, g, b, 255);
    }

    int gradient(Gradient g) {
        StringBuilder key = new StringBuilder();
        key.append(g.kind).append('|').append(g.stops.size()).append('|');
        for (Gradient.Stop s : g.stops) {
            key.append(s.offset).append(',');
            key.append(s.colorIdx).append(',');
            if (s.rgba != null) {
                for (int v : s.rgba) key.append(v).append('.');
            }
            key.append(';');
        }
        key.append('|');
        if (g.geometry != null) {
            for (double v : g.geometry) key.append(v).append(',');
        }
        key.append('|');
        if (g.transform != null) {
            for (double v : g.transform) key.append(v).append(',');
        }
        String k = key.toString();
        Integer i = gradIdx.get(k);
        if (i != null) return i;
        int idx = gradients.size();
        gradients.add(g);
        gradIdx.put(k, idx);
        return idx;
    }

    int style(int fillKind, int fillIdx,
              int strokeKind, int strokeIdx,
              float sw, int cap, int join,
              double miter, List<Double> dash,
              int opacity, int blend) {
        long key = ((long) fillKind << 52)
                ^ ((long) fillIdx << 36)
                ^ ((long) strokeKind << 32)
                ^ ((long) strokeIdx << 16)
                ^ (int) sw
                ^ (cap << 8) ^ (join << 6)
                ^ (opacity << 2) ^ blend;
        Integer i = styleIdx.get(key);
        if (i != null) return i;
        Style s = new Style();
        s.fillKind = fillKind; s.fillIdx = fillIdx;
        s.strokeKind = strokeKind; s.strokeIdx = strokeIdx;
        s.strokeWidth = sw;
        s.cap = cap; s.join = join;
        s.miterLimit = (float) miter;
        s.dash = new ArrayList<>(dash);
        s.opacity = opacity; s.blendMode = blend;
        int idx = styles.size();
        styles.add(s);
        styleIdx.put(key, idx);
        return idx;
    }

    public Document rect(double x, double y, double w, double h, String fill) {
        int ci = color(fill);
        int sid = style(1, ci, 0, 0, 0, 0, 0, 4.0,
                List.of(), 255, 0);
        Node nd = new Node();
        nd.type = Node.RECT;
        nd.rect = new double[]{x, y, w, h};
        nd.style = sid;
        nd.depth = 0;
        nodes.add(nd);
        return this;
    }

    public Document rect(double x, double y, double w, double h, Gradient g) {
        int gi = gradient(g);
        int sid = style(2, gi, 0, 0, 0, 0, 0, 4.0,
                List.of(), 255, 0);
        Node nd = new Node();
        nd.type = Node.RECT;
        nd.rect = new double[]{x, y, w, h};
        nd.style = sid;
        nd.depth = 0;
        nodes.add(nd);
        return this;
    }

    public Document rect(double x, double y, double w, double h,
                         Gradient g, int opacity) {
        int gi = gradient(g);
        int sid = style(2, gi, 0, 0, 0, 0, 0, 4.0,
                List.of(), opacity, 0);
        Node nd = new Node();
        nd.type = Node.RECT;
        nd.rect = new double[]{x, y, w, h};
        nd.style = sid;
        nd.depth = 0;
        nodes.add(nd);
        return this;
    }

    public BvgFile toBvgFile() {
        BvgFile f = new BvgFile(width, height);
        if (!palette.isEmpty())
            f.setChunk("COLR", Generated.packColr(palette),
                    Codec.NONE, 0, false);
        if (!gradients.isEmpty())
            f.setChunk("GRAD", Generated.packGrad(gradients),
                    Codec.NONE, 0, false);
        if (!styles.isEmpty())
            f.setChunk("STYL", Generated.packStyl(styles),
                    Codec.NONE, 0, false);
        // SPEC 3.2: NODE is always present (possibly empty)
        f.setChunk("NODE", Generated.packNode(nodes),
                Codec.NONE, 0, false);
        return f;
    }

    public void save(Path path, int codec, int level)
            throws Exception {
        BvgFile f = toBvgFile();
        f.write(path, true, true, codec, level);
    }
}