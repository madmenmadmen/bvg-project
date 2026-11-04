package io.bvg;

import java.io.ByteArrayOutputStream;

public final class RleCodec {
    private RleCodec() {}

    public static byte[] encode(byte[] data) {
        ByteArrayOutputStream out = new ByteArrayOutputStream(data.length);
        int i = 0, n = data.length;
        while (i < n) {
            byte b = data[i];
            int j = i + 1;
            while (j < n && data[j] == b && (j - i) < 130) j++;
            int run = j - i;
            if (run >= 3) {
                out.write(0x80 | (run - 3));
                out.write(b);
                i = j;
            } else {
                int start = i;
                i++;
                while (i < n && (i - start) < 128) {
                    if (i + 2 < n && data[i] == data[i+1]
                            && data[i] == data[i+2]) break;
                    i++;
                }
                int lit = i - start;
                out.write(lit - 1);
                out.write(data, start, lit);
            }
        }
        return out.toByteArray();
    }

    public static byte[] decode(byte[] data) throws BvgCodecException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(data.length * 2);
        int i = 0, n = data.length;
        while (i < n) {
            int h = data[i++] & 0xFF;
            if ((h & 0x80) != 0) {
                int run = (h & 0x7F) + 3;
                if (i >= n) throw new BvgCodecException("RLE: truncated run");
                byte b = data[i++];
                for (int k = 0; k < run; k++) out.write(b);
            } else {
                int lit = h + 1;
                if (i + lit > n)
                    throw new BvgCodecException("RLE: truncated literal");
                out.write(data, i, lit);
                i += lit;
            }
        }
        return out.toByteArray();
    }
}