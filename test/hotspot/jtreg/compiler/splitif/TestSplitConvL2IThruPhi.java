/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/**
* @test
* @bug 8298851
* @summary Test split through phi with ConvL2I.
* @run main/othervm -Xcomp -XX:CompileCommand=compileonly,compiler.splitif.TestSplitConvL2IThruPhi::*
*                   compiler.splitif.TestSplitConvL2IThruPhi
*/

package compiler.splitif;

public class TestSplitConvL2IThruPhi {
    static int x;
    static int iFld;
    static long lFld;
    static boolean flag;


    public static void main(String[] strArr) {
        for (int i = 0; i < 1000; i++) {
            testConvI2LPushThruPhi();
            testConvL2IPushThruPhi();
            testFuzzer();
        }
    }
    static void testConvI2LPushThruPhi() {
        for (int i = 10; i > 0; i--) {
            // The only use of ConvI2L is the DivL node. In first split-if application: late ctrl is the zero check proj
            // After first IGVN: Zero check removed. Later, we re-apply split-if and if ew split ConvI2L through the iv
            // phi i, we end up with a wrong type range for ConvI2L and the bailout for the division (does not have a
            // zero check) does not work because the ConvI2L input on the backedge does not contain zero in its type
            // range even though i could be zero. We need to handle this case to avoid a SIGFPE (see JDK-8299259) and
            // bail out of split-if (already fixed by JDK-6659207)
            lFld = 10L / (long) i;
        }
    }

    static void testConvL2IPushThruPhi() {
        for (long i = 10; i > 0; i--) {
            // ConvL2I (**) has two uses: DivL and CmpL. To prevent split-if before first IGVN iteration, we insert a
            // one iteration loop that is removed in first loop opts phase. IGVN is then run and the types of all nodes
            // in the loop are updated according to the improved type of the iv phi i. If we do not bail out, we get the
            // same problem as above (see JDK-8299259). To fix this, we should also bail out of split-if.
            for (int j = 0; j < 1; j++) {
            }
            iFld = 10 / (int)i; // (**)
            for (int j = 0; j < 10; j++) {
                flag = !flag;
            }
        }
    }

    static int testFuzzer() {
        int a = 5, b = 6;
        long lArr[] = new long[2];

        for (long i = 159; i > 1; i -= 3) {
            a += 3;
            for (int j = 1; j < 4; j++) {
                if (a == 9) {
                    if (x == 73) {
                        try {
                            b = 10 / (int) i;
                        } catch (ArithmeticException a_e) {
                        }
                    }
                }
            }
        }
        return b;
    }
}
