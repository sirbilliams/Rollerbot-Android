package io.github.sirbilliams.rollerbot;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.view.MotionEvent;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;

/**
 * Hosts the game. SDLActivity loads libSDL2.so and librollerbot.so, then calls SDL_main (src/platform/main.cpp).
 * The launcher's settings become RB_* environment variables; the game folder is RB_DATA.
 */
public class RollerActivity extends SDLActivity {
    CompanionPanel mPanel;
    private BroadcastReceiver mLid;

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "rollerbot" };   // librollerbot also holds the JNI bridge for Native
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            Os.setenv("RB_WIDESCREEN", PortPrefs.WIDESCREEN[PortPrefs.widescreen(this)], true);
            File game = GameImporter.findGameRoot(this);
            if (game != null) Os.setenv("RB_DATA", game.getPath(), true);
            Os.setenv("RB_SAVE", SaveInfo.dir(this).getPath(), true);
            Os.setenv("RB_VOLUME", String.valueOf(PortPrefs.volume(this)), true);
            Os.setenv("RB_PADJUMP", PortPrefs.CONTROLS_ENV[PortPrefs.controls(this)], true);
            int lvl = getIntent().getIntExtra("level", 0);   // from the launcher: Continue / Play level (0 = the title screen)
            if (lvl > 0) Os.setenv("RB_LEVEL", String.format(java.util.Locale.US, "level%03d", lvl), true);
        } catch (ErrnoException e) {
            // the game then looks in its working directory
        }
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mPanel == null) mPanel = new CompanionPanel(this);
        if (PortPrefs.companion(this)) mPanel.resume();
        registerLid();
    }

    @Override
    protected void onPause() {
        unregisterLid();
        if (mPanel != null) mPanel.pause();
        super.onPause();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        if (mPanel != null && mPanel.routeTouch(e)) return true;
        return super.dispatchTouchEvent(e);
    }

    // ---- lid / screen-off suspend hook -------------------------------------------------------
    // Closing a foldable handheld's lid (or the screen turning off) pauses the game at once, without
    // waiting for the activity to stop: the native loop stops updating and mixing and resumes with the
    // screen. Activity pause/resume still goes through SDL as usual.
    private void registerLid() {
        if (mLid != null) return;
        mLid = new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) {
                String a = i.getAction();
                if (Intent.ACTION_SCREEN_OFF.equals(a)) SDLActivity.nativePause();
                else if (Intent.ACTION_USER_PRESENT.equals(a) || Intent.ACTION_SCREEN_ON.equals(a)) SDLActivity.nativeResume();
            }
        };
        IntentFilter f = new IntentFilter(Intent.ACTION_SCREEN_OFF);
        f.addAction(Intent.ACTION_SCREEN_ON);
        f.addAction(Intent.ACTION_USER_PRESENT);
        registerReceiver(mLid, f);
    }

    private void unregisterLid() {
        if (mLid == null) return;
        try { unregisterReceiver(mLid); } catch (IllegalArgumentException ignored) { }
        mLid = null;
    }
}
