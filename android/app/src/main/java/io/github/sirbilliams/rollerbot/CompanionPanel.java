package io.github.sirbilliams.rollerbot;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.view.Display;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;

/**
 * Second-screen companion for the AYN Thor's bottom screen: a Presentation on the built-in secondary
 * display (BottomScreen picks it; casting displays are never used). Only placeholder tabs for now;
 * the HUD/inventory/map content arrives with the game lanes.
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
        if (pres != null) { try { pres.dismiss(); } catch (RuntimeException ignored) { } pres = null; }
    }

    /**
     * Hook for bottom-screen touches that arrive at the game window instead of the Presentation
     * (SpongeBob needed a TouchRouter for that on the Thor). Not needed for the empty shell: the
     * Presentation gets its own touches. Returns true when the panel consumed the event.
     */
    boolean routeTouch(MotionEvent e) { return false; }

    /** Placeholder panel: a row of tabs and an empty body. */
    static final class TabView extends View {
        static final String[] TABS = { "Hint", "Items", "Map", "Pad" };
        private int tab;
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);

        TabView(Context c) { super(c); }

        @Override protected void onDraw(Canvas c) {
            int w = getWidth(), h = getHeight(), th = Math.max(80, h / 10);
            c.drawColor(Color.rgb(20, 30, 50));
            p.setTextSize(th * 0.4f);
            p.setTextAlign(Paint.Align.CENTER);
            for (int i = 0; i < TABS.length; i++) {
                p.setColor(i == tab ? Color.rgb(242, 177, 52) : Color.rgb(50, 70, 100));
                c.drawRect(i * w / TABS.length + 3, 0, (i + 1) * w / TABS.length - 3, th, p);
                p.setColor(i == tab ? Color.BLACK : Color.WHITE);
                c.drawText(TABS[i], (i + 0.5f) * w / TABS.length, th * 0.65f, p);
            }
            p.setColor(Color.LTGRAY);
            c.drawText(TABS[tab] + ": coming soon", w / 2f, h / 2f, p);
        }

        @Override public boolean onTouchEvent(MotionEvent e) {
            if (e.getAction() == MotionEvent.ACTION_DOWN && e.getY() < Math.max(80, getHeight() / 10)) {
                tab = Math.min(TABS.length - 1, (int) (e.getX() * TABS.length / getWidth()));
                invalidate();
            }
            return true;
        }
    }
}
