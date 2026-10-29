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

    /** The folder holding Rollerbot.exe (searched up to 4 levels deep, case-insensitive), or null. */
    static File findGameRoot(Context c) { return find(gameDir(c), 0); }

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
        ContentResolver cr = c.getContentResolver();
        String name = displayName(cr, uri).toLowerCase();
        if (name.endsWith(".cue"))
            return "That is the .cue sheet: pick the .bin file next to it (roller.bin) instead.";
        try (ParcelFileDescriptor pfd = cr.openFileDescriptor(uri, "r")) {
            if (pfd == null) return "Couldn't open that file.";
            if (isZip(cr, uri)) return importZip(c, uri);
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
        File dest = gameDir(c);
        deleteTree(dest);
        String err = extract(cabDir.getPath(), dest.getPath(), PortPrefs.LANG_DIRS[lang], p);
        if (err != null) { deleteTree(dest); return err; }
        return findGameRoot(c) == null ? "Imported, but Rollerbot.exe was not found." : null;
    }

    static String importZip(Context c, Uri zip) {
        File dest = gameDir(c);
        try (InputStream in = c.getContentResolver().openInputStream(zip); ZipInputStream z = new ZipInputStream(in)) {
            String root = dest.getCanonicalPath() + File.separator;
            ZipEntry e;
            byte[] buf = new byte[1 << 16];
            while ((e = z.getNextEntry()) != null) {
                File out = new File(dest, e.getName());
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
        return findGameRoot(c) == null ? "Imported, but Rollerbot.exe was not found in it." : null;
    }

    // ---- folder: the disc, the Setup folder, a folder with an image, or the extracted game -----------------------

    private static final class Doc {
        String id, name; boolean dir;
        Doc(String i, String n, boolean d) { id = i; name = n; dir = d; }
    }

    static String importTree(Context c, Uri tree, int lang, Progress p) {
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
            if (hasExe) {                                     // an already-extracted game: copy it as it is
                deleteTree(gameDir(c));
                copyDir(cr, tree, DocumentsContract.getTreeDocumentId(tree), gameDir(c));
                return findGameRoot(c) == null ? "Imported, but Rollerbot.exe was not found." : null;
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

    private static void copyDir(ContentResolver cr, Uri tree, String docId, File dest) throws IOException {
        dest.mkdirs();
        Uri kids = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] cols = { DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                          DocumentsContract.Document.COLUMN_MIME_TYPE };
        try (Cursor cur = cr.query(kids, cols, null, null, null)) {
            if (cur == null) return;
            while (cur.moveToNext()) {
                String id = cur.getString(0), name = cur.getString(1).replace('/', '_');
                File out = new File(dest, name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(cur.getString(2))) copyDir(cr, tree, id, out);
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

    static void deleteGame(Context c) { deleteTree(gameDir(c)); }

    static void deleteTree(File f) {
        File[] k = f.listFiles();
        if (k != null) for (File x : k) deleteTree(x);
        f.delete();
    }
}
