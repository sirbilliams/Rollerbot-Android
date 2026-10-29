package io.github.sirbilliams.rollerbot;

import android.content.Context;
import android.content.SharedPreferences;

/** Launcher settings shared with the game process (read at start, exported as RB_* environment variables). */
final class PortPrefs {
    static final String SETTINGS = "settings";
    static final String[] WIDESCREEN = { "fill", "panels", "stretch", "off" };   // same order as rb::Widescreen

    private PortPrefs() { }

    static SharedPreferences get(Context c) {
        return c.getSharedPreferences(SETTINGS, Context.MODE_PRIVATE | Context.MODE_MULTI_PROCESS);
    }
    static int widescreen(Context c) { return Math.floorMod(get(c).getInt("widescreen", 0), WIDESCREEN.length); }
    static void setWidescreen(Context c, int m) { get(c).edit().putInt("widescreen", Math.floorMod(m, WIDESCREEN.length)).apply(); }
    /** Master volume in percent (steps of 10), exported as RB_VOLUME. */
    static int volume(Context c) { return Math.max(0, Math.min(100, get(c).getInt("volume", 100))); }
    static void setVolume(Context c, int pct) { get(c).edit().putInt("volume", Math.max(0, Math.min(100, pct))).apply(); }
    /** TEMPORARY, for review: launcher layout 0 classic, 1 = A (tabs + card), 2 = B (button stack). L1 / R1 / L cycles it. */
    static int layout(Context c) { return Math.floorMod(get(c).getInt("layout", 1), 3); }
    static void setLayout(Context c, int l) { get(c).edit().putInt("layout", Math.floorMod(l, 3)).apply(); }
    static final String[] CONTROLS = { "A and X jump", "A jumps", "X jumps" };
    static final String[] CONTROLS_ENV = { "both", "a", "x" };
    static int controls(Context c) { return Math.floorMod(get(c).getInt("controls", 0), CONTROLS.length); }
    static void setControls(Context c, int i) { get(c).edit().putInt("controls", Math.floorMod(i, CONTROLS.length)).apply(); }
    static final String[] LANG_DIRS = { "english", "french", "german", "spanish", "italian" };
    static final String[] LANG_LABELS = { "EN", "FR", "DE", "ES", "IT" };
    static int lang(Context c) { return Math.floorMod(get(c).getInt("lang", 0), LANG_DIRS.length); }
    static void setLang(Context c, int i) { get(c).edit().putInt("lang", Math.floorMod(i, LANG_DIRS.length)).apply(); }
    /** Highest level number reached so far (levels up to it are unlocked in the companion's level select). */
    static int reached(Context c) { return get(c).getInt("reached", 1); }
    static void setReached(Context c, int n) { if (n > reached(c)) get(c).edit().putInt("reached", n).apply(); }
    static boolean companion(Context c) { return get(c).getBoolean("companion", true); }
    static void setCompanion(Context c, boolean on) { get(c).edit().putBoolean("companion", on).apply(); }
}
