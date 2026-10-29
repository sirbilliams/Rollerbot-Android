package io.github.sirbilliams.rollerbot;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;

/**
 * Drawing kit for the launcher and the bottom-screen companion: a bright toy-like world drawn in code (no bitmaps, no fonts,
 * no game art). Four eras of the time journey (desert and pyramids, castle, city, space) scroll past a rolling ball; buttons
 * and cards are chunky blocks with a lit top and a dark underside. All shapes are original.
 */
final class Toy {
    static final int DESERT = 0, CASTLE = 1, CITY = 2, SPACE = 3;
    static final String[] ERA_NAMES = { "Desert", "Castle", "City", "Space" };
    static final int[] TOP = { 0xff2f9de6, 0xff4a86e0, 0xff1f2f7a, 0xff05072a };
    static final int[] BOTTOM = { 0xffffe6a8, 0xffc4e8ff, 0xffff9a78, 0xff3b2a86 };
    static final int[] GROUND = { 0xffe8b25a, 0xff63c85a, 0xff3a4466, 0xff7e72c8 };
    static final int[] GROUND_DARK = { 0xffc98c38, 0xff3e9e3c, 0xff262e4a, 0xff5a4fa0 };
    /** Toy palette: red, orange, yellow, green, teal, blue, violet, pink. */
    static final String[] LANG_LABELS = { "EN", "FR", "DE", "ES", "IT" };
    static final int[] TOYS = { 0xffff4b3e, 0xffff9a1f, 0xffffd81f, 0xff3cc84a, 0xff1fc4b4, 0xff2f8df0, 0xff8b5cf0, 0xfff05cae };

    final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Paint sp = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Paint tp = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Path path = new Path();
    final RectF rc = new RectF();
    private final Shader[] skyCache = new Shader[4];
    private float skyH;
    private final Typeface bold = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD);

    Toy() {
        sp.setStyle(Paint.Style.STROKE);
        tp.setTypeface(bold);
    }

    /** Sets a shader on p; paint alpha applies to shaders, so it is reset first. */
    void shader(Shader s) { p.setColor(Color.WHITE); p.setShader(s); }

    static int mix(int a, int b, float t) {
        t = Math.max(0, Math.min(1, t));
        return Color.argb((int) (Color.alpha(a) + (Color.alpha(b) - Color.alpha(a)) * t),
                (int) (Color.red(a) + (Color.red(b) - Color.red(a)) * t),
                (int) (Color.green(a) + (Color.green(b) - Color.green(a)) * t),
                (int) (Color.blue(a) + (Color.blue(b) - Color.blue(a)) * t));
    }
    static int alpha(int c, float a) { return Color.argb((int) (Color.alpha(c) * Math.max(0, Math.min(1, a))), Color.red(c), Color.green(c), Color.blue(c)); }
    static int shade(int c, float f) { return mix(c, f < 0 ? Color.BLACK : Color.WHITE, Math.abs(f)); }
    static float ease(float t) { t = Math.max(0, Math.min(1, t)); return t < 0.5f ? 4 * t * t * t : 1 - (float) Math.pow(-2 * t + 2, 3) / 2; }
    private static float hash(int i) { int x = i * 374761393 + 668265263; x = (x ^ (x >>> 13)) * 1274126177; return ((x ^ (x >>> 16)) & 0xffff) / 65535f; }

    // ---- text -------------------------------------------------------------------------------------------------
    /** Bold text with a dark drop shadow; align -1 left, 0 centre, 1 right. */
    void text(Canvas c, String s, float x, float y, float size, int color, int align) {
        tp.setTextSize(size);
        tp.setTextAlign(align < 0 ? Paint.Align.LEFT : align == 0 ? Paint.Align.CENTER : Paint.Align.RIGHT);
        tp.setColor(alpha(0xff0b1030, Color.alpha(color) / 255f * 0.45f));
        c.drawText(s, x + size * 0.04f, y + size * 0.06f, tp);
        tp.setColor(color);
        c.drawText(s, x, y, tp);
    }
    float measure(String s, float size) { tp.setTextSize(size); return tp.measureText(s); }

    /** The wordmark: chunky extruded letters in toy colours, each bobbing on its own phase. */
    void wordmark(Canvas c, String s, float x, float y, float size, float t) {
        tp.setTextSize(size);
        tp.setTextAlign(Paint.Align.LEFT);
        float cx = x;
        for (int i = 0; i < s.length(); i++) {
            String ch = s.substring(i, i + 1);
            float w = tp.measureText(ch);
            float by = y + (float) Math.sin(t * 2.2f + i * 0.7f) * size * 0.035f;
            int col = TOYS[i % TOYS.length];
            int depth = (int) (size * 0.085f);
            tp.setColor(0xff101840);
            for (int d = depth; d >= 1; d--) c.drawText(ch, cx + d * 0.55f, by + d, tp);
            tp.setColor(shade(col, -0.45f));
            c.drawText(ch, cx + 2, by + 3, tp);
            tp.setColor(col);
            c.drawText(ch, cx, by, tp);
            tp.setColor(alpha(Color.WHITE, 0.35f));
            c.save(); c.clipRect(cx - 4, by - size * 0.8f, cx + w + 4, by - size * 0.42f); c.drawText(ch, cx - 1, by - 2, tp); c.restore();
            cx += w + size * 0.01f;
        }
    }

    // ---- block shapes -----------------------------------------------------------------------------------------
    /** A chunky toy block: dark underside, lit face with a gloss band. depth = underside height. */
    void block(Canvas c, RectF r, float radius, float depth, int face, boolean glow) {
        if (glow) {
            for (int i = 3; i >= 1; i--) {
                sp.setStrokeWidth(8 * i);
                sp.setColor(alpha(0xffffffff, 0.10f));
                rc.set(r.left - 6 * i, r.top - 6 * i, r.right + 6 * i, r.bottom + 6 * i);
                c.drawRoundRect(rc, radius + 6 * i, radius + 6 * i, sp);
            }
        }
        p.setShader(null);
        p.setColor(alpha(0xff000000, 0.25f));
        rc.set(r.left + 4, r.top + depth + 8, r.right + 4, r.bottom + depth + 12);
        c.drawRoundRect(rc, radius, radius, p);
        p.setColor(shade(face, -0.5f));
        rc.set(r.left, r.top + depth, r.right, r.bottom + depth);
        c.drawRoundRect(rc, radius, radius, p);
        shader(new LinearGradient(0, r.top, 0, r.bottom, shade(face, 0.28f), shade(face, -0.12f), Shader.TileMode.CLAMP));
        c.drawRoundRect(r, radius, radius, p);
        p.setShader(null);
        p.setColor(alpha(Color.WHITE, 0.30f));
        rc.set(r.left + radius * 0.5f, r.top + 6, r.right - radius * 0.5f, r.top + (r.bottom - r.top) * 0.36f);
        c.drawRoundRect(rc, radius * 0.6f, radius * 0.6f, p);
        sp.setStrokeWidth(3);
        sp.setColor(shade(face, -0.35f));
        c.drawRoundRect(r, radius, radius, sp);
    }

    /** Cube seen from above-left: top, left and right faces. */
    void cube(Canvas c, float cx, float cy, float s, int col, float spin) {
        c.save(); c.translate(cx, cy); c.rotate((float) Math.sin(spin) * 12);
        path.reset(); path.moveTo(-s, -s * 0.2f); path.lineTo(0, s * 0.3f); path.lineTo(0, s * 1.3f); path.lineTo(-s, s * 0.8f); path.close();
        p.setShader(null); p.setColor(shade(col, -0.15f)); c.drawPath(path, p);
        path.reset(); path.moveTo(s, -s * 0.2f); path.lineTo(0, s * 0.3f); path.lineTo(0, s * 1.3f); path.lineTo(s, s * 0.8f); path.close();
        p.setColor(shade(col, -0.42f)); c.drawPath(path, p);
        path.reset(); path.moveTo(0, -s * 0.7f); path.lineTo(s, -s * 0.2f); path.lineTo(0, s * 0.3f); path.lineTo(-s, -s * 0.2f); path.close();
        p.setColor(shade(col, 0.3f)); c.drawPath(path, p);
        sp.setStrokeWidth(2.5f); sp.setColor(alpha(0xff000000, 0.25f)); c.drawPath(path, sp);
        c.restore();
    }

    /** A glossy sphere with a shadow on the ground at groundY (<0: none). */
    void sphere(Canvas c, float cx, float cy, float r, int col, float groundY) {
        if (groundY > 0) {
            float k = Math.max(0.3f, 1 - (groundY - cy - r) / 400f);
            p.setShader(null); p.setColor(alpha(0xff000000, 0.28f * k));
            rc.set(cx - r * k * 1.1f, groundY - r * 0.16f, cx + r * k * 1.1f, groundY + r * 0.16f);
            c.drawOval(rc, p);
        }
        shader(new RadialGradient(cx - r * 0.35f, cy - r * 0.4f, r * 1.5f, new int[] { shade(col, 0.55f), col, shade(col, -0.45f) },
                new float[] { 0f, 0.45f, 1f }, Shader.TileMode.CLAMP));
        c.drawCircle(cx, cy, r, p);
        p.setShader(null);
        p.setColor(alpha(Color.WHITE, 0.75f));
        rc.set(cx - r * 0.62f, cy - r * 0.72f, cx - r * 0.12f, cy - r * 0.42f);
        c.drawOval(rc, p);
    }

    /** The rolling ball: red body, white rivets and a stripe band that turn with spin (radians), shadow on the ground. */
    void ball(Canvas c, float cx, float cy, float r, float spin, float groundY) {
        p.setShader(null); p.setColor(alpha(0xff000000, 0.30f));
        rc.set(cx - r * 1.05f, groundY - r * 0.16f, cx + r * 1.05f, groundY + r * 0.18f);
        c.drawOval(rc, p);
        shader(new RadialGradient(cx - r * 0.3f, cy - r * 0.35f, r * 1.6f, new int[] { 0xffff9a7a, 0xffff3b32, 0xff8f0f26 }, new float[] { 0f, 0.5f, 1f }, Shader.TileMode.CLAMP));
        c.drawCircle(cx, cy, r, p);
        // rivets and a stripe: points on the sphere rotating about the vertical axis; width squashes toward the rim
        p.setShader(null);
        for (int i = 0; i < 6; i++) {
            float a = spin + i * (float) Math.PI / 3, depth = (float) Math.cos(a);
            if (depth <= 0.05f) continue;
            float px = cx + (float) Math.sin(a) * r * 0.78f, py = cy + (i % 2 == 0 ? -0.18f : 0.28f) * r;
            rc.set(px - r * 0.16f * depth, py - r * 0.16f, px + r * 0.16f * depth, py + r * 0.16f);
            p.setColor(alpha(0xfffff3d6, 0.9f)); c.drawOval(rc, p);
        }
        p.setColor(alpha(Color.WHITE, 0.8f));
        rc.set(cx - r * 0.62f, cy - r * 0.78f, cx - r * 0.1f, cy - r * 0.46f);
        c.drawOval(rc, p);
        sp.setStrokeWidth(r * 0.05f); sp.setColor(alpha(0xff3a0614, 0.5f));
        c.drawCircle(cx, cy, r, sp);
    }

    // ---- world ------------------------------------------------------------------------------------------------
    private void sky(Canvas c, int era, float w, float h) {
        if (skyH != h) { skyH = h; for (int i = 0; i < 4; i++) skyCache[i] = null; }
        if (skyCache[era] == null) skyCache[era] = new LinearGradient(0, 0, 0, h, new int[] { TOP[era], BOTTOM[era] }, new float[] { 0f, 0.86f }, Shader.TileMode.CLAMP);
        shader(skyCache[era]);
        c.drawRect(-10, -10, w + 10, h + 10, p);
        p.setShader(null);
    }

    private void cloud(Canvas c, float x, float y, float s, int col) {
        p.setColor(col);
        for (int i = 0; i < 5; i++) {
            float ox = (i - 2) * s * 0.55f, rr = s * (0.55f - Math.abs(i - 2) * 0.1f);
            rc.set(x + ox - rr, y - rr * 0.8f - (i == 2 ? s * 0.2f : 0), x + ox + rr, y + rr * 0.7f);
            c.drawOval(rc, p);
        }
        rc.set(x - s * 1.5f, y, x + s * 1.5f, y + s * 0.7f);
        c.drawRoundRect(rc, s * 0.35f, s * 0.35f, p);
    }

    /** Draw one era's whole backdrop for a world w x h; t seconds; ground top at gy. */
    void world(Canvas c, int era, float w, float h, float gy, float t) {
        sky(c, era, w, h);
        p.setShader(null);
        if (era == SPACE) {
            for (int i = 0; i < 110; i++) {
                float x = ((hash(i) * w * 1.1f - t * (2 + hash(i + 300) * 6)) % (w * 1.1f) + w * 1.1f) % (w * 1.1f);
                float y = hash(i + 99) * gy * 0.95f, tw = 0.55f + 0.45f * (float) Math.sin(t * (1 + hash(i + 7) * 2) + i);
                p.setColor(alpha(0xffffffff, tw * (0.4f + hash(i + 40) * 0.6f)));
                float s = 1.5f + hash(i + 11) * 2.6f;
                c.drawCircle(x, y, s, p);
            }
        } else {
            int cc = era == CITY ? 0xff8c6cb8 : 0xffffffff;
            float a = era == CITY ? 0.55f : 0.9f;
            for (int i = 0; i < 5; i++) cloud(c, ((i * 520 - t * (9 + i * 3)) % (w + 600) + w + 600) % (w + 600) - 300, 110 + (i * 97) % 230, 62 + (i * 13) % 40, alpha(cc, a));
        }
        // sun / planet
        if (era == DESERT) { sunGlow(c, w * 0.80f, 190, 95, 0xffffe46a); }
        else if (era == CASTLE) { sunGlow(c, w * 0.12f, 170, 70, 0xfffff2a0); }
        else if (era == CITY) { sunGlow(c, w * 0.52f, gy - 60, 120, 0xffffc27a); }
        else {
            float px = w * 0.82f, py = 230;
            shader(new RadialGradient(px - 40, py - 50, 180, new int[] { 0xffffb86b, 0xffe8602c, 0xff6a1f5a }, new float[] { 0f, 0.5f, 1f }, Shader.TileMode.CLAMP));
            c.drawCircle(px, py, 120, p); p.setShader(null);
            sp.setStrokeWidth(14); sp.setColor(alpha(0xffffe8b0, 0.85f));
            c.save(); c.rotate(-18, px, py); rc.set(px - 210, py - 38, px + 210, py + 38); c.drawOval(rc, sp); c.restore();
            p.setColor(0xff9ad0ff); c.drawCircle(w * 0.2f, 330 + (float) Math.sin(t * 0.4f) * 8, 30, p);
        }
        // far layer
        far(c, era, w, gy, t * 22);
        // mid layer
        mid(c, era, w, gy, t * 55);
        // ground
        shader(new LinearGradient(0, gy, 0, h, GROUND[era], GROUND_DARK[era], Shader.TileMode.CLAMP));
        c.drawRect(-10, gy, w + 10, h + 10, p); p.setShader(null);
        p.setColor(alpha(Color.WHITE, 0.35f)); c.drawRect(-10, gy, w + 10, gy + 6, p);
        ground(c, era, w, h, gy, t * 170);
    }

    private void sunGlow(Canvas c, float x, float y, float r, int col) {
        shader(new RadialGradient(x, y, r * 2.6f, new int[] { alpha(col, 0.55f), alpha(col, 0f) }, new float[] { 0f, 1f }, Shader.TileMode.CLAMP));
        c.drawCircle(x, y, r * 2.6f, p); p.setShader(null);
        p.setColor(col); c.drawCircle(x, y, r, p);
        p.setColor(alpha(Color.WHITE, 0.5f)); c.drawCircle(x - r * 0.25f, y - r * 0.25f, r * 0.55f, p);
    }

    private static float wrap(float x, float period) { return ((x % period) + period) % period; }

    private void far(Canvas c, int era, float w, float gy, float off) {
        float period = 900;
        for (int k = -1; k * period < w + period; k++) {
            float x0 = k * period - wrap(off, period);
            if (era == DESERT || era == CASTLE) {
                p.setColor(era == DESERT ? 0xfff6cf7c : 0xff9ccaf2);
                path.reset(); path.moveTo(x0, gy);
                path.quadTo(x0 + period * 0.25f, gy - (era == DESERT ? 95 : 170), x0 + period * 0.5f, gy - 40);
                path.quadTo(x0 + period * 0.75f, gy - (era == DESERT ? 60 : 130), x0 + period, gy);
                path.close(); c.drawPath(path, p);
            } else if (era == CITY) {
                for (int i = 0; i < 9; i++) {
                    float bw = 70 + hash(i + k * 9 + 50) * 40, bh = 120 + hash(i + 17) * 200;
                    p.setColor(0xff4a3f86); c.drawRect(x0 + i * 100, gy - bh, x0 + i * 100 + bw, gy, p);
                }
            } else {
                p.setColor(0xff4a3f96);
                path.reset(); path.moveTo(x0, gy);
                for (int i = 0; i <= 8; i++) path.lineTo(x0 + i * period / 8, gy - 30 - hash(i + 5 + (i == 8 ? -8 : 0)) * 90);
                path.lineTo(x0 + period, gy); path.close(); c.drawPath(path, p);
            }
        }
    }

    private void mid(Canvas c, int era, float w, float gy, float off) {
        float period = 1300;
        for (int k = -1; k * period < w + period; k++) {
            float x0 = k * period - wrap(off, period);
            if (era == DESERT) {
                pyramid(c, x0 + 150, gy, 400, 300); pyramid(c, x0 + 540, gy, 230, 170);
                palm(c, x0 + 800, gy, 190, off); palm(c, x0 + 1030, gy, 150, off + 1.3f);
            } else if (era == CASTLE) {
                castle(c, x0 + 80, gy, off);
                tree(c, x0 + 760, gy, 110); tree(c, x0 + 930, gy, 80); tree(c, x0 + 1150, gy, 120);
            } else if (era == CITY) {
                for (int i = 0; i < 6; i++) tower(c, x0 + 20 + i * 215, gy, 150 + hash(i + 3) * 60, 220 + hash(i + 31 + k) * 260, i);
            } else {
                dome(c, x0 + 200, gy, 170); crystal(c, x0 + 620, gy, 220, 0xff5ce0ff); crystal(c, x0 + 700, gy, 140, 0xfff05cae);
                dome(c, x0 + 1020, gy, 110); rocket(c, x0 + 1200, gy);
            }
        }
    }

    private void pyramid(Canvas c, float x, float gy, float bw, float bh) {
        path.reset(); path.moveTo(x, gy); path.lineTo(x + bw * 0.45f, gy - bh); path.lineTo(x + bw * 0.62f, gy); path.close();
        p.setColor(0xffffd983); c.drawPath(path, p);
        path.reset(); path.moveTo(x + bw * 0.45f, gy - bh); path.lineTo(x + bw, gy); path.lineTo(x + bw * 0.62f, gy); path.close();
        p.setColor(0xffd9963c); c.drawPath(path, p);
        sp.setStrokeWidth(2.5f); sp.setColor(alpha(0xff8a5a1a, 0.4f));
        for (int i = 1; i < 6; i++) { float f = i / 6f, yy = gy - bh * f; c.drawLine(x + bw * 0.45f * f, yy, x + bw - bw * 0.55f * f, yy, sp); }
        p.setColor(0xff7a4a14);
        path.reset(); path.moveTo(x + bw * 0.5f, gy); path.lineTo(x + bw * 0.5f, gy - bh * 0.12f); path.lineTo(x + bw * 0.56f, gy - bh * 0.12f); path.lineTo(x + bw * 0.58f, gy); path.close(); c.drawPath(path, p);
    }

    private void palm(Canvas c, float x, float gy, float hgt, float ph) {
        sp.setStrokeWidth(16); sp.setColor(0xff8a5a2a); sp.setStrokeCap(Paint.Cap.ROUND);
        path.reset(); path.moveTo(x, gy); path.quadTo(x + 25, gy - hgt * 0.5f, x + 6, gy - hgt); c.drawPath(path, sp);
        sp.setStrokeCap(Paint.Cap.BUTT);
        for (int i = 0; i < 6; i++) {
            float a = -150 + i * 56 + (float) Math.sin(ph * 0.05f + i) * 4;
            c.save(); c.translate(x + 6, gy - hgt); c.rotate(a);
            p.setColor(i % 2 == 0 ? 0xff3cc84a : 0xff2aa83a);
            rc.set(0, -14, 120, 14); c.drawOval(rc, p);
            c.restore();
        }
        p.setColor(0xff5a3a18); c.drawCircle(x + 2, gy - hgt + 10, 9, p); c.drawCircle(x + 16, gy - hgt + 12, 9, p);
    }

    private void castle(Canvas c, float x, float gy, float off) {
        p.setColor(0xffd7cdbd); c.drawRect(x + 100, gy - 190, x + 470, gy, p);
        p.setColor(0xffb7ab98); c.drawRect(x + 100, gy - 190, x + 130, gy, p);
        for (int i = 0; i < 6; i++) { p.setColor(0xffd7cdbd); c.drawRect(x + 100 + i * 64, gy - 222, x + 130 + i * 64, gy - 190, p); }
        p.setColor(0xff4a2e1a); rc.set(x + 250, gy - 120, x + 320, gy + 20); c.drawRoundRect(rc, 35, 35, p);
        roundTower(c, x + 40, gy, 80, 330, 0xffe8dfd0); roundTower(c, x + 450, gy, 90, 380, 0xffe8dfd0);
        flag(c, x + 80, gy - 330 - 70, off); flag(c, x + 495, gy - 380 - 78, off + 2);
    }
    private void roundTower(Canvas c, float x, float gy, float w, float hgt, int col) {
        p.setColor(col); c.drawRect(x, gy - hgt, x + w, gy, p);
        p.setColor(alpha(0xff000000, 0.12f)); c.drawRect(x + w * 0.62f, gy - hgt, x + w, gy, p);
        p.setColor(0xffe0443a);
        path.reset(); path.moveTo(x - 10, gy - hgt); path.lineTo(x + w / 2, gy - hgt - 80); path.lineTo(x + w + 10, gy - hgt); path.close(); c.drawPath(path, p);
        p.setColor(0xffa82a2a);
        path.reset(); path.moveTo(x + w / 2, gy - hgt - 80); path.lineTo(x + w + 10, gy - hgt); path.lineTo(x + w * 0.6f, gy - hgt); path.close(); c.drawPath(path, p);
        p.setColor(0xff3a4a7a); rc.set(x + w * 0.35f, gy - hgt * 0.6f, x + w * 0.65f, gy - hgt * 0.6f + 46); c.drawRoundRect(rc, 14, 14, p);
    }
    private void flag(Canvas c, float x, float y, float ph) {
        sp.setStrokeWidth(4); sp.setColor(0xff555555); c.drawLine(x, y + 70, x, y, sp);
        float wv = (float) Math.sin(ph * 0.08f) * 6;
        p.setColor(0xffffd81f); path.reset(); path.moveTo(x, y); path.quadTo(x + 25, y + 6 + wv, x + 50, y + 14); path.quadTo(x + 25, y + 22 - wv, x, y + 28); path.close(); c.drawPath(path, p);
    }
    private void tree(Canvas c, float x, float gy, float s) {
        p.setColor(0xff8a5a2a); c.drawRect(x - s * 0.07f, gy - s * 0.45f, x + s * 0.07f, gy, p);
        p.setColor(0xff2fa83a); c.drawCircle(x, gy - s * 0.7f, s * 0.42f, p);
        p.setColor(0xff4ccf52); c.drawCircle(x - s * 0.1f, gy - s * 0.78f, s * 0.3f, p);
    }
    private void tower(Canvas c, float x, float gy, float w, float hgt, int seed) {
        if (hgt <= 0) return;
        int col = TOYS[(seed * 3 + 5) % TOYS.length];
        p.setColor(mix(0xff2a3a78, col, 0.18f)); c.drawRect(x, gy - hgt, x + w * 0.72f, gy, p);
        p.setColor(mix(0xff1a2458, col, 0.12f)); path.reset(); path.moveTo(x + w * 0.72f, gy - hgt); path.lineTo(x + w, gy - hgt + 24); path.lineTo(x + w, gy); path.lineTo(x + w * 0.72f, gy); path.close(); c.drawPath(path, p);
        p.setColor(mix(0xff4a5aa8, col, 0.3f)); path.reset(); path.moveTo(x, gy - hgt); path.lineTo(x + w * 0.72f, gy - hgt); path.lineTo(x + w, gy - hgt + 24); path.lineTo(x + w * 0.28f, gy - hgt + 24); path.close(); c.drawPath(path, p);
        for (int r = 0; r < (int) (hgt / 46); r++) for (int q = 0; q < 3; q++) {
            if (hash(seed * 100 + r * 7 + q) < 0.45f) continue;
            p.setColor(hash(seed * 100 + r * 5 + q + 9) < 0.5f ? 0xffffe27a : 0xffbfe8ff);
            c.drawRect(x + 14 + q * (w * 0.72f - 20) / 3, gy - hgt + 38 + r * 46, x + 14 + q * (w * 0.72f - 20) / 3 + 18, gy - hgt + 58 + r * 46, p);
        }
    }
    private void dome(Canvas c, float x, float gy, float r) {
        p.setColor(0xffdfe6ff); rc.set(x - r, gy - r * 1.1f, x + r, gy + r * 0.9f); c.drawArc(rc, 180, 180, true, p);
        p.setColor(alpha(0xff6a4fe0, 0.5f)); c.drawArc(rc, 200, 100, false, p);
        p.setColor(0xff9ad0ff); rc.set(x - r * 0.9f, gy - r * 1.0f, x + r * 0.9f, gy + r * 0.8f); c.drawArc(rc, 180, 180, true, p);
        p.setColor(alpha(Color.WHITE, 0.5f)); rc.set(x - r * 0.6f, gy - r * 0.9f, x - r * 0.2f, gy - r * 0.6f); c.drawOval(rc, p);
        p.setColor(0xff7a6cc8); c.drawRect(x - r * 1.1f, gy - 8, x + r * 1.1f, gy, p);
    }
    private void crystal(Canvas c, float x, float gy, float hgt, int col) {
        path.reset(); path.moveTo(x - 36, gy); path.lineTo(x - 18, gy - hgt * 0.8f); path.lineTo(x, gy - hgt); path.lineTo(x + 6, gy); path.close();
        p.setColor(shade(col, 0.2f)); c.drawPath(path, p);
        path.reset(); path.moveTo(x, gy - hgt); path.lineTo(x + 40, gy - hgt * 0.6f); path.lineTo(x + 34, gy); path.lineTo(x + 6, gy); path.close();
        p.setColor(shade(col, -0.25f)); c.drawPath(path, p);
    }
    private void rocket(Canvas c, float x, float gy) {
        p.setColor(0xfff2f2ff); rc.set(x - 26, gy - 150, x + 26, gy - 20); c.drawRoundRect(rc, 26, 26, p);
        p.setColor(0xffff4b3e); path.reset(); path.moveTo(x - 26, gy - 110); path.quadTo(x, gy - 200, x + 26, gy - 110); path.close(); c.drawPath(path, p);
        path.reset(); path.moveTo(x - 26, gy - 50); path.lineTo(x - 56, gy); path.lineTo(x - 20, gy - 20); path.close(); c.drawPath(path, p);
        path.reset(); path.moveTo(x + 26, gy - 50); path.lineTo(x + 56, gy); path.lineTo(x + 20, gy - 20); path.close(); c.drawPath(path, p);
        p.setColor(0xff2f8df0); c.drawCircle(x, gy - 90, 12, p);
    }

    private void ground(Canvas c, int era, float w, float h, float gy, float off) {
        float period = 260;
        for (int k = -1; k * period < w + period; k++) {
            float x = k * period - wrap(off, period);
            if (era == DESERT) {
                sp.setStrokeWidth(5); sp.setColor(alpha(0xffb8782a, 0.45f));
                rc.set(x, gy + 26, x + 150, gy + 50); c.drawArc(rc, 200, 140, false, sp);
                rc.set(x + 100, gy + 82, x + 230, gy + 104); c.drawArc(rc, 200, 140, false, sp);
            } else if (era == CASTLE) {
                p.setColor(alpha(0xff2f8f34, 0.5f)); c.drawRect(x, gy + 20, x + 130, gy + 40, p);
                p.setColor(alpha(0xffffffff, 0.5f)); c.drawCircle(x + 180, gy + 70, 7, p);
                p.setColor(alpha(0xffffd81f, 0.9f)); c.drawCircle(x + 60, gy + 100, 6, p);
            } else if (era == CITY) {
                p.setColor(alpha(0xffffe27a, 0.9f)); c.drawRect(x, gy + 70, x + 130, gy + 80, p);
            } else {
                p.setColor(alpha(0xff4a3f96, 0.55f)); rc.set(x + 20, gy + 28, x + 130, gy + 58); c.drawOval(rc, p);
                rc.set(x + 150, gy + 80, x + 230, gy + 104); c.drawOval(rc, p);
            }
        }
    }

    /** A floating toy block / ball in front of the world, drifting with the scroll. */
    void floaters(Canvas c, float w, float gy, float t) { floaters(c, w, gy, t, false); }

    void floaters(Canvas c, float w, float gy, float t, boolean margins) {
        for (int i = 0; i < 7; i++) {
            float x = wrap(hash(i + 70) * (w + 400) - t * (40 + i * 6), w + 400) - 200;
            float y = 150 + hash(i + 80) * (gy - 420) + (float) Math.sin(t * 1.1f + i * 1.7f) * 22;
            if (margins) {   // centred layouts: only in the outer margins, bobbing in place
                if (i > 5) continue;
                x = i % 2 == 0 ? 80 + hash(i + 70) * 120 : w - 200 + hash(i + 70) * 120;
                y = 200 + (i / 2) * 210 + hash(i + 80) * 60 + (float) Math.sin(t * 1.1f + i * 1.7f) * 22;
            }
            float s = 24 + hash(i + 90) * 26;
            int col = TOYS[(i * 3) % TOYS.length];
            if (i % 2 == 0) cube(c, x, y, s, col, t * 0.6f + i); else sphere(c, x, y, s * 0.85f, col, -1);
        }
    }
}
