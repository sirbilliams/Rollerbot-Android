package android.graphics;
/** Desktop shim (tools/companion). */
public class RadialGradient extends Shader {
    public RadialGradient(float cx, float cy, float r, int[] colors, float[] pos, TileMode m) {
        float[] f = pos;
        if (f == null) { f = new float[colors.length]; for (int i = 0; i < f.length; i++) f[i] = (float) i / (f.length - 1); }
        java.awt.Color[] c = new java.awt.Color[colors.length];
        for (int i = 0; i < c.length; i++) c[i] = new java.awt.Color(colors[i], true);
        awt = new java.awt.RadialGradientPaint(cx, cy, r, f, c);
    }
}
