package io.github.sirbilliams.rollerbot;

import android.content.Context;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;

/**
 * Reads what the game keeps in its save folder (src/game/Save.cpp): rollerbot.cfg (progress) and globvars.dat, whose
 * global 100 (byte 400, little endian) is the high score. The folder is passed to the game as RB_SAVE.
 */
final class SaveInfo {
    int reached = 1, lastLevel = 1, lastScore, highScore;

    static File dir(Context c) { return new File(c.getFilesDir(), "save"); }

    static SaveInfo read(File dir) {
        SaveInfo s = new SaveInfo();
        try (BufferedReader r = new BufferedReader(new FileReader(new File(dir, "rollerbot.cfg")))) {
            String l;
            while ((l = r.readLine()) != null) {
                int eq = l.indexOf('=');
                if (eq < 0) continue;
                int v;
                try { v = Integer.parseInt(l.substring(eq + 1).trim()); } catch (NumberFormatException e) { continue; }
                String k = l.substring(0, eq);
                if (k.equals("reached")) s.reached = Math.max(1, v);
                else if (k.equals("last")) s.lastLevel = v;
                else if (k.equals("score")) s.lastScore = v;
            }
        } catch (IOException e) { /* no save yet */ }
        File g = new File(dir, "globvars.dat");
        if (g.length() >= 404) {
            try (FileInputStream in = new FileInputStream(g)) {
                byte[] b = new byte[404];
                int got = 0;
                while (got < 404) { int n = in.read(b, got, 404 - got); if (n < 0) break; got += n; }
                if (got == 404) s.highScore = Math.max(0, (b[400] & 0xff) | (b[401] & 0xff) << 8 | (b[402] & 0xff) << 16 | (b[403] & 0xff) << 24);
            } catch (IOException e) { /* ignore */ }
        }
        return s;
    }
}
