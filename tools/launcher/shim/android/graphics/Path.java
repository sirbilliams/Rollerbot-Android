package android.graphics;
/** Desktop shim (tools/launcher): tools/companion's Path plus curves and circles. */
public class Path {
    final java.awt.geom.Path2D.Float p = new java.awt.geom.Path2D.Float();
    public enum Direction { CW, CCW }
    public void reset() { p.reset(); }
    public void moveTo(float x, float y) { p.moveTo(x, y); }
    public void lineTo(float x, float y) { p.lineTo(x, y); }
    public void quadTo(float x1, float y1, float x2, float y2) { p.quadTo(x1, y1, x2, y2); }
    public void cubicTo(float x1, float y1, float x2, float y2, float x3, float y3) { p.curveTo(x1, y1, x2, y2, x3, y3); }
    public void addCircle(float x, float y, float r, Direction d) { p.append(new java.awt.geom.Ellipse2D.Float(x - r, y - r, 2 * r, 2 * r), false); }
    public void close() { p.closePath(); }
}
