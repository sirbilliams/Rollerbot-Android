package android.graphics;
/** Desktop shim (tools/launcher): like tools/companion's, plus alpha on every shape and text (Android semantics). */
public class Paint {
    public static final int ANTI_ALIAS_FLAG = 1, FILTER_BITMAP_FLAG = 2;
    public enum Style { FILL, STROKE }
    public enum Align { LEFT, CENTER, RIGHT }
    public enum Cap { BUTT, ROUND, SQUARE }
    public enum Join { MITER, ROUND, BEVEL }
    int color = 0xff000000; Style style = Style.FILL; float strokeWidth = 1; Cap cap = Cap.BUTT;
    float textSize = 12; Typeface typeface = Typeface.SANS_SERIF; Align align = Align.LEFT; Shader shader;
    int alpha = 255;
    public Paint() {}
    public Paint(int flags) {}
    public void setColor(int c) { color = c; alpha = c >>> 24; }
    public int getColor() { return color; }
    public void setAlpha(int a) { alpha = Math.max(0, Math.min(255, a)); color = (color & 0xffffff) | (alpha << 24); }
    public int getAlpha() { return alpha; }
    public void setStyle(Style s) { style = s; }
    public void setStrokeWidth(float w) { strokeWidth = w; }
    public void setStrokeCap(Cap c) { cap = c; }
    public void setStrokeJoin(Join j) {}
    public void setTextSize(float s) { textSize = s; }
    public float getTextSize() { return textSize; }
    public void setTypeface(Typeface t) { typeface = t; }
    public void setTextAlign(Align a) { align = a; }
    public Shader setShader(Shader s) { shader = s; return s; }
    java.awt.Font font() { return new java.awt.Font(typeface.family, typeface.style == 1 ? java.awt.Font.BOLD : java.awt.Font.PLAIN, 1).deriveFont(textSize); }
    private static java.awt.Graphics2D mg;
    public float measureText(String t) {
        if (mg == null) {
            mg = new java.awt.image.BufferedImage(1, 1, java.awt.image.BufferedImage.TYPE_INT_ARGB).createGraphics();
            mg.setRenderingHint(java.awt.RenderingHints.KEY_TEXT_ANTIALIASING, java.awt.RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        }
        mg.setFont(font());
        return (float) mg.getFontMetrics().getStringBounds(t, mg).getWidth();
    }
}
