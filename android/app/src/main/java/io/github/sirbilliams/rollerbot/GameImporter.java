package io.github.sirbilliams.rollerbot;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Copies the user's own game files into app storage (files/game): from a .zip, or from a folder picked with the
 * system file picker (e.g. the installed game, or the CodeAndData folder from the disc). The game root is the
 * folder containing Rollerbot.exe. InstallShield disc extraction (.cab) is a later step (needs unshield).
 */
final class GameImporter {
    static final String MARKER = "rollerbot.exe";

    interface Progress { void onFile(String name); }

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

    /** Returns null on success, else a message for the player. */
    static String importZip(Context c, Uri zip, Progress p) {
        File dest = gameDir(c);
        try (InputStream in = c.getContentResolver().openInputStream(zip);
             ZipInputStream z = new ZipInputStream(in)) {
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
                if (p != null) p.onFile(e.getName());
            }
        } catch (IOException | RuntimeException ex) {
            return "Import failed: " + ex.getMessage();
        }
        return findGameRoot(c) == null ? "Imported, but Rollerbot.exe was not found in it." : null;
    }

    /** Copies a folder picked with ACTION_OPEN_DOCUMENT_TREE. */
    static String importTree(Context c, Uri tree, Progress p) {
        ContentResolver cr = c.getContentResolver();
        try {
            String docId = DocumentsContract.getTreeDocumentId(tree);
            copyDir(cr, tree, docId, gameDir(c), p);
        } catch (IOException | RuntimeException ex) {
            return "Import failed: " + ex.getMessage();
        }
        return findGameRoot(c) == null ? "Imported, but Rollerbot.exe was not found in it." : null;
    }

    private static void copyDir(ContentResolver cr, Uri tree, String docId, File dest, Progress p) throws IOException {
        dest.mkdirs();
        Uri kids = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] cols = { DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                          DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                          DocumentsContract.Document.COLUMN_MIME_TYPE };
        try (Cursor cur = cr.query(kids, cols, null, null, null)) {
            if (cur == null) return;
            byte[] buf = new byte[1 << 16];
            while (cur.moveToNext()) {
                String id = cur.getString(0), name = cur.getString(1).replace('/', '_');
                File out = new File(dest, name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(cur.getString(2))) {
                    copyDir(cr, tree, id, out, p);
                } else {
                    try (InputStream in = cr.openInputStream(DocumentsContract.buildDocumentUriUsingTree(tree, id));
                         OutputStream o = new FileOutputStream(out)) {
                        int n;
                        while ((n = in.read(buf)) > 0) o.write(buf, 0, n);
                    }
                    if (p != null) p.onFile(name);
                }
            }
        }
    }

    static void deleteGame(Context c) { delete(gameDir(c)); }

    private static void delete(File f) {
        File[] k = f.listFiles();
        if (k != null) for (File x : k) delete(x);
        f.delete();
    }
}
