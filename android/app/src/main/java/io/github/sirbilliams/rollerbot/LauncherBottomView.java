package io.github.sirbilliams.rollerbot;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * The launcher's second half on the Thor's bottom screen (1240 x 1080 stage, touch only, same toy / era style as the top):
 * four big section buttons that switch the top screen too, and the current section as big touch controls: the level grid and a
 * Continue button, the game files status and import buttons, the settings as toggles, the about text. It reads everything from
 * the top LauncherView (its model) every frame, so both screens stay in step; pad focus stays on the top screen.
 */
final class LauncherBottomView extends View {
    private static final float W = 1240, H = 1080, GROUND = 930;
    private static final int[] PAGE_COL = { 0xff3cc84a, 0xffff9a1f, 0xff2f8df0, 0xff8b5cf0 };

    private final Toy toy = new Toy();
    private final LauncherView top;
    private final LauncherView.Actions actions;
    private final List<RectF> hit = new ArrayList<>();
    private final List<Runnable> act = new ArrayList<>();
    private final RectF r = new RectF(), r2 = new RectF();
    private final Paint pp = new Paint(Paint.ANTI_ALIAS_FLAG);
    private int era = -1, prevEra = -1;
    private long t0 = -1, lastMs;
    private float t, wipe = 1, hop = 9, spin;
    private float scale = 1, ox, oy, worldW = W, worldH = H;

    LauncherBottomView(Context c, LauncherView topView, LauncherView.Actions a) {
        super(c);
        top = topView;
        actions = a;
        setContentDescription("Rollerbot launcher, bottom screen");
    }

    private void button(Canvas c, String label, String value, float x, float y, float w, float h, int face, boolean glow, boolean enabled, Runnable a) {
        r.set(x, y, x + w, y + h);
        toy.block(c, r, 26, 9, enabled ? face : 0xff39466e, glow);
        int tc = enabled ? 0xffffffff : 0xff8d9bc4;
        if (value == null) toy.text(c, label, x + w / 2, y + h / 2 + 14, 40, tc, 0);
        else {
            toy.text(c, label, x + 30, y + h / 2 + 11, 32, tc, -1);
            toy.text(c, value, x + w - 30, y + h / 2 + 12, 34, enabled ? 0xffffe46a : tc, 1);
        }
        if (a != null && enabled) { hit.add(new RectF(x, y, x + w, y + h + 9)); act.add(a); }
    }

    @Override protected void onDraw(Canvas c) {
        long now = SystemClock.uptimeMillis();
        if (t0 < 0) { t0 = now; lastMs = now; }
        float dt = Math.min(0.05f, (now - lastMs) / 1000f);
        lastMs = now; t = (now - t0) / 1000f;
        wipe = Math.min(1, wipe + dt / 0.85f); hop += dt; spin += dt * 3f;
        int page = top.page();
        if (era != top.era()) { prevEra = era; era = top.era(); wipe = prevEra < 0 ? 1 : 0; }
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
        toy.floaters(c, worldW, gy, t, true);
        c.save(); c.translate(ox, oy);

        float sz = 92, tw = toy.measure("ROLLERBOT", sz) + sz * 0.09f;
        toy.wordmark(c, "ROLLERBOT", W / 2 - tw / 2, 100, sz, t);
        // the section buttons: they switch the top screen as well
        float bw = 272, g = 17, x0 = (W - (4 * bw + 3 * g)) / 2;
        for (int i = 0; i < 4; i++) {
            final int pi = i;
            button(c, LauncherView.PAGES[i], null, x0 + i * (bw + g), 150, bw, 84, i == page ? PAGE_COL[i] : Toy.shade(PAGE_COL[i], -0.2f), i == page, true,
                    new Runnable() { @Override public void run() { top.selectPage(pi); hop = 0; } });
        }
        panel(c, page);
        if (page == 0) drawPlay(c); else if (page == 1) drawFiles(c); else if (page == 2) drawSettings(c); else drawAbout(c);
        c.restore();
        float bh = hop < 0.55f ? (float) Math.sin(hop / 0.55f * Math.PI) * 150 : 0;
        toy.ball(c, ox + W - 120, gy + 40 - bh, 52, spin, gy + 62);
        c.restore();
        postInvalidateOnAnimation();
    }

    private void panel(Canvas c, int page) {
        float x = 50, y = 262, w = 1140, h = 640;
        pp.setShader(null); pp.setStyle(Paint.Style.FILL);
        pp.setColor(Toy.alpha(0xff000000, 0.25f)); r.set(x + 5, y + 12, x + w + 5, y + h + 12); c.drawRoundRect(r, 40, 40, pp);
        pp.setColor(0xfb152a5c); r.set(x, y, x + w, y + h); c.drawRoundRect(r, 40, 40, pp);
        pp.setStyle(Paint.Style.STROKE); pp.setStrokeWidth(8); pp.setColor(PAGE_COL[page]); c.drawRoundRect(r, 40, 40, pp);
        pp.setStyle(Paint.Style.FILL);
    }

    // ---- Play ---------------------------------------------------------------------------------------------------
    private void drawPlay(Canvas c) {
        boolean has = top.hasGame();
        String[] cap = { "LEVEL", "HIGH SCORE", "LAST SCORE" };
        String[] val = { has ? String.valueOf(Math.max(1, top.level())) : "-", String.valueOf(top.highScore()), String.valueOf(top.lastScore()) };
        int[] cols = { 0xff3cc84a, 0xffffd81f, 0xff1fc4b4 };
        for (int i = 0; i < 3; i++) {
            float x = 84 + i * 360, y = 290;
            r.set(x, y, x + 330, y + 100);
            toy.block(c, r, 24, 7, Toy.shade(cols[i], -0.2f), false);
            toy.text(c, cap[i], x + 165, y + 32, 22, Toy.alpha(0xffffffff, 0.8f), 0);
            toy.text(c, val[i], x + 165, y + 82, 46, cols[i] == 0xffffd81f ? 0xffffe46a : 0xffffffff, 0);
        }
        int[] nums = top.levelNums;
        int reached = Math.max(1, top.reached);
        float tw = 196, th = 86, gx = 25, gap = 16, gy0 = 414;
        for (int i = 0; i < nums.length; i++) {
            final int n = nums[i];
            boolean open = n <= reached || i == 0, cur = n == top.level();
            float x = 80 + (i % 5) * (tw + gx), y = gy0 + (i / 5) * (th + gap);
            int col = Toy.TOYS[(n - 1) % Toy.TOYS.length];
            r.set(x, y, x + tw, y + th);
            toy.block(c, r, 24, 7, open && has ? col : 0xff39466e, cur);
            toy.text(c, String.valueOf(n), x + tw / 2, y + th / 2 + 15, 44, open && has ? 0xffffffff : 0xff7f8db8, 0);
            if (open && has) { hit.add(new RectF(x, y, x + tw, y + th + 7)); act.add(new Runnable() { @Override public void run() { hop = 0; actions.playLevel(n); } }); }
        }
        if (nums.length == 0) toy.text(c, "Import your game to see its levels.", W / 2, gy0 + 60, 32, 0xffd6e2ff, 0);
        // the big Continue button
        LauncherView.Row[] rs = top.rowsOf(0);
        String label = rs.length > 0 ? rs[0].label : "Continue", value = rs.length > 0 ? rs[0].value : "";
        button(c, label, value, 84, 750, 1072, 104, 0xff3cc84a, true, true, new Runnable() { @Override public void run() { hop = 0; actions.select(0, 0); } });
    }

    // ---- Game files ---------------------------------------------------------------------------------------------
    private void drawFiles(Canvas c) {
        float x = 84, y = 290, w = 1072, h = 130;
        r.set(x, y, x + w, y + h);
        pp.setShader(null); pp.setStyle(Paint.Style.FILL); pp.setColor(0xff0e1d48); c.drawRoundRect(r, 28, 28, pp);
        boolean ok = top.hasGame();
        pp.setColor(ok ? 0xff3cc84a : 0xffffd81f); c.drawCircle(x + 40, y + 38, 14, pp);
        toy.text(c, ok ? "Game found" : "No game files yet", x + 70, y + 52, 38, 0xffffffff, -1);
        if (!top.sizeText.isEmpty()) toy.text(c, top.sizeText, x + w - 30, y + 50, 28, 0xffd6e2ff, 1);
        for (int i = 0; i < 5; i++) {
            boolean on = i < top.installed.length && top.installed[i];
            r2.set(x + 30 + i * 96, y + 76, x + 30 + i * 96 + 84, y + 116);
            toy.block(c, r2, 16, 4, on ? Toy.TOYS[(i * 2 + 3) % 8] : 0xff2a3a68, false);
            toy.text(c, Toy.LANG_LABELS[i], r2.centerX(), r2.centerY() + 11, 30, on ? 0xffffffff : 0xff6f7fae, 0);
        }
        LauncherView.Row[] rs = top.rowsOf(1);
        for (int i = 0; i < rs.length; i++) {
            final int ri = i;
            button(c, rs[i].label, null, 84, 450 + i * 108, 1072, 92, i == 2 ? 0xffd9534a : 0xffff9a1f, false, rs[i].enabled, new Runnable() { @Override public void run() { hop = 0; actions.select(1, ri); } });
        }
        if (top.progress() >= 0) {
            float py = 450 + rs.length * 108 + 6;
            r2.set(x, py, x + w, py + 40);
            pp.setColor(0xff0a1438); c.drawRoundRect(r2, 20, 20, pp);
            r2.set(x + 4, py + 4, x + 4 + Math.max(36, (w - 8) * Math.min(1, top.progress())), py + 36);
            pp.setColor(0xffff9a1f); c.drawRoundRect(r2, 16, 16, pp);
            toy.text(c, top.progressLabel(), x + w / 2, py + 30, 24, 0xffffffff, 0);
        }
    }

    // ---- Settings: the same rows as the top card, as big toggles in two columns under their headers ------------------
    private void drawSettings(Canvas c) {
        LauncherView.Row[] rs = top.rowsOf(2);
        float y = 288, tw = 521, th = 74, gap = 10;
        int colN = 0;
        for (int i = 0; i < rs.length; i++) {
            final LauncherView.Row rw = rs[i];
            final int ri = i;
            if (rw.header) {
                if (colN != 0) { y += th + gap; colN = 0; }
                toy.text(c, rw.label.toUpperCase(), 90, y + 28, 24, PAGE_COL[2], -1);
                float lw = toy.measure(rw.label.toUpperCase(), 24);
                pp.setStyle(Paint.Style.FILL); pp.setColor(Toy.alpha(PAGE_COL[2], 0.5f)); c.drawRect(106 + lw, y + 17, 1150, y + 20, pp);
                y += 40;
                continue;
            }
            float x = 84 + colN * (tw + 30);
            button(c, rw.label, rw.value, x, y, tw, th, 0xff2e4fa6, false, rw.enabled, new Runnable() {
                @Override public void run() { hop = 0; if (rw.adjustable) actions.adjust(2, ri, 2); else actions.select(2, ri); } });
            if (++colN == 2) { colN = 0; y += th + gap; }
        }
    }

    // ---- About ---------------------------------------------------------------------------------------------------
    private void drawAbout(Canvas c) {
        toy.text(c, "About", W / 2, 350, 52, 0xffffffff, 0);
        float y = 430;
        for (String raw : top.linesOf(3)) {
            String cur = "";
            for (String wd : raw.split(" ")) {
                String nx = cur.isEmpty() ? wd : cur + " " + wd;
                if (!cur.isEmpty() && toy.measure(nx, 30) > 1000) { toy.text(c, cur, W / 2, y, 30, 0xffd6e2ff, 0); y += 42; cur = wd; } else cur = nx;
            }
            toy.text(c, cur, W / 2, y, 30, 0xffd6e2ff, 0); y += 62;
        }
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
