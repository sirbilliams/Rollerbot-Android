package android.graphics;
import java.awt.image.BufferedImage;
/** Desktop shim (tools/companion): a BufferedImage. */
public class Bitmap {
    public enum Config { ARGB_8888 }
    public final BufferedImage img;
    private Bitmap(BufferedImage i) { img = i; }
    public static Bitmap createBitmap(int w, int h, Config c) { return new Bitmap(new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB)); }
    public static Bitmap createBitmap(int[] px, int w, int h, Config c) {
        Bitmap b = createBitmap(w, h, c);
        b.img.setRGB(0, 0, w, h, px, 0, w);
        return b;
    }
    public static Bitmap wrap(BufferedImage i) { return new Bitmap(i); }
    public int getWidth() { return img.getWidth(); }
    public int getHeight() { return img.getHeight(); }
    public void recycle() {}
}
