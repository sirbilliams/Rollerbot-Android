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
    private static final int REQ_ZIP = 1, REQ_TREE = 2;
    private TextView status;
    private Button play, widescreen, companion;

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
        Button imZip = button("Import game from .zip", v -> pick(REQ_ZIP));
        Button imDir = button("Import game from folder", v -> pick(REQ_TREE));
        widescreen = button("", v -> { PortPrefs.setWidescreen(this, PortPrefs.widescreen(this) + 1); refresh(); });
        companion = button("", v -> { PortPrefs.setCompanion(this, !PortPrefs.companion(this)); refresh(); });
        Button remove = button("Remove imported game", v -> { GameImporter.deleteGame(this); refresh(); });
        TextView legal = text("Bring your own disc: no game files are included. Not affiliated with the rights holders.", 12);
        for (View v : new View[] { title, status, play, imZip, imDir, widescreen, companion, remove, legal }) root.addView(v);
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
        if (req == REQ_ZIP) {
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
            String err = req == REQ_ZIP ? GameImporter.importZip(this, uri, null) : GameImporter.importTree(this, uri, null);
            runOnUiThread(() -> { refresh(); if (err != null) status.setText(err); });
        }).start();
    }

    private void refresh() {
        boolean have = GameImporter.findGameRoot(this) != null;
        status.setText(have ? "Game found." : "No game imported yet.");
        play.setEnabled(have);
        widescreen.setText("Widescreen: " + PortPrefs.WIDESCREEN[PortPrefs.widescreen(this)]);
        companion.setText("Bottom-screen panel: " + (PortPrefs.companion(this) ? "on" : "off"));
    }
}
