package android.graphics;
/** Desktop shim (tools/launcher): two-colour and multi-stop linear gradients. */
public class LinearGradient extends Shader {
    public LinearGradient(float x0, float y0, float x1, float y1, int c0, int c1, TileMode m) {
        awt = new java.awt.GradientPaint(x0, y0, new java.awt.Color(c0, true), x1, y1, new java.awt.Color(c1, true));
    }
    public LinearGradient(float x0, float y0, float x1, float y1, int[] colors, float[] pos, TileMode m) {
        float[] f = pos;
        if (f == null) { f = new float[colors.length]; for (int i = 0; i < f.length; i++) f[i] = (float) i / (f.length - 1); }
        java.awt.Color[] c = new java.awt.Color[colors.length];
        for (int i = 0; i < c.length; i++) c[i] = new java.awt.Color(colors[i], true);
        awt = new java.awt.LinearGradientPaint(x0, y0, x1, y1, f, c);
    }
}
