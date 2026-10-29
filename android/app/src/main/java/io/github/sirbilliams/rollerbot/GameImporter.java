package io.github.sirbilliams.rollerbot;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Puts the user's own game files into app storage (files/game). Sources:
 *  - the disc (a folder with its InstallShield files data1.hdr/data1.cab/data2.cab, or the Setup folder),
 *  - a CD image (.iso, or a raw .bin; for roller.bin/.cue pick the .bin),
 *  - a .zip or a folder with the game already extracted (the one holding Rollerbot.exe).
 * From the installer the CodeAndData(language) set is extracted (native, src/platform/import, unshield).
 */
final class GameImporter {
    static final String MARKER = "rollerbot.exe";

    /** Native progress callback: stage 0 = reading the image, 1 = extracting; return true to cancel. */
    interface Progress { boolean onProgress(int stage, long done, long total); }

    static boolean nativeOk;
    static {
        try { System.loadLibrary("rbimport"); nativeOk = true; } catch (UnsatisfiedLinkError e) { nativeOk = false; }
    }
    static native boolean isImage(int fd);
    static native String copyCabinets(int fd, String tmpDir, Progress p);
    static native String extract(String cabDir, String destDir, String language, Progress p);

    private GameImporter() { }

    static File gameDir(Context c) {
        File base = c.getExternalFilesDir(null);
        if (base == null) base = c.getFilesDir();
        return new File(base, "game");
    }

    /** Set false when the files change (import, remove): the next installed() re-checks every set's language. */
    static volatile boolean verified;

    /** The language of the set whose game root is root, from its files (LangSets.detect), or null. */
    static String detectLang(File root) {
        List<String> names = new ArrayList<>();
        collect(root, 0, names);
        return LangSets.detect(names);
    }
    private static void collect(File d, int depth, List<String> out) {
        File[] k = d.listFiles();
        if (k == null) return;
        for (File f : k) { if (f.isDirectory()) { if (depth < 2) collect(f, depth + 1, out); } else out.add(f.getName()); }
    }

    /**
     * game/<lang>/ must really be <lang>: a set is labelled by what its files say, never by its folder or the settings.
     * A mislabelled set (an old import that took Spanish into game/english) is moved to its real slot; a duplicate of a
     * set that is already right is dropped. One log line lists what is installed.
     */
    private static void repairLabels(Context c) {
        File g = gameDir(c);
        int n = PortPrefs.LANG_DIRS.length;
        String[] real = new String[n];
        File[] roots = new File[n];
        for (int i = 0; i < n; i++) {
            roots[i] = find(langDir(c, i), 0);
            if (roots[i] != null) real[i] = detectLang(roots[i]);
        }
        File tmp = new File(c.getCacheDir(), "relabel");
        deleteTree(tmp);
        boolean moved = false;
        for (int i = 0; i < n; i++) {                       // lift every mislabelled set out first
            if (roots[i] == null || real[i] == null || real[i].equals(LangSets.DIRS[i])) continue;
            File lifted = new File(tmp, String.valueOf(i));
            lifted.getParentFile().mkdirs();
            if (langDir(c, i).renameTo(lifted)) { moved = true; android.util.Log.w("Rollerbot", "language set in game/" + LangSets.DIRS[i] + " is really " + real[i] + ": relabelled"); }
            else real[i] = LangSets.DIRS[i];
        }
        if (moved) {
            for (int i = 0; i < n; i++) {
                File lifted = new File(tmp, String.valueOf(i));
                if (!lifted.exists()) continue;
                File to = langDir(c, LangSets.index(real[i]));
                if (to.exists()) deleteTree(lifted); else { g.mkdirs(); if (!lifted.renameTo(to)) deleteTree(lifted); }
            }
            deleteTree(tmp);
        }
        StringBuilder sb = new StringBuilder("language sets:");
        for (int i = 0; i < n; i++) {
            File r = find(langDir(c, i), 0);
            if (r == null) continue;
            String d = detectLang(r);
            sb.append(' ').append(PortPrefs.LANG_LABELS[i]).append('=').append(LangSets.DIRS[i]).append(d != null ? "(detected)" : "(unverified)");
        }
        android.util.Log.i("Rollerbot", sb.toString());
    }

    /** files/game/<language>/: one whole game per language set the source had (english, french, ...). */
    static File langDir(Context c, int lang) { return new File(gameDir(c), PortPrefs.LANG_DIRS[lang]); }

    /** Which languages are installed (index = PortPrefs.LANG_DIRS). Moves a pre-language install (files/game/...) into its set first. */
    static boolean[] installed(Context c) {
        if (!verified) { migrateLegacy(c); repairLabels(c); verified = true; }
        boolean[] b = new boolean[PortPrefs.LANG_DIRS.length];
        for (int i = 0; i < b.length; i++) b[i] = find(langDir(c, i), 0) != null;
        return b;
    }

    /** The language the game runs in: the chosen one when installed, else the first installed one (never another one silently when it is installed). */
    static int activeLang(Context c) { return LangSets.active(PortPrefs.lang(c), installed(c)); }

    private static void migrateLegacy(Context c) {
        File g = gameDir(c);
        File[] kids = g.listFiles();
        if (kids == null || kids.length == 0) return;
        for (File k : kids) if (k.isDirectory() && (LangSets.index(k.getName()) >= 0 || k.getName().equals(".plain"))) return;   // already the new layout
        File root = find(g, 0);
        if (root == null) return;
        String l = detectLang(root);                          // from the files; folder name / setting only when they say nothing
        if (l == null) l = LangSets.langOfDirName(root.getName());
        int i = l != null ? LangSets.index(l) : PortPrefs.lang(c);
        File tmp = new File(c.getCacheDir(), "legacy-game");
        deleteTree(tmp);
        if (!root.renameTo(tmp)) return;
        deleteTree(g);
        g.mkdirs();
        if (!tmp.renameTo(langDir(c, i))) deleteTree(tmp);
    }

    /** The folder holding Rollerbot.exe for the active language (searched up to 4 levels deep, case-insensitive), or null. */
    static File findRootIn(File dir) { return find(dir, 0); }

    static File findGameRoot(Context c) { return find(langDir(c, activeLang(c)), 0); }

    private static File find(File dir, int depth) {
        File[] list = dir.listFiles();
        if (list == null) return null;
        for (File f : list) if (f.isFile() && f.getName().equalsIgnoreCase(MARKER)) return dir;
        if (depth >= 4) return null;
        for (File f : list) if (f.isDirectory()) { File r = find(f, depth + 1); if (r != null) return r; }
        return null;
    }

    // ---- single file: .zip, or a CD image (.iso / raw .bin) ---------------------------------------------------

    /** Returns null on success, else a message for the player. */
    static String importFile(Context c, Uri uri, int lang, Progress p) {
        verified = false;
        ContentResolver cr = c.getContentResolver();
        String name = displayName(cr, uri).toLowerCase();
        if (name.endsWith(".cue"))
            return "That is the .cue sheet: pick the .bin file next to it (roller.bin) instead.";
        try (ParcelFileDescriptor pfd = cr.openFileDescriptor(uri, "r")) {
            if (pfd == null) return "Couldn't open that file.";
            if (isZip(cr, uri)) return importZip(c, uri, lang);
            if (!nativeOk) return "The disc importer isn't available in this build.";
            if (!isImage(pfd.getFd())) return "That file isn't a zip or a CD image (.iso / .bin).";
            return fromImage(c, pfd.getFd(), lang, p);
        } catch (IOException | RuntimeException e) {
            return "Import failed: " + e.getMessage();
        }
    }

    private static boolean isZip(ContentResolver cr, Uri u) {
        try (InputStream in = cr.openInputStream(u)) {
            return in != null && in.read() == 'P' && in.read() == 'K';
        } catch (IOException e) { return false; }
    }

    private static String fromImage(Context c, int fd, int lang, Progress p) {
        File tmp = new File(c.getCacheDir(), "cabs");
        deleteTree(tmp);
        try {
            String err = copyCabinets(fd, tmp.getPath(), p);
            if (err != null) return err;
            return extractFrom(c, tmp, lang, p);
        } finally { deleteTree(tmp); }
    }

    private static String extractFrom(Context c, File cabDir, int lang, Progress p) {
        deleteTree(gameDir(c));
        // Every language set the disc has, side by side (the cabinet's CodeAndData(<language>) groups); a missing group is skipped.
        String lastErr = null;
        for (int i = 0; i < PortPrefs.LANG_DIRS.length; i++) {
            File dest = langDir(c, i);
            String err = extract(cabDir.getPath(), dest.getPath(), PortPrefs.LANG_DIRS[i], p);
            if (err != null) { deleteTree(dest); lastErr = err; }
        }
        return afterInstall(c, lang, lastErr);
    }

    /** null when the chosen language is installed; else a message naming the languages the source did have. */
    private static String afterInstall(Context c, int lang, String lastErr) {
        boolean[] have = installed(c);
        boolean any = false;
        for (boolean b : have) any |= b;
        if (!any) return lastErr != null ? lastErr : "Imported, but Rollerbot.exe was not found.";
        return have[lang] ? null : LangSets.missingMessage(PortPrefs.LANG_LABELS, lang, have);
    }

    static String importZip(Context c, Uri zip, int lang) {
        File dest = gameDir(c);
        deleteTree(dest);
        File plain = new File(dest, ".plain");          // entries under no CodeAndData(<language>) folder
        try (InputStream in = c.getContentResolver().openInputStream(zip); ZipInputStream z = new ZipInputStream(in)) {
            String root = dest.getCanonicalPath() + File.separator;
            ZipEntry e;
            byte[] buf = new byte[1 << 16];
            while ((e = z.getNextEntry()) != null) {
                String lg = LangSets.langOf(e.getName());
                File out = lg != null ? new File(new File(dest, lg), LangSets.relAfter(e.getName())) : new File(plain, e.getName());
                if (!out.getCanonicalPath().startsWith(root)) continue;   // zip-slip guard
                if (e.isDirectory()) { out.mkdirs(); continue; }
                out.getParentFile().mkdirs();
                try (OutputStream o = new FileOutputStream(out)) {
                    int n;
                    while ((n = z.read(buf)) > 0) o.write(buf, 0, n);
                }
            }
        } catch (IOException | RuntimeException ex) {
            return "Import failed: " + ex.getMessage();
        }
        // Language folders in the zip win; a plain game folder (no CodeAndData(...)) is the chosen language's set.
        boolean[] have = installed(c);
        boolean any = false;
        for (boolean b : have) any |= b;
        if (any) deleteTree(plain);
        else if (find(plain, 0) != null) {
            File target = langDir(c, lang);
            target.getParentFile().mkdirs();
            if (!plain.renameTo(target)) return "Import failed: couldn't place the game files.";
        }
        return afterInstall(c, lang, "Imported, but Rollerbot.exe was not found in it.");
    }

    // ---- folder: the disc, the Setup folder, a folder with an image, or the extracted game -----------------------

    private static final class Doc {
        String id, name; boolean dir;
        Doc(String i, String n, boolean d) { id = i; name = n; dir = d; }
    }

    static String importTree(Context c, Uri tree, int lang, Progress p) {
        verified = false;
        ContentResolver cr = c.getContentResolver();
        try {
            List<Doc> all = new ArrayList<>();
            scan(cr, tree, DocumentsContract.getTreeDocumentId(tree), 0, all);
            boolean hasExe = false, hasHdr = false;
            Doc image = null;
            for (Doc d : all) {
                String n = d.name.toLowerCase();
                if (d.dir) continue;
                if (n.equals(MARKER)) hasExe = true;
                if (n.equals("data1.hdr")) hasHdr = true;
                if (image == null && (n.endsWith(".iso") || n.endsWith(".bin") || n.endsWith(".img"))) image = d;
            }
            if (hasExe) {                                     // an already-extracted game (or the disc's file tree)
                boolean sets = false;
                for (Doc d : all) if (d.dir && LangSets.langOfDirName(d.name) != null) sets = true;
                deleteTree(gameDir(c));
                String rootId = DocumentsContract.getTreeDocumentId(tree);
                if (sets) copyDir(cr, tree, rootId, gameDir(c), gameDir(c), true, null);     // each CodeAndData(<language>) to game/<language>/
                else copyDir(cr, tree, rootId, langDir(c, lang), gameDir(c), false, null);  // a plain game folder: the chosen language's set
                return afterInstall(c, lang, "Imported, but Rollerbot.exe was not found.");
            }
            if (hasHdr) {                                     // the disc / Setup folder: take the cabinet files
                if (!nativeOk) return "The disc importer isn't available in this build.";
                File tmp = new File(c.getCacheDir(), "cabs");
                deleteTree(tmp); tmp.mkdirs();
                try {
                    for (Doc d : all) {
                        String n = d.name.toLowerCase();
                        if (!d.dir && n.matches("data\\d+\\.(hdr|cab)"))
                            copyOne(cr, DocumentsContract.buildDocumentUriUsingTree(tree, d.id), new File(tmp, n));
                    }
                    return extractFrom(c, tmp, lang, p);
                } finally { deleteTree(tmp); }
            }
            if (image != null)
                return importFile(c, DocumentsContract.buildDocumentUriUsingTree(tree, image.id), lang, p);
            return "No game found in that folder (looked for Rollerbot.exe, data1.hdr or a .iso/.bin image).";
        } catch (IOException | RuntimeException ex) {
            return "Import failed: " + ex.getMessage();
        }
    }

    private static void scan(ContentResolver cr, Uri tree, String docId, int depth, List<Doc> out) {
        Uri kids = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] cols = { DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                          DocumentsContract.Document.COLUMN_MIME_TYPE };
        List<Doc> dirs = new ArrayList<>();
        try (Cursor cur = cr.query(kids, cols, null, null, null)) {
            if (cur == null) return;
            while (cur.moveToNext()) {
                boolean dir = DocumentsContract.Document.MIME_TYPE_DIR.equals(cur.getString(2));
                Doc d = new Doc(cur.getString(0), cur.getString(1), dir);
                out.add(d);
                if (dir) dirs.add(d);
            }
        }
        if (depth < 3) for (Doc d : dirs) scan(cr, tree, d.id, depth + 1, out);
    }

    private static void copyOne(ContentResolver cr, Uri src, File dest) throws IOException {
        byte[] buf = new byte[1 << 16];
        dest.getParentFile().mkdirs();
        try (InputStream in = cr.openInputStream(src); OutputStream o = new FileOutputStream(dest)) {
            int n;
            while ((n = in.read(buf)) > 0) o.write(buf, 0, n);
        }
    }

    /**
     * Copies a folder. With sets = true only what is under a CodeAndData(<language>) folder is copied, to game/<language>/;
     * lang is the set being copied (null above the language folders). Otherwise everything goes to dest.
     */
    private static void copyDir(ContentResolver cr, Uri tree, String docId, File dest, File base, boolean sets, String lang) throws IOException {
        if (!sets || lang != null) dest.mkdirs();
        Uri kids = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] cols = { DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                          DocumentsContract.Document.COLUMN_MIME_TYPE };
        try (Cursor cur = cr.query(kids, cols, null, null, null)) {
            if (cur == null) return;
            while (cur.moveToNext()) {
                String id = cur.getString(0), name = cur.getString(1).replace('/', '_');
                File out = new File(dest, name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(cur.getString(2))) {
                    String l = sets && lang == null ? LangSets.langOfDirName(name) : null;
                    if (l != null) copyDir(cr, tree, id, new File(base, l), base, true, l);
                    else copyDir(cr, tree, id, out, base, sets, lang);
                } else if (sets && lang == null) continue;
                else copyOne(cr, DocumentsContract.buildDocumentUriUsingTree(tree, id), out);
            }
        }
    }

    private static String displayName(ContentResolver cr, Uri u) {
        try (Cursor cur = cr.query(u, new String[] { "_display_name" }, null, null, null)) {
            if (cur != null && cur.moveToFirst() && cur.getString(0) != null) return cur.getString(0);
        } catch (RuntimeException ignored) { }
        return u.getLastPathSegment() == null ? "" : u.getLastPathSegment();
    }

    static void deleteGame(Context c) { deleteTree(gameDir(c)); verified = false; }

    static void deleteTree(File f) {
        File[] k = f.listFiles();
        if (k != null) for (File x : k) deleteTree(x);
        f.delete();
    }
}
