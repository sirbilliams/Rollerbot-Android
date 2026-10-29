package io.github.sirbilliams.rollerbot;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Launcher: Play / Continue, Import, Settings, About, drawn by LauncherView (see docs/launcher.md). This class holds the logic:
 * what the pages say, the import flow, the preferences the game reads at start (PortPrefs, exported by RollerActivity).
 */
public class SetupActivity extends Activity implements LauncherView.Actions {
    private static final int REQ_FILE = 1, REQ_TREE = 2;
    static final String[] WS_NAMES = { "Fill", "Side panels", "Stretch", "Original 4:3" };

    private LauncherView view;
    private boolean importing, confirmRemove;
    private int pick = -1;   // index into playable() chosen with Left / Right on the Play page

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        view = new LauncherView(this, this);
        setContentView(view);
        view.requestFocus();
        refresh();
    }

    @Override public void onWindowFocusChanged(boolean f) { super.onWindowFocusChanged(f); if (f) view.requestFocus(); }

    @Override
    protected void onResume() {
        super.onResume();
        refresh();
    }

    // The view takes keys and sticks itself; these make sure they reach it even when focus drifts.
    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        if (e.getAction() == KeyEvent.ACTION_DOWN && view.onKeyDown(e.getKeyCode(), e)) return true;
        return e.getAction() == KeyEvent.ACTION_UP && isHandled(e.getKeyCode()) || super.dispatchKeyEvent(e);
    }
    private static boolean isHandled(int k) {
        return k == KeyEvent.KEYCODE_DPAD_UP || k == KeyEvent.KEYCODE_DPAD_DOWN || k == KeyEvent.KEYCODE_DPAD_LEFT || k == KeyEvent.KEYCODE_DPAD_RIGHT
                || k == KeyEvent.KEYCODE_DPAD_CENTER || k == KeyEvent.KEYCODE_BUTTON_A || k == KeyEvent.KEYCODE_BUTTON_B || k == KeyEvent.KEYCODE_ENTER;
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        if ((e.getSource() & android.view.InputDevice.SOURCE_JOYSTICK) != 0 && e.getActionMasked() == MotionEvent.ACTION_MOVE) return view.onGenericMotionEvent(e);
        return super.dispatchGenericMotionEvent(e);
    }
    @Override public void onBackPressed() { if (!view.back()) finish(); }

    // ---- data -----------------------------------------------------------------------------------------------
    /** Level numbers present in the imported game folder (level001.prj ...). */
    private int[] levels() {
        File root = GameImporter.findGameRoot(this);
        List<Integer> l = new ArrayList<>();
        if (root != null) for (int n = 1; n < 100; n++) if (new File(root, String.format(java.util.Locale.US, "level%03d.prj", n)).exists()) l.add(n);
        int[] a = new int[l.size()];
        for (int i = 0; i < a.length; i++) a[i] = l.get(i);
        return a;
    }
    /** Levels the player can start from the Play page: unlocked ones past the title screen (the first real level is always open). */
    private int[] playable(int[] all, SaveInfo s) {
        List<Integer> l = new ArrayList<>();
        for (int n : all) if (n > 1 && (n <= s.reached || l.isEmpty())) l.add(n);
        int[] a = new int[l.size()];
        for (int i = 0; i < a.length; i++) a[i] = l.get(i);
        return a;
    }

    private void refresh() {
        boolean have = GameImporter.findGameRoot(this) != null;
        SaveInfo s = SaveInfo.read(SaveInfo.dir(this));
        PortPrefs.setReached(this, s.reached);
        int[] all = levels(), play = playable(all, s);
        if (pick < 0 || pick >= play.length) { pick = 0; for (int i = 0; i < play.length; i++) if (play[i] == s.lastLevel) pick = i; }
        int shown = play.length > 0 ? play[pick] : 0;
        int unlocked = 0;
        for (int n : all) if (n > 1 && n <= s.reached) unlocked++;
        int total = 0;
        for (int n : all) if (n > 1) total++;
        view.setPlay(have, s.lastLevel > 1 ? s.lastLevel : 0, s.highScore, s.lastScore, Math.max(unlocked, have ? 1 : 0), total, shown > 1 ? indexOf(all, shown) : 0);

        List<LauncherView.Row> pr = new ArrayList<>();
        if (have) {
            pr.add(new LauncherView.Row(s.lastLevel > 1 ? "Continue" : "Start game", s.lastLevel > 1 ? "Level " + s.lastLevel : "from the title", true, false));
            pr.add(new LauncherView.Row("New game", "from the title", true, false));
            pr.add(new LauncherView.Row("Play level", String.valueOf(shown), play.length > 0, true));
        } else {
            pr.add(new LauncherView.Row("Import your game first", "", false, false));
        }
        view.setRows(0, pr.toArray(new LauncherView.Row[0]));
        view.setLines(0, have ? "Play" : "Welcome", have ? new String[0] : new String[] { "No game imported yet. Choose Import and bring your own disc." });

        view.setLines(1, "Import your game",
                have ? "Game found. Importing again replaces it." : "Pick the folder of your disc, or an image (.iso, .bin) or a .zip.",
                "Language to install: " + PortPrefs.LANG_LABELS[PortPrefs.lang(this)]);
        view.setRows(1, new LauncherView.Row[] {
            new LauncherView.Row("Import from disc folder", "", !importing, false),
            new LauncherView.Row("Import from image or .zip", "", !importing, false),
            new LauncherView.Row("Game language", PortPrefs.LANG_LABELS[PortPrefs.lang(this)], !importing, true),
            new LauncherView.Row(confirmRemove ? "Press again to remove" : "Remove imported game", "", have && !importing, false) });

        view.setLines(2, "Settings", "Move: D-pad or left stick     Jump: " + PortPrefs.CONTROLS[PortPrefs.controls(this)],
                "Start opens the game menu: resume, restart, level, screen.", "Changes apply the next time the game starts.");
        view.setRows(2, new LauncherView.Row[] {
            new LauncherView.Row("Widescreen", WS_NAMES[PortPrefs.widescreen(this)], true, true),
            new LauncherView.Row("Controls", PortPrefs.CONTROLS[PortPrefs.controls(this)], true, true),
            new LauncherView.Row("Bottom screen", PortPrefs.companion(this) ? "On" : "Off", true, true) });

        view.setLines(3, "About", "Rollerbot: A Time Journey, rebuilt for handhelds and desktop.",
                "Bring your own disc: no game files are included.", "Not affiliated with or endorsed by the rights holders.", "Version " + version());
        view.setRows(3, new LauncherView.Row[0]);
    }

    private static int indexOf(int[] a, int v) { for (int i = 0; i < a.length; i++) if (a[i] == v) return i; return 0; }
    private String version() {
        try { return getPackageManager().getPackageInfo(getPackageName(), 0).versionName; } catch (Exception e) { return "0.55"; }
    }

    // ---- LauncherView.Actions -------------------------------------------------------------------------------
    @Override public void select(int page, int row) {
        confirmRemove = confirmRemove && page == 1 && row == 3;
        if (page == 0) {
            SaveInfo s = SaveInfo.read(SaveInfo.dir(this));
            int[] play = playable(levels(), s);
            if (row == 0) play(s.lastLevel > 1 ? s.lastLevel : 0);
            else if (row == 1) play(0);
            else if (row == 2 && play.length > 0) play(play[Math.min(pick, play.length - 1)]);
        } else if (page == 1) {
            if (row == 0) pickSource(REQ_TREE);
            else if (row == 1) pickSource(REQ_FILE);
            else if (row == 3) {
                if (confirmRemove) { GameImporter.deleteGame(this); confirmRemove = false; } else confirmRemove = true;
                refresh();
            }
        }
    }

    @Override public void adjust(int page, int row, int dir) {
        if (page == 0 && row == 2) {
            int[] play = playable(levels(), SaveInfo.read(SaveInfo.dir(this)));
            if (play.length > 0) pick = Math.floorMod(pick + dir, play.length);
        } else if (page == 1 && row == 2) PortPrefs.setLang(this, PortPrefs.lang(this) + dir);
        else if (page == 2 && row == 0) PortPrefs.setWidescreen(this, PortPrefs.widescreen(this) + dir);
        else if (page == 2 && row == 1) PortPrefs.setControls(this, PortPrefs.controls(this) + dir);
        else if (page == 2 && row == 2) PortPrefs.setCompanion(this, !PortPrefs.companion(this));
        refresh();
    }

    @Override public void quit() { finish(); }

    private void play(int level) {
        Intent i = new Intent(this, RollerActivity.class);
        if (level > 0) i.putExtra("level", level);
        startActivity(i);
    }

    // ---- import (unchanged flow) ----------------------------------------------------------------------------
    private void pickSource(int req) {
        Intent i;
        if (req == REQ_FILE) i = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
        else i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        startActivityForResult(i, req);
    }

    @Override
    protected void onActivityResult(final int req, int res, Intent data) {
        if (res != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        importing = true;
        refresh();
        view.setProgress(0, "Starting...");
        new Thread(new Runnable() {
            @Override public void run() {
                GameImporter.Progress prog = new GameImporter.Progress() {
                    @Override public boolean onProgress(final int stage, final long done, final long total) {
                        runOnUiThread(new Runnable() { @Override public void run() {
                            int pct = total > 0 ? (int) (100 * done / total) : 0;
                            view.setProgress(pct / 100f, (stage == 0 ? "Reading disc image: " : "Installing: ") + pct + "%");
                        } });
                        return false;
                    }
                };
                int lg = PortPrefs.lang(SetupActivity.this);
                final String err = req == REQ_FILE ? GameImporter.importFile(SetupActivity.this, uri, lg, prog) : GameImporter.importTree(SetupActivity.this, uri, lg, prog);
                runOnUiThread(new Runnable() { @Override public void run() {
                    importing = false;
                    refresh();
                    view.setProgress(err == null ? -1 : 0, err == null ? "" : err);
                } });
            }
        }).start();
    }
}
