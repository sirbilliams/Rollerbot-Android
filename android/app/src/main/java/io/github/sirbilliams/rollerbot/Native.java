package io.github.sirbilliams.rollerbot;

/** JNI bridge to rb::Bridge (src/platform/BridgeJNI.cpp); only valid in the game process (librollerbot loaded). */
final class Native {
    private Native() { }
    /** "level|lives|fruit|points|paused|widescreen|1,2,3" */
    static native String status();
    static native void requestLevel(int n);
    static native void requestPause(boolean on);
    static native void requestWidescreen(int mode);

    /** Parsed status. */
    static final class Status {
        int level, lives, fruit, points, widescreen;
        boolean paused;
        int[] levels = new int[0];

        static Status parse(String s) {
            Status st = new Status();
            if (s == null) return st;
            String[] f = s.split("\\|", -1);
            if (f.length < 7) return st;
            try {
                st.level = Integer.parseInt(f[0]); st.lives = Integer.parseInt(f[1]); st.fruit = Integer.parseInt(f[2]);
                st.points = Integer.parseInt(f[3]); st.paused = "1".equals(f[4]); st.widescreen = Integer.parseInt(f[5]);
                if (!f[6].isEmpty()) {
                    String[] l = f[6].split(",");
                    st.levels = new int[l.length];
                    for (int i = 0; i < l.length; i++) st.levels[i] = Integer.parseInt(l[i]);
                }
            } catch (NumberFormatException e) { /* keep what parsed */ }
            return st;
        }
    }
}
