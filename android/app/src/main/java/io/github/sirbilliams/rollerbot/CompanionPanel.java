package io.github.sirbilliams.rollerbot;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Second-screen companion for the AYN Thor's bottom screen: a Presentation on the built-in secondary display
 * (BottomScreen picks it; casting displays are never used). Reads the running game through Native (rb::Bridge)
 * and sends back pause, level and widescreen requests. Tabs: Status, Levels, Settings.
 */
final class CompanionPanel {
    private final Activity act;
    private Presentation pres;
    private TabView view;

    CompanionPanel(Activity a) { act = a; }

    boolean isEnabled() { return pres != null; }

    void resume() {
        if (pres != null) return;
        DisplayManager dm = (DisplayManager) act.getSystemService(Context.DISPLAY_SERVICE);
        Display d = BottomScreen.find(act, dm, "resume");
        if (d == null) return;
        try {
            pres = new Presentation(act, d) {
                @Override protected void onCreate(Bundle b) {
                    super.onCreate(b);
                    view = new TabView(act);
                    setContentView(view);
                    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                }
            };
            pres.show();
        } catch (WindowManager.InvalidDisplayException | IllegalStateException e) {
            pres = null;
        }
    }

    void pause() {
        if (view != null) view.stop();
        if (pres != null) { try { pres.dismiss(); } catch (RuntimeException ignored) { } pres = null; }
        view = null;
    }

    /**
     * Hook for bottom-screen touches that arrive at the game window instead of the Presentation
     * (SpongeBob needed a TouchRouter for that on the Thor). The Presentation gets its own touches,
     * so nothing is routed here yet. Returns true when the panel consumed the event.
     */
    boolean routeTouch(MotionEvent e) { return false; }

    static final class TabView extends View {
        static final String[] TABS = { "Status", "Levels", "Settings" };
        static final String[] WS = { "Fill", "Side panels", "Stretch", "Original 4:3" };
        private static final String[] CONTROLS = {
            "D-pad / left stick: move",
            "A: accept / jump     B: back",
            "X: jump     Y: action",
            "Start: pause     Select: menu",
            "Keyboard: arrows, Enter, Space, Esc"
        };
        private final Handler h = new Handler(Looper.getMainLooper());
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final List<RectF> hit = new ArrayList<>();
        private final List<Runnable> act = new ArrayList<>();
        private final Runnable poll = new Runnable() {
            @Override public void run() { refresh(); h.postDelayed(this, 250); }
        };
        private Native.Status st = new Native.Status();
        private int tab;

        TabView(Context c) { super(c); h.post(poll); }

        void stop() { h.removeCallbacks(poll); }

        private void refresh() {
            try { st = Native.Status.parse(Native.status()); } catch (UnsatisfiedLinkError e) { st = new Native.Status(); }
            if (st.level > 0) PortPrefs.setReached(getContext(), st.level);
            invalidate();
        }

        private int th() { return Math.max(80, getHeight() / 10); }

        private void button(Canvas c, String label, float x, float y, float w, float hh, boolean on, Runnable r) {
            RectF rc = new RectF(x, y, x + w, y + hh);
            p.setColor(on ? Color.rgb(242, 177, 52) : Color.rgb(50, 70, 100));
            c.drawRoundRect(rc, 12, 12, p);
            p.setColor(on ? Color.BLACK : Color.WHITE);
            p.setTextAlign(Paint.Align.CENTER);
            c.drawText(label, x + w / 2, y + hh / 2 + p.getTextSize() / 3, p);
            if (r != null) { hit.add(rc); act.add(r); }
        }

        private void line(Canvas c, String s, float x, float y) {
            p.setColor(Color.WHITE); p.setTextAlign(Paint.Align.LEFT); c.drawText(s, x, y, p);
        }

        @Override protected void onDraw(Canvas c) {
            hit.clear(); act.clear();
            int w = getWidth(), hh = getHeight(), t = th();
            c.drawColor(Color.rgb(20, 30, 50));
            p.setTextSize(t * 0.4f);
            for (int i = 0; i < TABS.length; i++) {
                final int ti = i;
                button(c, TABS[i], i * w / TABS.length + 3, 0, w / TABS.length - 6, t, i == tab, new Runnable() {
                    @Override public void run() { tab = ti; invalidate(); } });
            }
            float x = w * 0.06f, y = t * 2.0f, step = t * 0.7f;
            if (tab == 0) {
                p.setTextSize(t * 0.55f);
                line(c, st.level > 0 ? "Level " + st.level : "Game not running", x, y);
                p.setTextSize(t * 0.45f);
                line(c, "Lives  " + st.lives, x, y + step * 1.4f);
                line(c, "Fruit  " + st.fruit, x, y + step * 2.4f);
                line(c, "Points  " + st.points, x, y + step * 3.4f);
                p.setTextSize(t * 0.45f);
                button(c, st.paused ? "Resume" : "Pause", x, hh - t * 2.2f, w - 2 * x, t * 1.4f, st.paused, new Runnable() {
                    @Override public void run() { Native.requestPause(!st.paused); } });
            } else if (tab == 1) {
                int reached = Math.max(1, PortPrefs.reached(getContext())), cols = 5;
                float cw = (w - 2 * x) / cols, ch = t * 1.3f;
                p.setTextSize(t * 0.45f);
                for (int i = 0; i < st.levels.length; i++) {
                    final int n = st.levels[i];
                    boolean open = n <= reached;
                    float bx = x + (i % cols) * cw, by = y - t * 0.4f + (i / cols) * (ch + 8);
                    button(c, String.valueOf(n), bx + 4, by, cw - 8, ch, n == st.level,
                           open ? new Runnable() { @Override public void run() { Native.requestLevel(n); } } : null);
                    if (!open) { p.setColor(Color.argb(150, 20, 30, 50)); c.drawRoundRect(new RectF(bx + 4, by, bx + cw - 4, by + ch), 12, 12, p); }
                }
            } else {
                p.setTextSize(t * 0.45f);
                final int cur = st.widescreen;
                button(c, "Widescreen: " + WS[Math.floorMod(cur, 4)], x, y - t * 0.4f, w - 2 * x, t * 1.3f, false, new Runnable() {
                    @Override public void run() {
                        int next = (cur + 1) % 4;
                        PortPrefs.setWidescreen(getContext(), next);
                        Native.requestWidescreen(next);
                    } });
                p.setTextSize(t * 0.36f);
                for (int i = 0; i < CONTROLS.length; i++) line(c, CONTROLS[i], x, y + t * 1.6f + i * t * 0.55f);
            }
        }

        @Override public boolean onTouchEvent(MotionEvent e) {
            if (e.getAction() == MotionEvent.ACTION_DOWN)
                for (int i = 0; i < hit.size(); i++)
                    if (hit.get(i).contains(e.getX(), e.getY())) { act.get(i).run(); invalidate(); break; }
            return true;
        }
    }
}
