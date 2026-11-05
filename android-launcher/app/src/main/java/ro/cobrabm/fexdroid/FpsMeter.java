package ro.cobrabm.fexdroid;

/** Presentation rate over real monotonic time, including stalls with no new frames. */
final class FpsMeter {
    private long frames, time;
    private boolean initialized;
    private double fps;

    double sample(long totalFrames, long nowNanos) {
        if (!initialized || totalFrames<frames || nowNanos<=time) {
            initialized=true;
            frames=totalFrames;
            time=nowNanos;
            fps=0;
            return fps;
        }
        long elapsed=nowNanos-time;
        if (elapsed>=500_000_000L) {
            fps=(totalFrames-frames)*1_000_000_000.0/elapsed;
            frames=totalFrames;
            time=nowNanos;
        }
        return fps;
    }

    void reset() { initialized=false; fps=0; }
}
