package io.github.sirbilliams.rollerbot;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Launcher: import the game from your own copy (zip or folder), settings, Play. Drawn in code. */
public class SetupActivity extends Activity {
    private static final int REQ_FILE = 1, REQ_TREE = 2;
    private TextView status;
    private Button play, widescreen, companion, lang;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundColor(Color.rgb(20, 30, 50));
        root.setPadding(48, 24, 48, 24);

        TextView title = text("Rollerbot: A Time Journey", 28);
        status = text("", 16);
        play = button("Play", v -> startActivity(new Intent(this, RollerActivity.class)));
        lang = button("", v -> { PortPrefs.setLang(this, PortPrefs.lang(this) + 1); refresh(); });
        Button imDir = button("Import from disc / folder", v -> pick(REQ_TREE));
        Button imZip = button("Import from image (.iso/.bin) or .zip", v -> pick(REQ_FILE));
        widescreen = button("", v -> { PortPrefs.setWidescreen(this, PortPrefs.widescreen(this) + 1); refresh(); });
        companion = button("", v -> { PortPrefs.setCompanion(this, !PortPrefs.companion(this)); refresh(); });
        Button remove = button("Remove imported game", v -> { GameImporter.deleteGame(this); refresh(); });
        TextView legal = text("Bring your own disc: no game files are included. Not affiliated with the rights holders.", 12);
        for (View v : new View[] { title, status, play, lang, imDir, imZip, widescreen, companion, remove, legal }) root.addView(v);
        setContentView(root);
        refresh();
    }

    private TextView text(String s, int sp) {
        TextView t = new TextView(this);
        t.setText(s); t.setTextSize(sp); t.setTextColor(Color.WHITE); t.setGravity(Gravity.CENTER);
        t.setPadding(0, 8, 0, 8);
        return t;
    }

    private Button button(String s, View.OnClickListener l) {
        Button x = new Button(this);
        x.setText(s); x.setOnClickListener(l);
        return x;
    }

    private void pick(int req) {
        Intent i;
        if (req == REQ_FILE) {
            i = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
        } else {
            i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        }
        startActivityForResult(i, req);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        if (res != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        status.setText("Importing...");
        new Thread(() -> {
            GameImporter.Progress prog = (stage, done, total) -> {
                runOnUiThread(() -> status.setText((stage == 0 ? "Reading disc image: " : "Installing: ") + (total > 0 ? 100 * done / total : 0) + "%"));
                return false;
            };
            int lg = PortPrefs.lang(this);
            String err = req == REQ_FILE ? GameImporter.importFile(this, uri, lg, prog) : GameImporter.importTree(this, uri, lg, prog);
            runOnUiThread(() -> { refresh(); if (err != null) status.setText(err); });
        }).start();
    }

    private void refresh() {
        boolean have = GameImporter.findGameRoot(this) != null;
        status.setText(have ? "Game found." : "No game imported yet.");
        play.setEnabled(have);
        lang.setText("Game language to install: " + PortPrefs.LANG_LABELS[PortPrefs.lang(this)]);
        widescreen.setText("Widescreen: " + PortPrefs.WIDESCREEN[PortPrefs.widescreen(this)]);
        companion.setText("Bottom-screen panel: " + (PortPrefs.companion(this) ? "on" : "off"));
    }
}
