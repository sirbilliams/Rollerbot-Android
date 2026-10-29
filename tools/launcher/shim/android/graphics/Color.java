package android.graphics;
/** Desktop shim (tools/companion). */
public final class Color {
    public static final int WHITE = 0xffffffff, BLACK = 0xff000000;
    public static int rgb(int r, int g, int b) { return argb(255, r, g, b); }
    public static int argb(int a, int r, int g, int b) { return (a << 24) | (r << 16) | (g << 8) | b; }
    public static int alpha(int c) { return c >>> 24; }
    public static int red(int c) { return (c >> 16) & 0xff; }
    public static int green(int c) { return (c >> 8) & 0xff; }
    public static int blue(int c) { return c & 0xff; }
}
