package android.view;
import android.content.Context;
import android.graphics.Canvas;
/** Desktop shim (tools/launcher): a size and onDraw. */
public class View {
    private int mW, mH;
    public View(Context c) {}
    public void setTestSize(int w, int h) { mW = w; mH = h; }
    public int getWidth() { return mW; }
    public int getHeight() { return mH; }
    public void invalidate() {}
    public void setFocusable(boolean b) {}
    public void setFocusableInTouchMode(boolean b) {}
    public void postInvalidateDelayed(long ms) {}
    public void postInvalidateOnAnimation() {}
    public boolean isInTouchMode() { return false; }
    public boolean onKeyDown(int key, KeyEvent e) { return false; }
    public boolean onGenericMotionEvent(MotionEvent e) { return false; }
    public void setBackgroundColor(int c) {}
    public void setContentDescription(String s) {}
    public boolean onTouchEvent(MotionEvent e) { return false; }
    public boolean performClick() { return true; }
    protected void onDetachedFromWindow() {}
    protected void onDraw(Canvas c) {}
    public void drawForTest(Canvas c) { onDraw(c); }
}
