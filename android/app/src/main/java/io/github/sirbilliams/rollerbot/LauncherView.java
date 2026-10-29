package io.github.sirbilliams.rollerbot;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

/**
 * The launcher, drawn entirely in code on a 1920 x 1080 stage (letterboxed, the world behind it fills the view): a toy-like
 * time journey where each page is an era (Play: desert, Import: castle, Settings: city, About: space) and a red ball rolls
 * through it. Left: four chunky page buttons; right: the page card. Pad (D-pad / left stick / A / B) and touch both work.
 * See docs/launcher.md. SetupActivity owns the logic and feeds pages through setRows / setLines / setPlay / setProgress.
 */
final class LauncherView extends View {
    static final String[] PAGES = { "Play", "Import", "Settings", "About" };
    private static final String[] SUB = { "Roll on", "Bring your disc", "Make it yours", "Who made this" };
    private static final int[] PAGE_COL = { 0xff3cc84a, 0xffff9a1f, 0xff2f8df0, 0xff8b5cf0 };

    interface Actions {
        void select(int page, int row);
        void adjust(int page, int row, int dir);
        /** B / Back at the top level: leave the launcher. */
        void quit();
    }

    static final class Row {
        final String label, value;
        final boolean enabled, adjustable;
        Row(String label, String value, boolean enabled, boolean adjustable) { this.label = label; this.value = value; this.enabled = enabled; this.adjustable = adjustable; }
    }

    // stage geometry (1920 x 1080)
    private static final float NAV_X = 110, NAV_Y = 330, NAV_W = 600, NAV_H = 104, NAV_GAP = 30;
    private static final float PAN_X = 800, PAN_Y = 250, PAN_W = 1010, ROW_H = 74, ROW_GAP = 14;
    private static final float GROUND = 934;

    private final Toy toy = new Toy();
    private final Actions actions;
    private final Row[][] rows = { {}, {}, {}, {} };
    private final String[][] lines = { {}, {}, {}, {} };
    private final String[] heading = { "Play", "Import your game", "Settings", "About" };

    // play card data
    private boolean hasGame;
    private int level, hi, last, unlocked, total, pickLevel;
    private float progress = -1;
    private String progressLabel = "";

    // focus and animation state
    private int page, col, row;
    private int era, prevEra = -1;
    private long t0 = -1, lastMs;
    private float t, wipe = 1, panelIn = 1, bounceT = 9, spin;
    private final float[] navScale = { 1, 1, 1, 1 }, rowScale = new float[6];
    private float stickX, stickY;
    private boolean stickLatchX, stickLatchY;
    private int downNav = -1, downRow = -1;
    private float scale = 1, ox, oy, worldW = 1920, worldH = 1080;
    private final Path tri = new Path();
    private final RectF r = new RectF(), r2 = new RectF();
    private final Paint pp = new Paint(Paint.ANTI_ALIAS_FLAG);

    LauncherView(Context c, Actions a) {
        super(c);
        actions = a;
        setFocusable(true);
        setFocusableInTouchMode(true);
        setContentDescription("Rollerbot launcher");
    }

    // ---- model ----------------------------------------------------------------------------------------------
    void setRows(int p, Row[] rs) { rows[p] = rs; if (p == page && row >= rs.length) row = Math.max(0, rs.length - 1); invalidate(); }
    void setLines(int p, String head, String... ls) {
        heading[p] = head;
        java.util.ArrayList<String> out = new java.util.ArrayList<>();
        for (String l : ls) {   // word wrap to the card
            String cur = "";
            for (String wd : l.split(" ")) {
                String nx = cur.isEmpty() ? wd : cur + " " + wd;
                if (!cur.isEmpty() && toy.measure(nx, 30) > PAN_W - 120) { out.add(cur); cur = wd; } else cur = nx;
            }
            out.add(cur);
        }
        lines[p] = out.toArray(new String[0]);
        invalidate();
    }
    void setPlay(boolean has, int lvl, int hiScore, int lastScore, int unlockedLevels, int totalLevels, int picked) {
        hasGame = has; level = lvl; hi = hiScore; last = lastScore; unlocked = unlockedLevels; total = totalLevels; pickLevel = picked; invalidate();
    }
    void setProgress(float f, String label) { progress = f; progressLabel = label; invalidate(); }
    int page() { return page; }
    boolean inContent() { return col == 1; }
    void showPage(int p) { if (p != page) { switchPage(p); } }
    int row() { return row; }

    private void switchPage(int p) {
        prevEra = era; page = p; era = p; wipe = 0; panelIn = 0; bounceT = 0;
        row = firstEnabled(p);
    }
    private int firstEnabled(int p) { for (int i = 0; i < rows[p].length; i++) if (rows[p][i].enabled) return i; return 0; }

    // ---- input ----------------------------------------------------------------------------------------------
    private void move(int dy) {
        if (col == 0) { int p = Math.max(0, Math.min(PAGES.length - 1, page + dy)); if (p != page) switchPage(p); return; }
        Row[] rs = rows[page];
        int i = row;
        for (int n = 0; n < rs.length; n++) { i += dy; if (i < 0 || i >= rs.length) return; if (rs[i].enabled) { row = i; bounceT = 0.25f; return; } }
    }
    private void side(int dx) {
        if (col == 0) { if (dx > 0 && rows[page].length > 0) { col = 1; row = firstEnabled(page); } return; }
        Row[] rs = rows[page];
        if (row < rs.length && rs[row].adjustable && rs[row].enabled) { actions.adjust(page, row, dx); bounceT = 0.3f; }
        else if (dx < 0) col = 0;
    }
    private void accept() {
        if (col == 0) { if (rows[page].length > 0) { col = 1; row = firstEnabled(page); } return; }
        Row[] rs = rows[page];
        if (row < rs.length && rs[row].enabled) { actions.select(page, row); bounceT = 0; }
    }
    /** Returns true when B stepped back inside the launcher; false at the top level (the Activity leaves). */
    boolean back() { if (col == 1) { col = 0; return true; } return false; }

    @Override public boolean onKeyDown(int k, KeyEvent e) {
        switch (k) {
            case KeyEvent.KEYCODE_DPAD_UP: move(-1); break;
            case KeyEvent.KEYCODE_DPAD_DOWN: move(1); break;
            case KeyEvent.KEYCODE_DPAD_LEFT: side(-1); break;
            case KeyEvent.KEYCODE_DPAD_RIGHT: side(1); break;
            case KeyEvent.KEYCODE_BUTTON_A: case KeyEvent.KEYCODE_DPAD_CENTER: case KeyEvent.KEYCODE_ENTER:
                if (e == null || e.getRepeatCount() == 0) accept();
                break;
            case KeyEvent.KEYCODE_BUTTON_B: case KeyEvent.KEYCODE_BACK:
                if (!back()) actions.quit();
                break;
            default: return false;
        }
        invalidate();
        return true;
    }

    @Override public boolean onGenericMotionEvent(MotionEvent e) {
        float x = e.getAxisValue(MotionEvent.AXIS_X), y = e.getAxisValue(MotionEvent.AXIS_Y);
        if (Math.abs(e.getAxisValue(MotionEvent.AXIS_HAT_X)) > 0.5f) x = e.getAxisValue(MotionEvent.AXIS_HAT_X);
        if (Math.abs(e.getAxisValue(MotionEvent.AXIS_HAT_Y)) > 0.5f) y = e.getAxisValue(MotionEvent.AXIS_HAT_Y);
        // latch at 0.6, release under 0.3: one move per push
        if (Math.abs(y) > 0.6f) { if (!stickLatchY) { stickLatchY = true; move(y > 0 ? 1 : -1); } } else if (Math.abs(y) < 0.3f) stickLatchY = false;
        if (Math.abs(x) > 0.6f) { if (!stickLatchX) { stickLatchX = true; side(x > 0 ? 1 : -1); } } else if (Math.abs(x) < 0.3f) stickLatchX = false;
        invalidate();
        return true;
    }

    private float sx(float px) { return px / scale - ox; }
    private float sy(float py) { return py / scale - oy; }
    private int hitNav(float x, float y) {
        for (int i = 0; i < PAGES.length; i++) if (x >= NAV_X && x <= NAV_X + NAV_W && y >= NAV_Y + i * (NAV_H + NAV_GAP) && y <= NAV_Y + i * (NAV_H + NAV_GAP) + NAV_H + 14) return i;
        return -1;
    }
    private int hitRow(float x, float y) {
        float top = rowsTop(page);
        for (int i = 0; i < rows[page].length; i++) {
            float ry = top + i * (ROW_H + ROW_GAP);
            if (x >= PAN_X + 40 && x <= PAN_X + PAN_W - 40 && y >= ry && y <= ry + ROW_H) return i;
        }
        return -1;
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        float x = sx(e.getX()), y = sy(e.getY());
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downNav = hitNav(x, y); downRow = hitRow(x, y);
                if (downNav >= 0) { col = 0; if (downNav != page) switchPage(downNav); }
                else if (downRow >= 0 && rows[page][downRow].enabled) { col = 1; row = downRow; }
                else bounceT = 0;   // a tap in the world makes the ball hop
                break;
            case MotionEvent.ACTION_UP:
                if (downNav >= 0 && hitNav(x, y) == downNav) { if (rows[page].length > 0) { col = 1; row = firstEnabled(page); } }
                else if (downRow >= 0 && hitRow(x, y) == downRow && rows[page][downRow].enabled) {
                    Row rw = rows[page][downRow];
                    if (rw.adjustable) {   // tap the right half to go up, the left quarter to go down; the middle selects
                        float rel = (x - PAN_X) / PAN_W;
                        if (rel > 0.72f) actions.adjust(page, downRow, 1); else if (rel > 0.5f) actions.adjust(page, downRow, -1); else actions.select(page, downRow);
                    } else actions.select(page, downRow);
                    bounceT = 0;
                }
                downNav = downRow = -1;
                break;
            case MotionEvent.ACTION_CANCEL: downNav = downRow = -1; break;
            default: break;
        }
        invalidate();
        return true;
    }

    // ---- drawing --------------------------------------------------------------------------------------------
    private float panH() { return Math.max(440, rowsTop(page) - PAN_Y + rows[page].length * (ROW_H + ROW_GAP) + 26); }

    private float rowsTop(int p) {
        if (p == 0) return PAN_Y + 330;
        return PAN_Y + 130 + lines[p].length * 40 + (progress >= 0 && p == 1 ? 56 : 0) + 16;
    }

    @Override protected void onDraw(Canvas c) {
        long now = SystemClock.uptimeMillis();
        if (t0 < 0) { t0 = now; lastMs = now; }
        float dt = Math.min(0.05f, (now - lastMs) / 1000f);
        lastMs = now; t = (now - t0) / 1000f;
        wipe = Math.min(1, wipe + dt / 0.85f); panelIn = Math.min(1, panelIn + dt / 0.35f); bounceT += dt;
        spin += dt * 4.2f;
        for (int i = 0; i < 4; i++) navScale[i] += ((i == page ? (col == 0 ? 1.07f : 1.03f) : 1f) - navScale[i]) * Math.min(1, dt * 14);
        for (int i = 0; i < rowScale.length; i++) rowScale[i] += (((col == 1 && i == row) ? 1.04f : 1f) - rowScale[i]) * Math.min(1, dt * 14);

        int w = getWidth(), h = getHeight();
        scale = Math.min(w / 1920f, h / 1080f);
        worldW = w / scale; worldH = h / scale;
        ox = (worldW - 1920) / 2; oy = (worldH - 1080) / 2;
        c.save();
        c.scale(scale, scale);
        float gy = oy + GROUND;

        // world: the previous era, then the new one wiped in from the left behind a bright edge
        if (wipe < 1 && prevEra >= 0) {
            toy.world(c, prevEra, worldW, worldH, gy, t);
            float wx = Toy.ease(wipe) * (worldW + 120) - 60;
            c.save(); c.clipRect(0, 0, wx, worldH); toy.world(c, era, worldW, worldH, gy, t); c.restore();
            pp.setStyle(Paint.Style.FILL);
            pp.setColor(Toy.alpha(0xffffffff, 0.8f * (1 - Math.abs(wipe - 0.5f) * 1.2f))); c.drawRect(wx - 5, 0, wx + 5, worldH, pp);
            pp.setColor(Toy.alpha(0xffffffff, 0.22f * (1 - Math.abs(wipe - 0.5f)))); c.drawRect(wx - 40, 0, wx, worldH, pp);
        } else toy.world(c, era, worldW, worldH, gy, t);
        toy.floaters(c, worldW, gy, t);

        c.save();
        c.translate(ox, oy);
        drawTitle(c);
        drawNav(c);
        float slide = (1 - Toy.ease(panelIn)) * 70;
        c.save(); c.translate(slide, 0); drawPanel(c); c.restore();
        c.restore();

        // the ball: rolls in place on the ground (the world scrolls past), hops on every action
        float hop = bounceT < 0.6f ? (float) Math.sin(bounceT / 0.6f * Math.PI) * 190 : 0;
        float bx = ox + 1530, br = 66;
        toy.ball(c, bx, gy + 56 - br - hop + br * 0.9f - 20, br, spin, gy + 62);
        drawHints(c);
        c.restore();
        postInvalidateOnAnimation();
    }

    private void drawTitle(Canvas c) {
        toy.wordmark(c, "ROLLERBOT", NAV_X - 6, 190, 160, t);
        String s = "A TIME JOURNEY";
        float bw = toy.measure(s, 44) + 70;
        r.set(NAV_X, 222, NAV_X + bw, 282);
        toy.block(c, r, 18, 8, 0xffff9a1f, false);
        toy.text(c, s, NAV_X + 35, 266, 44, 0xffffffff, -1);
        // era chips
        pp.setStyle(Paint.Style.FILL); pp.setShader(null); pp.setColor(Toy.alpha(0xff0b1030, 0.55f)); r.set(NAV_X + bw + 14, 236, NAV_X + bw + 30 + 4 * 46 - 10, 268); c.drawRoundRect(r, 16, 16, pp);
        for (int i = 0; i < 4; i++) {
            float x = NAV_X + bw + 30 + i * 46;
            pp.setStyle(Paint.Style.FILL); pp.setColor(i == era ? 0xffffffff : Toy.alpha(0xffffffff, 0.35f));
            c.drawCircle(x, 252, i == era ? 13 : 9, pp);
        }
    }

    private void icon(Canvas c, int i, float cx, float cy, float s, int col) {
        pp.setStyle(Paint.Style.FILL); pp.setShader(null);
        pp.setColor(0xffffffff);
        switch (i) {
            case 0: tri.reset(); tri.moveTo(cx - s * 0.35f, cy - s * 0.5f); tri.lineTo(cx + s * 0.55f, cy); tri.lineTo(cx - s * 0.35f, cy + s * 0.5f); tri.close(); c.drawPath(tri, pp); break;
            case 1: r2.set(cx - s * 0.5f, cy + s * 0.15f, cx + s * 0.5f, cy + s * 0.5f); c.drawRoundRect(r2, 6, 6, pp);
                tri.reset(); tri.moveTo(cx - s * 0.32f, cy - s * 0.05f); tri.lineTo(cx + s * 0.32f, cy - s * 0.05f); tri.lineTo(cx, cy + s * 0.3f); tri.close(); c.drawPath(tri, pp);
                r2.set(cx - s * 0.1f, cy - s * 0.55f, cx + s * 0.1f, cy); c.drawRect(r2, pp); break;
            case 2: c.drawCircle(cx, cy, s * 0.36f, pp);
                for (int k = 0; k < 8; k++) { c.save(); c.rotate(k * 45 + t * 12, cx, cy); r2.set(cx - s * 0.09f, cy - s * 0.55f, cx + s * 0.09f, cy - s * 0.3f); c.drawRect(r2, pp); c.restore(); }
                pp.setColor(col); c.drawCircle(cx, cy, s * 0.15f, pp); break;
            default: c.drawCircle(cx, cy, s * 0.5f, pp); pp.setColor(col);
                c.drawCircle(cx, cy - s * 0.26f, s * 0.07f, pp); r2.set(cx - s * 0.065f, cy - s * 0.12f, cx + s * 0.065f, cy + s * 0.28f); c.drawRect(r2, pp); break;
        }
    }

    private void drawNav(Canvas c) {
        for (int i = 0; i < PAGES.length; i++) {
            float y = NAV_Y + i * (NAV_H + NAV_GAP), s = navScale[i], cx = NAV_X + NAV_W / 2, cy = y + NAV_H / 2;
            boolean sel = i == page;
            c.save();
            c.translate(sel ? 16 * (s - 1) / 0.07f * 0.7f : 0, 0);
            c.translate(cx, cy); c.scale(s, s); c.translate(-cx, -cy);
            r.set(NAV_X, y, NAV_X + NAV_W, y + NAV_H);
            int face = sel ? PAGE_COL[i] : Toy.shade(PAGE_COL[i], -0.12f);
            toy.block(c, r, 30, sel ? 12 : 8, face, sel && col == 0);
            // round badge with the icon
            pp.setShader(null); pp.setStyle(Paint.Style.FILL); pp.setColor(Toy.alpha(0xff000000, 0.22f)); c.drawCircle(NAV_X + 62, y + NAV_H / 2 + 3, 36, pp);
            icon(c, i, NAV_X + 62, y + NAV_H / 2, 60, Toy.shade(face, -0.2f));
            toy.text(c, PAGES[i], NAV_X + 120, y + 52, 50, 0xffffffff, -1);
            toy.text(c, SUB[i], NAV_X + 122, y + 86, 24, Toy.alpha(0xffffffff, 0.8f), -1);
            c.restore();
        }
    }

    private void drawPanel(Canvas c) {
        final float PAN_H = panH();
        r.set(PAN_X, PAN_Y, PAN_X + PAN_W, PAN_Y + PAN_H);
        pp.setShader(null); pp.setStyle(Paint.Style.FILL);
        pp.setColor(Toy.alpha(0xff000000, 0.25f)); r2.set(r.left + 6, r.top + 14, r.right + 6, r.bottom + 14); c.drawRoundRect(r2, 44, 44, pp);
        pp.setColor(Toy.shade(PAGE_COL[page], -0.35f)); r2.set(r.left, r.top + 10, r.right, r.bottom + 10); c.drawRoundRect(r2, 44, 44, pp);
        pp.setColor(0xfb152a5c); c.drawRoundRect(r, 44, 44, pp);
        pp.setStyle(Paint.Style.STROKE); pp.setStrokeWidth(8); pp.setColor(PAGE_COL[page]); c.drawRoundRect(r, 44, 44, pp);
        pp.setStyle(Paint.Style.FILL); pp.setColor(Toy.alpha(0xffffffff, 0.07f)); r2.set(r.left + 14, r.top + 14, r.right - 14, r.top + 100); c.drawRoundRect(r2, 32, 32, pp);
        toy.text(c, heading[page], PAN_X + 48, PAN_Y + 80, 56, 0xffffffff, -1);
        // era tag on the right of the heading
        toy.text(c, Toy.ERA_NAMES[page] + " era", PAN_X + PAN_W - 48, PAN_Y + 76, 30, Toy.alpha(PAGE_COL[page], 1f), 1);

        if (page == 0) drawPlayCard(c);
        else {
            float y = PAN_Y + 130;
            for (String l : lines[page]) { toy.text(c, l, PAN_X + 56, y + 20, 30, 0xffd6e2ff, -1); y += 40; }
            if (page == 1 && progress >= 0) {
                y += 8;
                r2.set(PAN_X + 56, y, PAN_X + PAN_W - 56, y + 34);
                pp.setColor(0xff0a1438); c.drawRoundRect(r2, 17, 17, pp);
                r2.set(PAN_X + 60, y + 4, PAN_X + 60 + Math.max(30, (PAN_W - 120) * Math.min(1, progress)), y + 30);
                pp.setColor(0xffff9a1f); c.drawRoundRect(r2, 13, 13, pp);
                toy.text(c, progressLabel, PAN_X + PAN_W / 2, y + 27, 22, 0xffffffff, 0);
            }
        }
        drawRows(c);
    }

    private void drawPlayCard(Canvas c) {
        String[] cap = { hasGame ? "LEVEL" : "NO GAME", "HIGH SCORE", "LAST SCORE" };
        String[] val = { hasGame ? String.valueOf(Math.max(1, level)) : "-", String.valueOf(hi), String.valueOf(last) };
        int[] cols = { 0xff3cc84a, 0xffffd81f, 0xff1fc4b4 };
        float cw = (PAN_W - 48 * 2 - 40) / 3f;
        for (int i = 0; i < 3; i++) {
            float x = PAN_X + 48 + i * (cw + 20), y = PAN_Y + 120;
            r.set(x, y, x + cw, y + 130);
            toy.block(c, r, 26, 8, Toy.shade(cols[i], -0.2f), false);
            toy.text(c, cap[i], x + cw / 2, y + 38, 24, Toy.alpha(0xffffffff, 0.8f), 0);
            toy.text(c, val[i], x + cw / 2, y + 100, 56, cols[i] == 0xffffd81f ? 0xffffe46a : 0xffffffff, 0);
        }
        // one dot per level: lit = unlocked, ring = the one that will be played
        float dy = PAN_Y + 295;
        int n = Math.max(total, 1);
        float step = Math.min(56, (PAN_W - 120) / n);
        float dx0 = PAN_X + PAN_W / 2 - (n - 1) * step / 2;
        for (int i = 0; i < n; i++) {
            boolean on = i < unlocked;
            toy.sphere(c, dx0 + i * step, dy + (i + 1 == pickLevel ? -6 : 0), 17, on ? Toy.TOYS[i % Toy.TOYS.length] : 0xff3a4a7a, -1);
            if (i + 1 == pickLevel && hasGame) { pp.setStyle(Paint.Style.STROKE); pp.setStrokeWidth(4); pp.setColor(0xffffffff); c.drawCircle(dx0 + i * step, dy - 6, 24, pp); pp.setStyle(Paint.Style.FILL); }
        }
    }

    private void drawRows(Canvas c) {
        Row[] rs = rows[page];
        float top = rowsTop(page);
        for (int i = 0; i < rs.length; i++) {
            Row rw = rs[i];
            float y = top + i * (ROW_H + ROW_GAP), s = rowScale[Math.min(i, rowScale.length - 1)];
            boolean sel = col == 1 && i == row;
            float cx = PAN_X + PAN_W / 2, cy = y + ROW_H / 2;
            c.save(); c.translate(cx, cy); c.scale(s, s); c.translate(-cx, -cy);
            r.set(PAN_X + 40, y, PAN_X + PAN_W - 40, y + ROW_H);
            int face = !rw.enabled ? 0xff39466e : sel ? PAGE_COL[page] : 0xff2e4fa6;
            toy.block(c, r, 24, sel ? 10 : 7, face, sel);
            int tc = rw.enabled ? 0xffffffff : 0xff8d9bc4;
            toy.text(c, rw.label, PAN_X + 76, y + ROW_H / 2 + 13, 38, tc, -1);
            if (rw.value != null && !rw.value.isEmpty()) {
                float vr = PAN_X + PAN_W - (rw.adjustable ? 130 : 76);
                toy.text(c, rw.value, vr, y + ROW_H / 2 + 12, 34, rw.enabled ? 0xffffe46a : tc, 1);
                if (rw.adjustable && rw.enabled) {
                    float ax = PAN_X + PAN_W - 106, lx = vr - toy.measure(rw.value, 34) - 18;
                    float nudge = sel ? (float) Math.sin(t * 8) * 3 : 0;
                    pp.setStyle(Paint.Style.FILL); pp.setColor(0xffffffff);
                    tri.reset(); tri.moveTo(ax + 18 + nudge, y + 22); tri.lineTo(ax + 18 + nudge, y + ROW_H - 22); tri.lineTo(ax + 44 + nudge, y + ROW_H / 2); tri.close(); c.drawPath(tri, pp);
                    tri.reset(); tri.moveTo(lx - nudge, y + 22); tri.lineTo(lx - nudge, y + ROW_H - 22); tri.lineTo(lx - 26 - nudge, y + ROW_H / 2); tri.close(); c.drawPath(tri, pp);
                }
            }
            c.restore();
        }
    }

    private void drawHints(Canvas c) {
        String s = col == 0 ? "Up / Down  choose     Right or A  open     B  quit"
                : "Up / Down  choose     Left / Right  change     A  select     B  back";
        float w = toy.measure(s, 28) + 56;
        r.set(NAV_X, GROUND + 28, NAV_X + w, GROUND + 80);
        pp.setStyle(Paint.Style.FILL); pp.setShader(null); pp.setColor(Toy.alpha(0xff0b1030, 0.72f));
        c.drawRoundRect(r, 26, 26, pp);
        toy.text(c, s, NAV_X + 28, GROUND + 65, 28, 0xffffffff, -1);
    }

    @Override protected void onDetachedFromWindow() { super.onDetachedFromWindow(); }
}
