public class Reduced {
    static byte byFld;
    static short sFld;
    static int iArrFld[][];

    public static void main(String[] strArr) {
        test();
    }

    static void test() {
        int i10, i16;
        try {
            for (i10 = 61; ; ) {
                for (i16 = 2; i16 > i10; i16--) {
                    sFld *= iArrFld[i16][i16] = byFld *= sFld;
                }
            }
        } catch (NegativeArraySizeException exc3) {
        }
    }
}
