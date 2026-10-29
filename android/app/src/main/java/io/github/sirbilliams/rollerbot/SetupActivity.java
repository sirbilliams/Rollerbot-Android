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
    private LauncherPanel panel;
    private int pick = -1;   // index into playable() chosen with Left / Right on the Play page

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        view = new LauncherView(this, this);
        view.setLayout(PortPrefs.layout(this) + 1);
        setContentView(view);
        view.requestFocus();
        refresh();
    }

    @Override public void onWindowFocusChanged(boolean f) { super.onWindowFocusChanged(f); if (f) view.requestFocus(); }

    @Override
    protected void onResume() {
        super.onResume();
        int gl = LangSets.index(SaveInfo.read(SaveInfo.dir(this)).lang);   // a language chosen in the game's menu carries over
        if (gl >= 0 && GameImporter.installed(this)[gl]) PortPrefs.setLang(this, gl);
        if (panel == null) panel = new LauncherPanel(this, view, this);
        if (PortPrefs.companion(this)) panel.resume(); else panel.pause();
        refresh();
    }

    @Override
    protected void onPause() {
        if (panel != null) panel.pause();
        super.onPause();
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
        boolean[] inst = GameImporter.installed(this);
        int active = GameImporter.activeLang(this);
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

        // ---- Play: status tiles are drawn by the view; rows are the three ways to start, plus one notice when the chosen language is missing
        List<LauncherView.Row> pr = new ArrayList<>();
        int want = PortPrefs.lang(this);
        if (have) {
            pr.add(new LauncherView.Row(s.lastLevel > 1 ? "Continue" : "Start game", s.lastLevel > 1 ? "Level " + s.lastLevel : "from the title", true, false));
            pr.add(new LauncherView.Row("New game", "from the title", true, false));
            pr.add(new LauncherView.Row("Play level", String.valueOf(shown), play.length > 0, true));
            if (!inst[want]) pr.add(new LauncherView.Row(PortPrefs.LANG_LABELS[want] + " isn't installed", "Game files", true, false));
        } else {
            pr.add(new LauncherView.Row("No game files yet", "Game files", true, false));
        }
        view.setRows(0, pr.toArray(new LauncherView.Row[0]));
        view.setLines(0, "Play");
        { List<Integer> l = new ArrayList<>(); for (int n : all) if (n > 1) l.add(n); int[] nums = new int[l.size()]; for (int k = 0; k < nums.length; k++) nums[k] = l.get(k); view.setLevels(nums, s.reached); }

        // ---- Game files: a status block (drawn by the view), then the actions
        view.setLines(1, "Game files");
        view.setFiles(inst, have ? sizeText() : "");
        view.setRows(1, new LauncherView.Row[] {
            new LauncherView.Row("Import from disc folder", "", !importing, false),
            new LauncherView.Row("Import from image or .zip", "", !importing, false),
            new LauncherView.Row(confirmRemove ? "Press again to remove" : "Remove game files", "", have && !importing, false) });

        // ---- Settings, grouped
        view.setLines(2, "Settings");
        view.setRows(2, new LauncherView.Row[] {
            LauncherView.Row.header("Display"),
            new LauncherView.Row("Widescreen", WS_NAMES[PortPrefs.widescreen(this)], true, true),
            new LauncherView.Row("Bottom screen", PortPrefs.companion(this) ? "On" : "Off", true, true),
            new LauncherView.Row("Menu layout", PortPrefs.LAYOUTS[PortPrefs.layout(this)], true, true),
            LauncherView.Row.header("Controls"),
            new LauncherView.Row("Jump button", JUMP[PortPrefs.controls(this)], true, true),
            new LauncherView.Row("Controls reference", "", true, false),
            LauncherView.Row.header("Sound"),
            new LauncherView.Row("Volume", PortPrefs.volume(this) + "%", true, true),
            LauncherView.Row.header("Language"),
            new LauncherView.Row("Game language", PortPrefs.LANG_LABELS[active], have && countOf(inst) > 1, true) });

        view.setLines(3, "About", "Rollerbot: A Time Journey, rebuilt for handhelds and desktop.",
                "The game belongs to its original makers; this port is an independent fan project with original code.",
                "Bring your own disc: no game files are included. Not affiliated with or endorsed by the rights holders.", "Version " + version());
        view.setRows(3, new LauncherView.Row[0]);
        if (panel != null) panel.refresh();
    }

    static final String[] JUMP = { "A and X", "A only", "X only" };
    private String sizeCache;
    /** Size of the installed game files, e.g. "3 sets, 1.2 GB". */
    private String sizeText() {
        if (sizeCache == null) {
            long total = 0; int sets = 0;
            boolean[] inst = GameImporter.installed(this);
            for (int i = 0; i < inst.length; i++) if (inst[i]) { sets++; total += dirSize(GameImporter.langDir(this, i), 0); }
            sizeCache = sets + (sets == 1 ? " set, " : " sets, ") + (total >= (1L << 30) ? String.format(java.util.Locale.US, "%.1f GB", total / 1073741824.0) : (total >> 20) + " MB");
        }
        return sizeCache;
    }
    private static long dirSize(File d, int depth) {
        long n = 0; File[] ks = d.listFiles();
        if (ks == null || depth > 6) return 0;
        for (File k : ks) n += k.isDirectory() ? dirSize(k, depth + 1) : k.length();
        return n;
    }

    private static int countOf(boolean[] b) { int n = 0; for (boolean x : b) if (x) n++; return n; }
    private static int indexOf(int[] a, int v) { for (int i = 0; i < a.length; i++) if (a[i] == v) return i; return 0; }
    private String version() {
        try { return getPackageManager().getPackageInfo(getPackageName(), 0).versionName; } catch (Exception e) { return "0.55"; }
    }

    // ---- LauncherView.Actions -------------------------------------------------------------------------------
    @Override public void select(int page, int row) {
        confirmRemove = confirmRemove && page == 1 && row == 2;
        if (page == 0) {
            SaveInfo s = SaveInfo.read(SaveInfo.dir(this));
            int[] play = playable(levels(), s);
            boolean have = GameImporter.findGameRoot(this) != null;
            if (!have || row == 3) { view.selectPage(1); return; }     // the notice rows lead to Game files
            if (row == 0) play(s.lastLevel > 1 ? s.lastLevel : 0);
            else if (row == 1) play(0);
            else if (row == 2 && play.length > 0) play(play[Math.min(pick, play.length - 1)]);
        } else if (page == 1) {
            if (row == 0) pickSource(REQ_TREE);
            else if (row == 1) pickSource(REQ_FILE);
            else if (row == 2) {
                if (confirmRemove) { GameImporter.deleteGame(this); confirmRemove = false; sizeCache = null; } else confirmRemove = true;
                refresh();
            }
        } else if (page == 2 && row == 6) {
            view.showInfo("Controls reference", "Move: D-pad or left stick", "Jump: " + JUMP[PortPrefs.controls(this)] + " (see Jump button)", "Menu: Start (resume, restart, level, settings)",
                    "Back: B", "Keyboard: arrows, Enter or Space, Esc");
        }
    }

    @Override public void playLevel(int n) {
        SaveInfo s = SaveInfo.read(SaveInfo.dir(this));
        if (GameImporter.findGameRoot(this) != null && n > 0 && n <= Math.max(s.reached, 3)) play(n);
    }

    @Override public void adjust(int page, int row, int dir) {
        int step = dir == 2 ? 1 : dir;
        if (page == 0 && row == 2) {
            int[] play = playable(levels(), SaveInfo.read(SaveInfo.dir(this)));
            if (play.length > 0) pick = Math.floorMod(pick + step, play.length);
        } else if (page == 2 && row == 1) PortPrefs.setWidescreen(this, PortPrefs.widescreen(this) + step);
        else if (page == 2 && row == 2) PortPrefs.setCompanion(this, !PortPrefs.companion(this));
        else if (page == 2 && row == 3) { PortPrefs.setLayout(this, PortPrefs.layout(this) + step); view.setLayout(PortPrefs.layout(this) + 1); }
        else if (page == 2 && row == 5) PortPrefs.setControls(this, PortPrefs.controls(this) + step);
        else if (page == 2 && row == 8) PortPrefs.setVolume(this, dir == 2 && PortPrefs.volume(this) >= 100 ? 0 : PortPrefs.volume(this) + 10 * step);
        else if (page == 2 && row == 10) {      // Game language: only the installed sets, no re-import; also the language the next import opens first
            boolean[] inst = GameImporter.installed(this);
            int n = PortPrefs.LANG_DIRS.length, cur = GameImporter.activeLang(this);
            for (int k = 1; k <= n; k++) {
                int i = Math.floorMod(cur + step * k, n);
                if (inst[i]) { PortPrefs.setLang(this, i); break; }
            }
        }
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
                    sizeCache = null;
                    refresh();
                    view.setProgress(err == null ? -1 : 0, err == null ? "" : err);
                } });
            }
        }).start();
    }
}
