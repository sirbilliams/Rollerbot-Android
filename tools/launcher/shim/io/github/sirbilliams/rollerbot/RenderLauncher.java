package io.github.sirbilliams.rollerbot;
import android.content.Context;
import android.graphics.*;
import android.os.SystemClock;
import java.io.File;
import javax.imageio.ImageIO;
/** Desktop harness: renders the real LauncherView (and the companion) frame by frame on a stepped clock. Made-up data only. */
public final class RenderLauncher {
 static long clock;
 static void frames(android.view.View v, int n, Canvas c) { for (int i = 0; i < n; i++) { clock += 16; SystemClock.testNow = clock; v.drawForTest(c); } }
 static void shot(android.view.View v, String out, int w, int h) throws Exception {
  Bitmap b = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888); clock += 16; SystemClock.testNow = clock; v.drawForTest(new Canvas(b));
  ImageIO.write(b.img, "png", new File(out)); System.out.println(out);
 }
 static LauncherView make(int page) {
  LauncherView.Actions none = new LauncherView.Actions() { public void select(int p, int r) {} public void adjust(int p, int r, int d) {} public void playLevel(int n) {} public void quit() {} };
  LauncherView v = new LauncherView(new Context(), none); v.setTestSize(1920, 1080);
  v.setPlay(true, 5, 14775, 3200, 5, 14, 5);
  v.setLevels(new int[] { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, 5);
  v.setFiles(new boolean[] { true, true, false, false, true }, "3 sets, 1.2 GB");
  LauncherView.Row R = null;
  v.setLines(0, "Play");
  v.setRows(0, new LauncherView.Row[] { new LauncherView.Row("Continue", "Level 5", true, false), new LauncherView.Row("New game", "from the title", true, false), new LauncherView.Row("Play level", "5", true, true), new LauncherView.Row("DE isn't installed", "Game files", true, false) });
  v.setLines(1, "Game files");
  v.setRows(1, new LauncherView.Row[] { new LauncherView.Row("Import from disc folder", "", true, false), new LauncherView.Row("Import from image or .zip", "", true, false), new LauncherView.Row("Remove game files", "", true, false) });
  v.setLines(2, "Settings");
  v.setRows(2, new LauncherView.Row[] { LauncherView.Row.header("Display"), new LauncherView.Row("Widescreen", "Fill", true, true), new LauncherView.Row("Bottom screen", "On", true, true), new LauncherView.Row("Menu layout", "Tabs", true, true),
    LauncherView.Row.header("Controls"), new LauncherView.Row("Jump button", "A and X", true, true), new LauncherView.Row("Controls reference", "", true, false),
    LauncherView.Row.header("Sound"), new LauncherView.Row("Volume", "100%", true, true),
    LauncherView.Row.header("Language"), new LauncherView.Row("Game language", "EN", true, true) });
  v.setLines(3, "About", "Rollerbot: A Time Journey, rebuilt for handhelds and desktop.", "The game belongs to its original makers; this port is an independent fan project with original code.",
    "Bring your own disc: no game files are included. Not affiliated with or endorsed by the rights holders.", "Version 0.55 (October 2026)");
  v.setRows(3, new LauncherView.Row[0]);
  v.showPage(page);
  return v;
 }
 static CompanionView companion(int tab, boolean paused) {
  CompanionView.Host h = new CompanionView.Host() { public void pause(boolean on) {} public void restart() {} public void level(int n) {} public void widescreen(int m) {} public void volume(int p) {} public void language(int i) {} public int reached() { return 5; } };
  CompanionView v = new CompanionView(new Context(), h); v.setTestSize(1240, 1080);
  Native.Status st = Native.Status.parse("5|3|7|3200|" + (paused ? "1" : "0") + "|1|1,3,4,5,6,7,8,9,10,11,12,13,14,15|14775|5|0|80|0|19");
  v.setStatus(st, st.highScore, false);
  v.showTab(tab);
  return v;
 }
 static void check(boolean ok, String what) { if (!ok) throw new AssertionError(what); }
 static void key(LauncherView v, int k) { check(v.onKeyDown(k, new android.view.KeyEvent()), "key " + k); }
 /** Navigation, stick latch, adjust/select/back callbacks and touch mapping on the real view. */
 static void verify() {
  final int[] got = new int[3]; final boolean[] quit = new boolean[1];
  LauncherView.Actions act = new LauncherView.Actions() {
   public void select(int p, int r) { got[0] = p * 10 + r + 100; }
   public void adjust(int p, int r, int d) { got[1] = p * 100 + r * 10 + d; }
   public void playLevel(int n) {}
   public void quit() { quit[0] = true; } };
  LauncherView v = new LauncherView(new Context(), act); v.setTestSize(1920, 1080); v.setLayout(2);
  v.setRows(0, new LauncherView.Row[] { new LauncherView.Row("a", "", true, false), new LauncherView.Row("b", "", false, false), new LauncherView.Row("c", "5", true, true) });
  v.setRows(2, new LauncherView.Row[] { new LauncherView.Row("w", "Fill", true, true) });
  clock = 1000; SystemClock.testNow = clock; v.drawForTest(new Canvas(Bitmap.createBitmap(1920, 1080, Bitmap.Config.ARGB_8888)));
  check(v.page() == 0 && !v.inContent(), "start");
  key(v, android.view.KeyEvent.KEYCODE_DPAD_DOWN); check(v.page() == 1, "down switches page");
  key(v, android.view.KeyEvent.KEYCODE_DPAD_UP); check(v.page() == 0, "up");
  key(v, android.view.KeyEvent.KEYCODE_DPAD_RIGHT); check(v.inContent() && v.row() == 0, "right enters content");
  key(v, android.view.KeyEvent.KEYCODE_DPAD_DOWN); check(v.row() == 2, "disabled row skipped");
  key(v, android.view.KeyEvent.KEYCODE_DPAD_RIGHT); check(got[1] == 2 * 10 + 1, "adjust +1: " + got[1]);
  key(v, android.view.KeyEvent.KEYCODE_BUTTON_A); check(got[0] == 100 + 2, "select row 2: " + got[0]);
  key(v, android.view.KeyEvent.KEYCODE_BUTTON_B); check(!v.inContent() && !quit[0], "B steps back");
  key(v, android.view.KeyEvent.KEYCODE_BUTTON_B); check(quit[0], "B at top quits");
  // stick: one move per push, released under 0.3
  android.view.MotionEvent push = new android.view.MotionEvent(2, 0, 0, 0, 0.9f), mid = new android.view.MotionEvent(2, 0, 0, 0, 0.45f), rel = new android.view.MotionEvent(2, 0, 0, 0, 0.1f);
  v.onGenericMotionEvent(push); v.onGenericMotionEvent(push); check(v.page() == 1, "stick latch: one move");
  v.onGenericMotionEvent(mid); v.onGenericMotionEvent(push); check(v.page() == 1, "no repeat above release level");
  v.onGenericMotionEvent(rel); v.onGenericMotionEvent(push); check(v.page() == 2, "released then pushed");
  // touch on the 'Settings' nav button (stage coordinates, view is exactly 1920x1080)
  v.showPage(0);
  v.onTouchEvent(new android.view.MotionEvent(0, 900, 308 + 2 * 124 + 40, 0, 0)); v.onTouchEvent(new android.view.MotionEvent(1, 900, 308 + 2 * 124 + 40, 0, 0));
  check(v.page() == 2 && v.inContent(), "touch nav -> settings page, content focus");
  // sections with headers: the pad skips them
  v.setRows(2, new LauncherView.Row[] { LauncherView.Row.header("Display"), new LauncherView.Row("a", "1", true, true), new LauncherView.Row("b", "2", true, true), LauncherView.Row.header("Sound"), new LauncherView.Row("c", "3", true, true) });
  v.showPage(1); v.showPage(2);
  key(v, android.view.KeyEvent.KEYCODE_DPAD_RIGHT); check(v.inContent() && v.row() == 1, "header skipped on entry: " + v.row());
  key(v, android.view.KeyEvent.KEYCODE_DPAD_DOWN); key(v, android.view.KeyEvent.KEYCODE_DPAD_DOWN); check(v.row() == 4, "header skipped going down: " + v.row());
  key(v, android.view.KeyEvent.KEYCODE_DPAD_UP); key(v, android.view.KeyEvent.KEYCODE_DPAD_UP); check(v.row() == 1, "header skipped going up: " + v.row());
  // bottom screen: the section buttons switch the top screen
  LauncherBottomView bv = new LauncherBottomView(new Context(), v, act); bv.setTestSize(1240, 1080);
  clock += 100; SystemClock.testNow = clock; bv.drawForTest(new Canvas(Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888)));
  bv.onTouchEvent(new android.view.MotionEvent(0, 200, 190, 0, 0)); check(v.page() == 0, "bottom Play button -> top page 0: " + v.page());
  clock += 100; SystemClock.testNow = clock; bv.drawForTest(new Canvas(Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888)));
  bv.onTouchEvent(new android.view.MotionEvent(0, 1000, 190, 0, 0)); check(v.page() == 3, "bottom About button -> top page 3: " + v.page());
  System.out.println("launcher checks ok");
 }
 public static void main(String[] a) throws Exception {
  verify();
  String out = a[0]; new File(out).mkdirs();
  String[] names2 = { "play", "import", "settings", "about" };
  Canvas scratch2 = new Canvas(Bitmap.createBitmap(1920, 1080, Bitmap.Config.ARGB_8888));
  for (int lay = 1; lay <= 2; lay++) {
   String pre = lay == 1 ? "A_" : "B_";
   for (int p = 0; p < 4; p++) {
    clock = 40000; SystemClock.testNow = clock;
    LauncherView v = make(0); v.setLayout(lay); frames(v, 5, scratch2);
    v.showPage(p); frames(v, 70, scratch2);
    if (lay == 2) { shot(v, out + "/" + pre + names2[p] + "_menu.png", 1920, 1080); v.onKeyDown(android.view.KeyEvent.KEYCODE_DPAD_RIGHT, new android.view.KeyEvent()); frames(v, 70, scratch2); }
    shot(v, out + "/" + pre + names2[p] + ".png", 1920, 1080);
    // the bottom screen for the same section
    LauncherView tv = make(0); tv.setLayout(lay); tv.showPage(p);
    LauncherBottomView bv = new LauncherBottomView(new Context(), tv, new LauncherView.Actions() { public void select(int a, int b) {} public void adjust(int a, int b, int d) {} public void playLevel(int n) {} public void quit() {} });
    bv.setTestSize(1240, 1080); Canvas bc = new Canvas(Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888));
    frames(bv, 70, bc); shot(bv, out + "/bottom-" + pre + names2[p] + ".png", 1240, 1080);
   }
  }
  String[] ctabs = { "now", "levels", "screen" };
  Bitmap cs = Bitmap.createBitmap(1240, 1080, Bitmap.Config.ARGB_8888); Canvas cc = new Canvas(cs);
  for (int i = 0; i < 3; i++) { clock = 40000; SystemClock.testNow = clock; CompanionView v = companion(0, false); frames(v, 3, cc); v.showTab(i); frames(v, 70, cc); shot(v, out + "/bottom-game-" + ctabs[i] + ".png", 1240, 1080); }
  { clock = 40000; SystemClock.testNow = clock; CompanionView v = companion(0, true); frames(v, 80, cc); shot(v, out + "/bottom-game-paused.png", 1240, 1080); }
 }
}
