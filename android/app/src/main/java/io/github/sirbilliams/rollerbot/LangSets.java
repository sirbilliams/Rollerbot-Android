package io.github.sirbilliams.rollerbot;

import java.util.Locale;

/**
 * Which language set a source path belongs to. The disc (and an installed game) keeps one folder per language,
 * "CodeAndData(english)", "CodeAndData(french)" ...; each holds a whole game (Rollerbot.exe + data). Pure Java, so it is
 * unit-tested on the JVM (android/app/src/test, tests/lang/LangSetsTest.java): a source is never read "first folder wins".
 */
final class LangSets {
    static final String[] DIRS = { "english", "french", "german", "spanish", "italian" };

    private LangSets() { }

    /** "CodeAndData(French)" -> "french" (any case, one of DIRS); anything else -> null. */
    static String langOfDirName(String name) {
        if (name == null) return null;
        String n = name.toLowerCase(Locale.ROOT);
        if (!n.startsWith("codeanddata(") || !n.endsWith(")")) return null;
        String l = n.substring(12, n.length() - 1);
        for (String d : DIRS) if (d.equals(l)) return l;
        return null;
    }

    /** Language of a source path ("Disc/CodeAndData(german)/x/y.dat" -> "german"), or null when it is under none. */
    static String langOf(String path) {
        for (String seg : path.replace('\\', '/').split("/")) {
            String l = langOfDirName(seg);
            if (l != null) return l;
        }
        return null;
    }

    /** The path below the CodeAndData(...) folder ("x/y.dat"), or the whole path when there is none. */
    static String relAfter(String path) {
        String p = path.replace('\\', '/');
        String[] segs = p.split("/");
        StringBuilder sb = new StringBuilder();
        boolean after = false;
        for (String s : segs) {
            if (after) { if (sb.length() > 0) sb.append('/'); sb.append(s); }
            else if (langOfDirName(s) != null) after = true;
        }
        return after ? sb.toString() : p;
    }

    /**
     * The language of a set from its FILES, never from folder names or settings: the retail sets differ in their loading
     * screens (english load1.pcx..load6.pcx, french load1f.pcx.., german ..g, spanish ..s, italian ..i). Null when no
     * loading screen is found. Names are matched case-insensitively, any folder prefix is ignored.
     */
    static String detect(Iterable<String> fileNames) {
        boolean plain = false;
        String found = null;
        for (String raw : fileNames) {
            String n = raw.replace('\\', '/');
            n = n.substring(n.lastIndexOf('/') + 1).toLowerCase(Locale.ROOT);
            if (n.matches("load[1-9]\\.pcx")) plain = true;
            else if (n.matches("load[1-9][fgsi]\\.pcx") && found == null) {
                char c = n.charAt(5);
                found = c == 'f' ? "french" : c == 'g' ? "german" : c == 's' ? "spanish" : "italian";
            }
        }
        return found != null ? found : plain ? "english" : null;
    }

    /** Index into DIRS, or -1. */
    static int index(String lang) { for (int i = 0; i < DIRS.length; i++) if (DIRS[i].equals(lang)) return i; return -1; }

    /**
     * Which of the installed languages (bit i = DIRS[i] installed) is active: the chosen one when installed, else the first
     * installed one (english first), else the chosen one (nothing installed).
     */
    static int active(int chosen, boolean[] installed) {
        if (chosen >= 0 && chosen < installed.length && installed[chosen]) return chosen;
        for (int i = 0; i < installed.length; i++) if (installed[i]) return i;
        return chosen;
    }

    /** Message when the chosen language is not in a source that has other sets, e.g. "English isn't in this source; it has FR, DE." */
    static String missingMessage(String[] labels, int chosen, boolean[] present) {
        StringBuilder have = new StringBuilder();
        for (int i = 0; i < present.length; i++) if (present[i]) { if (have.length() > 0) have.append(", "); have.append(labels[i]); }
        return labels[chosen] + " isn't in that source. It has: " + (have.length() > 0 ? have : "no language sets") + ". Installed those; pick one under Settings > Game language.";
    }
}
