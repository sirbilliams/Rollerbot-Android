package io.github.sirbilliams.rollerbot;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.view.Display;
import android.view.WindowManager;

/**
 * Shows LauncherBottomView on the Thor's built-in bottom screen while the launcher is in front (same display picking as the
 * in-game CompanionPanel, so casting displays are never used). Nothing happens when there is no such screen. When the game
 * starts the launcher pauses, this goes away and RollerActivity's companion takes over.
 */
final class LauncherPanel {
    private final Activity act;
    private final LauncherView top;
    private final LauncherView.Actions actions;
    private Presentation pres;

    LauncherPanel(Activity a, LauncherView topView, LauncherView.Actions act2) { act = a; top = topView; actions = act2; }

    void resume() {
        if (pres != null) return;
        DisplayManager dm = (DisplayManager) act.getSystemService(Context.DISPLAY_SERVICE);
        Display d = BottomScreen.find(act, dm, "launcher");
        if (d == null) return;
        try {
            pres = new Presentation(act, d) {
                @Override protected void onCreate(Bundle b) {
                    super.onCreate(b);
                    setContentView(new LauncherBottomView(act, top, actions));
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

    /** The Bottom screen setting may have changed. */
    void refresh() { if (PortPrefs.companion(act)) resume(); else pause(); }
}
