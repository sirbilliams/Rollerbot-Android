package io.github.sirbilliams.rollerbot;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.MotionEvent;
import android.view.WindowManager;

/**
 * Second-screen companion for the AYN Thor's bottom screen: a Presentation on the built-in secondary display
 * (BottomScreen picks it; casting displays are never used). Reads the running game through Native (rb::Bridge)
 * and sends back pause, level and widescreen requests. Drawn by CompanionView (Now, Levels, Screen).
 */
final class CompanionPanel {
    private final Activity act;
    private Presentation pres;
    private PanelView view;

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
                    view = new PanelView(act);
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

    static final class PanelView extends android.widget.FrameLayout {
        private final Handler h = new Handler(Looper.getMainLooper());
        private final CompanionView cv;
        private final Runnable poll = new Runnable() {
            @Override public void run() { refresh(); h.postDelayed(this, 250); }
        };

        PanelView(final Context c) {
            super(c);
            cv = new CompanionView(c, new CompanionView.Host() {
                @Override public void pause(boolean on) { try { Native.requestPause(on); } catch (UnsatisfiedLinkError e) { /* game not loaded */ } }
                @Override public void restart() { try { Native.requestRestart(); } catch (UnsatisfiedLinkError e) { /* ditto */ } }
                @Override public void level(int n) { try { Native.requestLevel(n); } catch (UnsatisfiedLinkError e) { /* ditto */ } }
                @Override public void widescreen(int m) {
                    PortPrefs.setWidescreen(c, m);
                    try { Native.requestWidescreen(m); } catch (UnsatisfiedLinkError e) { /* ditto */ }
                }
                @Override public int reached() { return PortPrefs.reached(c); }
            });
            addView(cv, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
            h.post(poll);
        }

        void stop() { h.removeCallbacks(poll); }

        private void refresh() {
            Native.Status st;
            try { st = Native.Status.parse(Native.status()); } catch (UnsatisfiedLinkError e) { st = new Native.Status(); }
            if (st.level > 0) PortPrefs.setReached(getContext(), st.level);
            cv.setStatus(st, st.highScore, st.menuOpen);
        }
    }
}
