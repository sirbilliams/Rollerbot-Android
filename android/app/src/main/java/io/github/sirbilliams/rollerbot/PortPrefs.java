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
    static boolean companion(Context c) { return get(c).getBoolean("companion", true); }
    static void setCompanion(Context c, boolean on) { get(c).edit().putBoolean("companion", on).apply(); }
}
