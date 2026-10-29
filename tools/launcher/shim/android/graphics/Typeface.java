package android.graphics;
/** Desktop shim (tools/launcher): a java.awt font family and style; Android's heavy families map to bold. */
public final class Typeface {
    public static final int NORMAL = 0, BOLD = 1;
    public static final Typeface SANS_SERIF = new Typeface("SansSerif", 0), MONOSPACE = new Typeface("Monospaced", 0);
    final String family; final int style;
    private Typeface(String f, int s) { family = f; style = s; }
    public static Typeface create(String family, int style) {
        boolean heavy = family != null && (family.contains("black") || family.contains("medium"));
        return new Typeface("SansSerif", heavy ? BOLD : style);
    }
    public static Typeface create(Typeface t, int style) { return new Typeface(t.family, style); }
}
