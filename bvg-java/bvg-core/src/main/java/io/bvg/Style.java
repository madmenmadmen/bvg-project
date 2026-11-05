// GENERATED FILE - DO NOT EDIT
// Source: bvg-spec/schema.yaml
package io.bvg;

import java.util.ArrayList;
import java.util.List;

public final class Style {
    public int fillKind;
    public int fillIdx;
    public int strokeKind;
    public int strokeIdx;
    public float strokeWidth;
    public int cap;
    public int join;
    public float miterLimit = 4.0f;
    public List<Double> dash = new ArrayList<>();
    public int opacity = 255;
    public int blendMode = 0;

    public Style() {}
}