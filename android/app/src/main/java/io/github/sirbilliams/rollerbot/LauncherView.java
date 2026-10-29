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
    private float panX = 800, panY = 250, panW = 1010, rowH = 74, rowGap = 14;
    // card metrics: the classic layout, or the compact ones the two centred layouts use
    private float headSz = 56, headY = 80, linesY = 130, lineStep = 40, chipY = 120, chipH = 130, dotY = 295, playRowsY = 330, minH = 440;
    private int maxLines = 99;
    /** 0 = the original left/right layout, 1 = A (tabs on top, one card), 2 = B (stack of buttons, the card replaces it). */
    private int layout;
    private float open;   // layout B: 0 = the stack, 1 = the card
    private final RectF backR = new RectF();
    private final String[][] rawLines = { {}, {}, {}, {} };
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
        rawLines[p] = ls;
        wrap(p);
    }
    private void wrap(int p) {
        String[] ls = rawLines[p];
        java.util.ArrayList<String> out = new java.util.ArrayList<>();
        for (String l : ls) {   // word wrap to the card
            String cur = "";
            for (String wd : l.split(" ")) {
                String nx = cur.isEmpty() ? wd : cur + " " + wd;
                if (!cur.isEmpty() && toy.measure(nx, layout == 0 ? 30 : 28) > panW - 120) { out.add(cur); cur = wd; } else cur = nx;
            }
            out.add(cur);
        }
        while (out.size() > (p == 2 ? maxLines : 5)) out.remove(out.size() - 1);
        lines[p] = out.toArray(new String[0]);
        invalidate();
    }

    /** Layout variants for review: 0 classic, 1 = A (tabs + card), 2 = B (button stack, card replaces it). */
    void setLayout(int l) {
        layout = Math.max(0, Math.min(2, l));
        if (layout == 0) { panX = 800; panY = 250; panW = 1010; rowH = 74; rowGap = 14; headSz = 56; headY = 80; linesY = 130; lineStep = 40; chipY = 120; chipH = 130; dotY = 295; playRowsY = 330; minH = 440; maxLines = 99; }
        else {
            panX = 385; panW = 1150; panY = layout == 1 ? 358 : 292; rowH = 60; rowGap = 10; headSz = 44; headY = 62; linesY = 96; lineStep = 38; chipY = 92; chipH = 108; dotY = 236; playRowsY = 270; minH = 300; maxLines = 2;
        }
        for (int i = 0; i < 4; i++) wrap(i);
        open = col == 1 ? 1 : 0;
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
        android.util.Log.i("Rollerbot", "launcher page " + p);   // the CI smoke test checks the pad moved the pages
        prevEra = era; page = p; era = p; wipe = 0; panelIn = 0; bounceT = 0;
        row = firstEnabled(p);
    }
    private int firstEnabled(int p) { for (int i = 0; i < rows[p].length; i++) if (rows[p][i].enabled) return i; return 0; }

    // ---- input ----------------------------------------------------------------------------------------------
    private void move(int dy) {
        if (layout == 1 && col == 0) { if (dy > 0 && rows[page].length > 0) { col = 1; row = firstEnabled(page); } return; }   // A: Down enters the card
        if (layout == 1 && col == 1 && dy < 0 && row <= firstEnabled(page)) { col = 0; return; }                                // A: Up from the first row
        if (col == 0) { int p = Math.max(0, Math.min(PAGES.length - 1, page + dy)); if (p != page) switchPage(p); return; }
        Row[] rs = rows[page];
        int i = row;
        for (int n = 0; n < rs.length; n++) { i += dy; if (i < 0 || i >= rs.length) return; if (rs[i].enabled) { row = i; bounceT = 0.25f; return; } }
    }
    private void side(int dx) {
        if (layout == 1 && col == 0) { int p = Math.max(0, Math.min(PAGES.length - 1, page + dx)); if (p != page) switchPage(p); return; }   // A: Left / Right move the tabs
        if (col == 0) { if (dx > 0 && rows[page].length > 0) { col = 1; row = firstEnabled(page); } return; }
        Row[] rs = rows[page];
        if (row < rs.length && rs[row].adjustable && rs[row].enabled) { actions.adjust(page, row, dx); bounceT = 0.3f; }
        else if (dx < 0 && layout != 1) col = 0;
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
    /** The page button i: left column (classic), tab row (A) or centred stack (B). */
    private void navRect(int i, RectF o) {
        if (layout == 1) { float w = 330, g = 24, x0 = 960 - (4 * w + 3 * g) / 2; o.set(x0 + i * (w + g), 250, x0 + i * (w + g) + w, 330); }
        else if (layout == 2) { float h = 100, g = 24; o.set(660, 308 + i * (h + g), 1260, 308 + i * (h + g) + h); }
        else o.set(NAV_X, NAV_Y + i * (NAV_H + NAV_GAP), NAV_X + NAV_W, NAV_Y + i * (NAV_H + NAV_GAP) + NAV_H);
    }
    private boolean cardHidden() { return layout == 2 && open < 0.5f; }
    private boolean stackHidden() { return layout == 2 && open > 0.5f; }
    private int hitNav(float x, float y) {
        if (stackHidden()) return -1;
        for (int i = 0; i < PAGES.length; i++) { navRect(i, r2); if (x >= r2.left && x <= r2.right && y >= r2.top && y <= r2.bottom + 14) return i; }
        return -1;
    }
    private int hitRow(float x, float y) {
        if (cardHidden()) return -1;
        float top = rowsTop(page);
        for (int i = 0; i < rows[page].length; i++) {
            float ry = top + i * (rowH + rowGap);
            if (x >= panX + 40 && x <= panX + panW - 40 && y >= ry && y <= ry + rowH) return i;
        }
        return -1;
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        float x = sx(e.getX()), y = sy(e.getY());
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downNav = hitNav(x, y); downRow = hitRow(x, y);
                if (layout == 2 && !cardHidden() && backR.contains(x, y)) { back(); break; }
                if (downNav >= 0) { col = 0; if (downNav != page) switchPage(downNav); }
                else if (downRow >= 0 && rows[page][downRow].enabled) { col = 1; row = downRow; }
                else bounceT = 0;   // a tap in the world makes the ball hop
                break;
            case MotionEvent.ACTION_UP:
                if (downNav >= 0 && hitNav(x, y) == downNav) { if (rows[page].length > 0 && layout != 1) { col = 1; row = firstEnabled(page); } else if (layout == 1) col = 0; }
                else if (downRow >= 0 && hitRow(x, y) == downRow && rows[page][downRow].enabled) {
                    Row rw = rows[page][downRow];
                    if (rw.adjustable) {   // tap the right half to go up, the left quarter to go down; the middle selects
                        float rel = (x - panX) / panW;
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
    private float panH() { return Math.max(minH, rowsTop(page) - panY + rows[page].length * (rowH + rowGap) + 26); }

    private float rowsTop(int p) {
        if (p == 0) return panY + playRowsY;
        return panY + linesY + lines[p].length * lineStep + (progress >= 0 && p == 1 ? 56 : 0) + (layout == 0 ? 16 : 26);
    }

    @Override protected void onDraw(Canvas c) {
        long now = SystemClock.uptimeMillis();
        if (t0 < 0) { t0 = now; lastMs = now; }
        float dt = Math.min(0.05f, (now - lastMs) / 1000f);
        lastMs = now; t = (now - t0) / 1000f;
        wipe = Math.min(1, wipe + dt / 0.85f); panelIn = Math.min(1, panelIn + dt / 0.35f); bounceT += dt;
        spin += dt * 4.2f;
        open += ((col == 1 ? 1f : 0f) - open) * Math.min(1, dt * 7);
        if (layout != 2) open = col == 1 ? 1 : 0;
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
        toy.floaters(c, worldW, gy, t, layout != 0);

        c.save();
        c.translate(ox, oy);
        drawTitle(c);
        if (layout == 2) {   // B: the stack slides away to the left, the card comes in from the right
            float e = Toy.ease(open);
            if (e < 0.98f) { c.save(); c.translate(-e * 1500, 0); drawNav(c); c.restore(); }
            if (e > 0.02f) { c.save(); c.translate((1 - e) * 1500, 0); drawPanel(c); c.restore(); }
        } else {
            drawNav(c);
            float slide = (1 - Toy.ease(panelIn)) * (layout == 0 ? 70 : 0), lift = layout == 0 ? 0 : (1 - Toy.ease(panelIn)) * 26;
            c.save(); c.translate(slide, lift); drawPanel(c); c.restore();
        }
        c.restore();

        // the ball: rolls in place on the ground (the world scrolls past), hops on every action
        float hop = bounceT < 0.6f ? (float) Math.sin(bounceT / 0.6f * Math.PI) * 190 : 0;
        float bx = layout == 0 ? ox + 1530 : ox + 150, br = 66;
        toy.ball(c, bx, gy + 56 - br - hop + br * 0.9f - 20, br, spin, gy + 62);
        if (layout != 0) {   // a small toy stack in the opposite margin keeps the bottom balanced
            toy.cube(c, ox + 1790, gy + 4, 40, Toy.TOYS[1], 0.4f);
            toy.cube(c, ox + 1730, gy + 4, 30, Toy.TOYS[5], 1.1f);
        }
        drawHints(c);
        c.restore();
        postInvalidateOnAnimation();
    }

    private void drawTitle(Canvas c) {
        if (layout != 0) {
            float sz = 132;
            float w = toy.measure("ROLLERBOT", sz) + sz * 0.01f * 9;
            toy.wordmark(c, "ROLLERBOT", 960 - w / 2, 148, sz, t);
            String s2 = "A TIME JOURNEY";
            float bw = toy.measure(s2, 38) + 64;
            r.set(960 - bw / 2, 168, 960 + bw / 2, 220);
            toy.block(c, r, 16, 7, 0xffff9a1f, false);
            toy.text(c, s2, 960, 206, 38, 0xffffffff, 0);
            return;
        }
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
            navRect(i, r);
            float x = r.left, y = r.top, w = r.width(), h = r.height(), s = navScale[i], cx = x + w / 2, cy = y + h / 2;
            boolean sel = i == page;
            c.save();
            if (layout == 0) c.translate(sel ? 16 * (s - 1) / 0.07f * 0.7f : 0, 0);
            c.translate(cx, cy); c.scale(s, s); c.translate(-cx, -cy);
            int face = sel ? PAGE_COL[i] : Toy.shade(PAGE_COL[i], -0.12f);
            toy.block(c, r, layout == 1 ? 40 : 30, sel ? 12 : 8, face, sel && col == 0);
            float rad = layout == 1 ? 28 : 36, bx = x + (layout == 1 ? 50 : 62);
            pp.setShader(null); pp.setStyle(Paint.Style.FILL); pp.setColor(Toy.alpha(0xff000000, 0.22f)); c.drawCircle(bx, cy + 3, rad, pp);
            icon(c, i, bx, cy, rad * 1.66f, Toy.shade(face, -0.2f));
            if (layout == 1) toy.text(c, PAGES[i], x + 94, cy + 14, 42, 0xffffffff, -1);
            else {
                toy.text(c, PAGES[i], x + 120, y + 52, 50, 0xffffffff, -1);
                toy.text(c, SUB[i], x + 122, y + 86, 24, Toy.alpha(0xffffffff, 0.8f), -1);
            }
            c.restore();
        }
    }

    private void drawPanel(Canvas c) {
        final float PAN_H = panH();
        r.set(panX, panY, panX + panW, panY + PAN_H);
        pp.setShader(null); pp.setStyle(Paint.Style.FILL);
        pp.setColor(Toy.alpha(0xff000000, 0.25f)); r2.set(r.left + 6, r.top + 14, r.right + 6, r.bottom + 14); c.drawRoundRect(r2, 44, 44, pp);
        pp.setColor(Toy.shade(PAGE_COL[page], -0.35f)); r2.set(r.left, r.top + 10, r.right, r.bottom + 10); c.drawRoundRect(r2, 44, 44, pp);
        pp.setColor(0xfb152a5c); c.drawRoundRect(r, 44, 44, pp);
        pp.setStyle(Paint.Style.STROKE); pp.setStrokeWidth(8); pp.setColor(PAGE_COL[page]); c.drawRoundRect(r, 44, 44, pp);
        pp.setStyle(Paint.Style.FILL); pp.setColor(Toy.alpha(0xffffffff, 0.07f)); r2.set(r.left + 14, r.top + 14, r.right - 14, r.top + (layout == 0 ? 100 : 84)); c.drawRoundRect(r2, 32, 32, pp);
        if (layout == 0) {
            toy.text(c, heading[page], panX + 48, panY + headY, headSz, 0xffffffff, -1);
            toy.text(c, Toy.ERA_NAMES[page] + " era", panX + panW - 48, panY + 76, 30, Toy.alpha(PAGE_COL[page], 1f), 1);
        } else {
            toy.text(c, heading[page], panX + panW / 2, panY + headY, headSz, 0xffffffff, 0);
            if (layout == 2) {   // Back pill, touch target and a reminder of the B button
                backR.set(panX + 34, panY + 20, panX + 190, panY + 76);
                toy.block(c, backR, 24, 6, 0xffff9a1f, false);
                toy.text(c, "Back", backR.centerX() + 16, backR.centerY() + 12, 32, 0xffffffff, 0);
                pp.setStyle(Paint.Style.FILL); pp.setColor(0xffffffff);
                tri.reset(); tri.moveTo(backR.left + 40, backR.centerY() - 11); tri.lineTo(backR.left + 40, backR.centerY() + 11); tri.lineTo(backR.left + 24, backR.centerY()); tri.close(); c.drawPath(tri, pp);
            }
        }

        if (page == 0) drawPlayCard(c);
        else {
            float y = panY + linesY;
            for (String l : lines[page]) { toy.text(c, l, layout == 0 ? panX + 56 : panX + panW / 2, y + 20, layout == 0 ? 30 : 28, 0xffd6e2ff, layout == 0 ? -1 : 0); y += lineStep; }
            if (page == 1 && progress >= 0) {
                y += 8;
                r2.set(panX + 56, y, panX + panW - 56, y + 34);
                pp.setColor(0xff0a1438); c.drawRoundRect(r2, 17, 17, pp);
                r2.set(panX + 60, y + 4, panX + 60 + Math.max(30, (panW - 120) * Math.min(1, progress)), y + 30);
                pp.setColor(0xffff9a1f); c.drawRoundRect(r2, 13, 13, pp);
                toy.text(c, progressLabel, panX + panW / 2, y + 27, 22, 0xffffffff, 0);
            }
        }
        drawRows(c);
    }

    private void drawPlayCard(Canvas c) {
        String[] cap = { hasGame ? "LEVEL" : "NO GAME", "HIGH SCORE", "LAST SCORE" };
        String[] val = { hasGame ? String.valueOf(Math.max(1, level)) : "-", String.valueOf(hi), String.valueOf(last) };
        int[] cols = { 0xff3cc84a, 0xffffd81f, 0xff1fc4b4 };
        float cw = (panW - 48 * 2 - 40) / 3f;
        for (int i = 0; i < 3; i++) {
            float x = panX + 48 + i * (cw + 20), y = panY + chipY, k = chipH / 130f;
            r.set(x, y, x + cw, y + chipH);
            toy.block(c, r, 26, 8, Toy.shade(cols[i], -0.2f), false);
            toy.text(c, cap[i], x + cw / 2, y + 38 * k, 24, Toy.alpha(0xffffffff, 0.8f), 0);
            toy.text(c, val[i], x + cw / 2, y + 100 * k, 56 * (0.6f + 0.4f * k), cols[i] == 0xffffd81f ? 0xffffe46a : 0xffffffff, 0);
        }
        // one dot per level: lit = unlocked, ring = the one that will be played
        float dy = panY + dotY;
        int n = Math.max(total, 1);
        float step = Math.min(56, (panW - 120) / n);
        float dx0 = panX + panW / 2 - (n - 1) * step / 2;
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
            float y = top + i * (rowH + rowGap), s = rowScale[Math.min(i, rowScale.length - 1)];
            boolean sel = col == 1 && i == row;
            float cx = panX + panW / 2, cy = y + rowH / 2;
            c.save(); c.translate(cx, cy); c.scale(s, s); c.translate(-cx, -cy);
            r.set(panX + 40, y, panX + panW - 40, y + rowH);
            int face = !rw.enabled ? 0xff39466e : sel ? PAGE_COL[page] : 0xff2e4fa6;
            toy.block(c, r, 24, sel ? 10 : 7, face, sel);
            int tc = rw.enabled ? 0xffffffff : 0xff8d9bc4;
            toy.text(c, rw.label, panX + 76, y + rowH / 2 + 13 * rowH / 74f, 38 * (0.75f + 0.25f * rowH / 74f), tc, -1);
            if (rw.value != null && !rw.value.isEmpty()) {
                float vr = panX + panW - (rw.adjustable ? 130 : 76);
                toy.text(c, rw.value, vr, y + rowH / 2 + 12, 34, rw.enabled ? 0xffffe46a : tc, 1);
                if (rw.adjustable && rw.enabled) {
                    float ax = panX + panW - 106, lx = vr - toy.measure(rw.value, 34) - 18;
                    float nudge = sel ? (float) Math.sin(t * 8) * 3 : 0;
                    pp.setStyle(Paint.Style.FILL); pp.setColor(0xffffffff);
                    tri.reset(); tri.moveTo(ax + 18 + nudge, y + 22); tri.lineTo(ax + 18 + nudge, y + rowH - 22); tri.lineTo(ax + 44 + nudge, y + rowH / 2); tri.close(); c.drawPath(tri, pp);
                    tri.reset(); tri.moveTo(lx - nudge, y + 22); tri.lineTo(lx - nudge, y + rowH - 22); tri.lineTo(lx - 26 - nudge, y + rowH / 2); tri.close(); c.drawPath(tri, pp);
                }
            }
            c.restore();
        }
    }

    private void drawHints(Canvas c) {
        String s = layout == 1 ? (col == 0 ? "Left / Right  choose     Down or A  open     B  quit" : "Up / Down  choose     Left / Right  change     A  select     B  back")
                : col == 0 ? "Up / Down  choose     Right or A  open     B  quit"
                : "Up / Down  choose     Left / Right  change     A  select     B  back";
        float w = toy.measure(s, 28) + 56;
        float hx = layout == 0 ? NAV_X : 960 - w / 2;
        r.set(hx, GROUND + 28, hx + w, GROUND + 80);
        pp.setStyle(Paint.Style.FILL); pp.setShader(null); pp.setColor(Toy.alpha(0xff0b1030, 0.72f));
        c.drawRoundRect(r, 26, 26, pp);
        toy.text(c, s, hx + 28, GROUND + 65, 28, 0xffffffff, -1);
    }

    @Override protected void onDetachedFromWindow() { super.onDetachedFromWindow(); }
}
