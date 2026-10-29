package io.github.sirbilliams.rollerbot;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * The bottom-screen companion, in the launcher's toy style (see Toy / LauncherView): Now (live status, pause, restart),
 * Levels (unlocked levels, tap to jump) and Screen (widescreen mode, controls). Drawn on a 1240 x 1080 stage; touch only.
 * CompanionPanel polls rb::Bridge and calls setStatus; the harness in tools/launcher renders it on the desktop.
 */
final class CompanionView extends View {
    interface Host {
        void pause(boolean on);
        void restart();
        void level(int n);
        void widescreen(int mode);
        void volume(int pct);
        void language(int idx);
        int reached();
    }

    static final String[] TABS = { "Now", "Levels", "Settings" };
    static final String[] WS = { "Fill", "Side panels", "Stretch", "Original 4:3" };
    private static final int[] TAB_COL = { 0xff3cc84a, 0xffff9a1f, 0xff2f8df0 };
    private static final float W = 1240, H = 1080, GROUND = 930;

    private final Toy toy = new Toy();
    private final Host host;
    private Native.Status st = new Native.Status();
    private int hiScore;
    private boolean menuOpen;
    private int tab;
    private int era;
    private int prevEra = -1;
    private long t0 = -1, lastMs;
    private float t, wipe = 1, hop = 9, spin;
    private float scale = 1, ox, oy, worldW = W, worldH = H;
    private final List<RectF> hit = new ArrayList<>();
    private final List<Runnable> act = new ArrayList<>();
    private final RectF r = new RectF();
    private final Paint pp = new Paint(Paint.ANTI_ALIAS_FLAG);

    CompanionView(Context c, Host h) {
        super(c);
        host = h;
        setContentDescription("Rollerbot companion");
    }

    void setStatus(Native.Status s, int highScore, boolean menu) { st = s; hiScore = highScore; menuOpen = menu; invalidate(); }
    void showTab(int i) { if (i != tab) { prevEra = era; tab = i; era = i == 0 ? Toy.CASTLE : i == 1 ? Toy.DESERT : Toy.SPACE; wipe = 0; hop = 0; } }
    int tab() { return tab; }

    private void button(Canvas c, String label, String value, float x, float y, float w, float h, int face, boolean glow, Runnable a) {
        r.set(x, y, x + w, y + h);
        toy.block(c, r, 26, 9, face, glow);
        float ls = Math.min(40, 40 * (w - 24) / Math.max(1, toy.measure(label, 40)));
        toy.text(c, label, value == null ? x + w / 2 : x + 36, y + h / 2 + 14 * ls / 40, ls, 0xffffffff, value == null ? 0 : -1);
        if (value != null) toy.text(c, value, x + w - 36, y + h / 2 + 13, 34, 0xffffe46a, 1);
        if (a != null) { hit.add(new RectF(x, y, x + w, y + h + 9)); act.add(a); }
    }

    @Override protected void onDraw(Canvas c) {
        long now = SystemClock.uptimeMillis();
        if (t0 < 0) { t0 = now; lastMs = now; era = Toy.CASTLE; }
        float dt = Math.min(0.05f, (now - lastMs) / 1000f);
        lastMs = now; t = (now - t0) / 1000f;
        wipe = Math.min(1, wipe + dt / 0.8f); hop += dt;
        spin += dt * (st.running() && !st.paused && !menuOpen ? 4.5f : 1.2f);
        hit.clear(); act.clear();
        int w = getWidth(), h = getHeight();
        scale = Math.min(w / W, h / H);
        worldW = w / scale; worldH = h / scale; ox = (worldW - W) / 2; oy = (worldH - H) / 2;
        c.save(); c.scale(scale, scale);
        float gy = oy + GROUND;
        if (wipe < 1 && prevEra >= 0) {
            toy.world(c, prevEra, worldW, worldH, gy, t);
            float wx = Toy.ease(wipe) * (worldW + 120) - 60;
            c.save(); c.clipRect(0, 0, wx, worldH); toy.world(c, era, worldW, worldH, gy, t); c.restore();
            pp.setStyle(Paint.Style.FILL); pp.setColor(Toy.alpha(0xffffffff, 0.8f * (1 - Math.abs(wipe - 0.5f) * 1.2f))); c.drawRect(wx - 5, 0, wx + 5, worldH, pp);
        } else toy.world(c, era, worldW, worldH, gy, t);
        toy.floaters(c, worldW, gy, t);
        c.save(); c.translate(ox, oy);

        toy.wordmark(c, "ROLLERBOT", 150, 110, 104, t);
        // status ribbon
        String sub = !st.running() ? "Waiting for the game" : menuOpen ? "Game menu open" : st.paused ? "Paused" : "Rolling";
        toy.text(c, sub, W - 70, 150, 30, st.paused || menuOpen ? 0xffffe46a : 0xffffffff, 1);

        for (int i = 0; i < TABS.length; i++) {
            final int ti = i;
            float x = 50 + i * 388;
            button(c, TABS[i], null, x, 170, 364, 84, i == tab ? TAB_COL[i] : Toy.shade(TAB_COL[i], -0.2f), i == tab, new Runnable() { @Override public void run() { showTab(ti); } });
        }
        if (tab == 0) drawNow(c); else if (tab == 1) drawLevels(c); else drawScreen(c);

        c.restore();
        float bh = hop < 0.55f ? (float) Math.sin(hop / 0.55f * Math.PI) * 150 : 0;
        toy.ball(c, ox + W - 140, gy + 40 - bh, 56, spin, gy + 62);
        c.restore();
        postInvalidateOnAnimation();
    }

    private void panel(Canvas c, float x, float y, float w, float h, int edge) {
        pp.setShader(null); pp.setStyle(Paint.Style.FILL);
        pp.setColor(Toy.alpha(0xff000000, 0.25f)); r.set(x + 5, y + 12, x + w + 5, y + h + 12); c.drawRoundRect(r, 40, 40, pp);
        pp.setColor(0xfb152a5c); r.set(x, y, x + w, y + h); c.drawRoundRect(r, 40, 40, pp);
        pp.setStyle(Paint.Style.STROKE); pp.setStrokeWidth(8); pp.setColor(edge); c.drawRoundRect(r, 40, 40, pp);
        pp.setStyle(Paint.Style.FILL);
    }

    private void chip(Canvas c, String cap, String val, float x, float y, float w, int col) {
        r.set(x, y, x + w, y + 128);
        toy.block(c, r, 26, 8, Toy.shade(col, -0.2f), false);
        toy.text(c, cap, x + w / 2, y + 38, 24, Toy.alpha(0xffffffff, 0.8f), 0);
        toy.text(c, val, x + w / 2, y + 100, 54, 0xffffffff, 0);
    }

    private void drawNow(Canvas c) {
        panel(c, 50, 280, 1140, 620, TAB_COL[0]);
        String head = st.running() ? (st.level > 1 ? "Level " + st.level : "Title screen") : "Game not running";
        toy.text(c, head, 90, 360, 60, 0xffffffff, -1);
        float cw = 255;
        chip(c, "LIVES", String.valueOf(st.lives), 90, 400, cw, 0xffff4b3e);
        chip(c, "FRUIT", String.valueOf(st.fruit), 90 + (cw + 20), 400, cw, 0xffff9a1f);
        chip(c, "POINTS", String.valueOf(st.points), 90 + 2 * (cw + 20), 400, cw, 0xff1fc4b4);
        chip(c, "HIGH SCORE", String.valueOf(hiScore), 90 + 3 * (cw + 20) - 0, 400, cw - 25, 0xffffd81f);
        // lives as balls
        for (int i = 0; i < Math.min(9, Math.max(0, st.lives)); i++) toy.ball(c, 120 + i * 62, 598, 24, spin + i, -100);
        final boolean paused = st.paused || menuOpen;
        button(c, paused ? "Resume" : "Pause", null, 90, 660, 520, 100, paused ? 0xffff9a1f : 0xff3cc84a, false, new Runnable() { @Override public void run() { host.pause(!st.paused); } });
        button(c, "Restart level", null, 630, 660, 520, 100, 0xff2e4fa6, false, new Runnable() { @Override public void run() { host.restart(); } });
        toy.text(c, "Start on the pad opens the game menu too.", 90, 830, 28, 0xffd6e2ff, -1);
    }

    private void drawLevels(Canvas c) {
        panel(c, 50, 280, 1140, 620, TAB_COL[1]);
        int reached = Math.max(Math.max(1, host.reached()), st.reached);
        toy.text(c, "Pick a level", 90, 360, 56, 0xffffffff, -1);
        toy.text(c, "Levels unlock as you play", 1150, 356, 28, 0xffd6e2ff, 1);
        int cols = 5;
        float cw = 212, ch = 150;
        for (int i = 0; i < st.levels.length; i++) {
            final int n = st.levels[i];
            boolean open = n <= reached, cur = n == st.level;
            float x = 90 + (i % cols) * (cw + 5), y = 400 + (i / cols) * ch;
            int col = Toy.TOYS[(n - 1) % Toy.TOYS.length];
            float cx = x + cw / 2, cy = y + 56;
            toy.sphere(c, cx, cy, 50, open ? col : 0xff44507a, y + 118);
            if (cur) { pp.setStyle(Paint.Style.STROKE); pp.setStrokeWidth(6); pp.setColor(0xffffffff); c.drawCircle(cx, cy, 62, pp); pp.setStyle(Paint.Style.FILL); }
            toy.text(c, n == 1 ? "Title" : String.valueOf(n), cx, cy + 16, n == 1 ? 30 : 42, open ? 0xffffffff : 0xff9aa6cc, 0);
            if (open) { hit.add(new RectF(x, y - 10, x + cw, y + 130)); act.add(new Runnable() { @Override public void run() { hop = 0; host.level(n); } }); }
        }
        if (st.levels.length == 0) toy.text(c, "Start the game to list its levels.", 90, 450, 34, 0xffd6e2ff, -1);
    }

    /** Settings: the launcher's Display, Sound and Language items under the same names. */
    private void drawScreen(Canvas c) {
        panel(c, 50, 280, 1140, 620, TAB_COL[2]);
        header(c, "Display", 90, 340);
        toy.text(c, "Widescreen", 90, 400, 34, 0xffffffff, -1);
        for (int i = 0; i < WS.length; i++) {
            final int m = i;
            boolean on = i == Math.floorMod(st.widescreen, 4);
            button(c, WS[i], null, 90 + i * 272, 420, 258, 76, on ? 0xff2f8df0 : 0xff2e4fa6, on, new Runnable() { @Override public void run() { host.widescreen(m); } });
        }
        header(c, "Sound", 90, 560);
        toy.text(c, "Volume", 90, 620, 34, 0xffffffff, -1);
        button(c, "-", null, 300, 570, 120, 76, 0xff2e4fa6, false, new Runnable() { @Override public void run() { host.volume(Math.max(0, st.volume - 10)); } });
        toy.text(c, st.volume + "%", 595, 622, 44, 0xffffe46a, 0);
        button(c, "+", null, 770, 570, 120, 76, 0xff2e4fa6, false, new Runnable() { @Override public void run() { host.volume(Math.min(100, st.volume + 10)); } });
        header(c, "Language", 90, 710);
        toy.text(c, "Game language", 90, 780, 30, 0xffffffff, -1);
        String[] lb = Toy.LANG_LABELS;
        for (int i = 0; i < 5; i++) {
            final int li = i;
            boolean has = (st.langMask & (1 << i)) != 0, on = i == st.lang;
            button(c, lb[i], null, 440 + i * 146, 730, 135, 76, has ? (on ? 0xff2f8df0 : 0xff2e4fa6) : 0xff39466e, on, has ? new Runnable() { @Override public void run() { host.language(li); } } : null);
        }
    }

    private void header(Canvas c, String s, float x, float y) {
        toy.text(c, s.toUpperCase(), x, y, 24, TAB_COL[2], -1);
        float w = toy.measure(s.toUpperCase(), 24);
        pp.setStyle(Paint.Style.FILL); pp.setColor(Toy.alpha(TAB_COL[2], 0.5f)); c.drawRect(x + 16 + w, y - 11, 1150, y - 8, pp);
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
            float x = e.getX() / scale - ox, y = e.getY() / scale - oy;
            boolean used = false;
            for (int i = 0; i < hit.size(); i++) if (hit.get(i).contains(x, y)) { act.get(i).run(); used = true; break; }
            if (!used) hop = 0;
            invalidate();
        }
        return true;
    }
}
