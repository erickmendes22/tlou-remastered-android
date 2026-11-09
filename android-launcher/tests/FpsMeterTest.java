package ro.cobrabm.fexdroid;
public final class FpsMeterTest {
    private static void expect(double actual,double expected) {
        if(Math.abs(actual-expected)>0.0001) throw new AssertionError(actual+" != "+expected);
    }
    public static void main(String[] args) {
        FpsMeter meter=new FpsMeter();
        expect(meter.sample(1000,1_000_000_000L),0);
        expect(meter.sample(1030,1_500_000_000L),60);
        expect(meter.sample(1096,3_700_000_000L),30); // actual elapsed time, delayed UI callback
        expect(meter.sample(1096,4_700_000_000L),0); // stalled renderer
        expect(meter.sample(0,5_200_000_000L),0); // counter restarted
        expect(meter.sample(15,5_700_000_000L),30);
        meter.reset();
        expect(meter.sample(20000,90_000_000_000L),0); // discard background interval
        expect(meter.sample(20030,90_500_000_000L),60);
        expect(meter.sample(20031,90_400_000_000L),0); // invalid time sample resets
        System.out.println("fps-meter: rates, stalls, counter reset and resume passed");
    }
}
