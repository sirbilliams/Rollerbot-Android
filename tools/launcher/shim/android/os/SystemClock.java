package android.os;
/** Desktop shim (tools/launcher): a clock the renderer steps frame by frame for repeatable pictures. */
public final class SystemClock {
    public static long testNow = -1;
    public static long uptimeMillis() { return testNow >= 0 ? testNow : System.nanoTime() / 1000000L; }
}
