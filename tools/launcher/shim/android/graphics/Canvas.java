package android.graphics;
import java.awt.AlphaComposite;
import java.awt.BasicStroke;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.geom.*;
import java.util.ArrayDeque;
/** Desktop shim (tools/launcher): tools/companion's Canvas with paint alpha applied like Android. */
public class Canvas {
    private Graphics2D g;
    private final ArrayDeque<Graphics2D> stack = new ArrayDeque<>();
    public Canvas(Bitmap b) {
        g = b.img.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL, RenderingHints.VALUE_STROKE_PURE);
        g.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
    }
    public int save() { stack.push(g); g = (Graphics2D) g.create(); return stack.size(); }
    public void restore() { g.dispose(); g = stack.pop(); }
    public void translate(float x, float y) { g.translate(x, y); }
    public void scale(float x, float y) { g.scale(x, y); }
    public void rotate(float d) { g.rotate(Math.toRadians(d)); }
    public void rotate(float d, float x, float y) { g.rotate(Math.toRadians(d), x, y); }
    public boolean clipRect(RectF r) { g.clip(new Rectangle2D.Float(r.left, r.top, r.width(), r.height())); return true; }
    public boolean clipRect(float l, float t, float r, float b) { return clipRect(new RectF(l,t,r,b)); }
    private void use(Paint p) {
        if (p.shader != null) {
            g.setPaint(p.shader.awt);
            g.setComposite(AlphaComposite.getInstance(AlphaComposite.SRC_OVER, p.alpha / 255f));
        } else {
            g.setPaint(new java.awt.Color(p.color, true));
            g.setComposite(AlphaComposite.SrcOver);
        }
        int cap = p.cap == Paint.Cap.ROUND ? BasicStroke.CAP_ROUND : p.cap == Paint.Cap.SQUARE ? BasicStroke.CAP_SQUARE : BasicStroke.CAP_BUTT;
        g.setStroke(new BasicStroke(p.strokeWidth, cap, BasicStroke.JOIN_ROUND));
    }
    private void shape(java.awt.Shape s, Paint p) { use(p); if (p.style == Paint.Style.STROKE) g.draw(s); else g.fill(s); }
    public void drawColor(int c) { g.setComposite(AlphaComposite.SrcOver); g.setPaint(new java.awt.Color(c, true)); g.fill(new Rectangle2D.Float(-1e5f, -1e5f, 2e5f, 2e5f)); }
    public void drawRect(float l, float t, float r, float b, Paint p) { shape(new Rectangle2D.Float(l, t, r - l, b - t), p); }
    public void drawRect(RectF r, Paint p) { drawRect(r.left, r.top, r.right, r.bottom, p); }
    public void drawRoundRect(RectF r, float rx, float ry, Paint p) { shape(new RoundRectangle2D.Float(r.left, r.top, r.width(), r.height(), rx * 2, ry * 2), p); }
    public void drawCircle(float x, float y, float r, Paint p) { shape(new Ellipse2D.Float(x - r, y - r, 2 * r, 2 * r), p); }
    public void drawOval(RectF r, Paint p) { shape(new Ellipse2D.Float(r.left, r.top, r.width(), r.height()), p); }
    public void drawArc(RectF r, float start, float sweep, boolean centre, Paint p) {
        shape(new Arc2D.Float(r.left, r.top, r.width(), r.height(), -start, -sweep, centre ? Arc2D.PIE : Arc2D.OPEN), p);
    }
    public void drawLine(float x0, float y0, float x1, float y1, Paint p) { use(p); g.draw(new Line2D.Float(x0, y0, x1, y1)); }
    public void drawPath(Path path, Paint p) { shape(path.p, p); }
    public void drawText(String s, float x, float y, Paint p) {
        use(p);
        g.setFont(p.font());
        float w = (float) g.getFontMetrics().getStringBounds(s, g).getWidth();
        if (p.align == Paint.Align.CENTER) x -= w / 2; else if (p.align == Paint.Align.RIGHT) x -= w;
        g.drawString(s, x, y);
    }
    public void drawBitmap(Bitmap b, float x, float y, Paint p) { g.drawImage(b.img, Math.round(x), Math.round(y), null); }
    public void drawBitmap(Bitmap b, Rect s, RectF d, Paint p) {
        g.setComposite(AlphaComposite.getInstance(AlphaComposite.SRC_OVER, p != null ? p.alpha / 255f : 1f));
        g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
        int sl = s != null ? s.left : 0, st = s != null ? s.top : 0, sr = s != null ? s.right : b.getWidth(), sb = s != null ? s.bottom : b.getHeight();
        java.awt.geom.AffineTransform t = g.getTransform();
        g.translate(d.left, d.top); g.scale(d.width() / (sr - sl), d.height() / (sb - st));
        g.drawImage(b.img, 0, 0, sr - sl, sb - st, sl, st, sr, sb, null);
        g.setTransform(t);
    }
}
