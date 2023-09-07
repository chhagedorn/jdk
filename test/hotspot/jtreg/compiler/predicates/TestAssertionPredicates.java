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
 *
 */

/*
 * @test id=Xbatch
 * @bug 8288981
 * @summary Test all possible cases in which Assertion Predicates are required such that the graph is not left in a
 *          broken state to trigger assertions. Additional tests ensure the correctness of the implementation.
 * @run main/othervm -Xbatch
 *                   -XX:CompileCommand=compileonly,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=dontinline,compiler.predicates.TestAssertionPredicates::*
 *                   compiler.predicates.TestAssertionPredicates Xbatch
 */

/*
 * @test id=Xcomp
 * @bug 8288981
 * @run main/othervm -Xcomp
 *                   -XX:CompileCommand=compileonly,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=dontinline,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=inline,compiler.predicates.TestAssertionPredicates::inline
 *                   compiler.predicates.TestAssertionPredicates Xcomp
 */

/*
 * @test id=UseProfiledLoopPredicateFalse
 * @bug 8288981
 * @requires vm.compiler2.enabled
 * @run main/othervm -Xcomp -XX:-UseProfiledLoopPredicate
 *                   -XX:CompileCommand=compileonly,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=dontinline,compiler.predicates.TestAssertionPredicates::*
 *                   compiler.predicates.TestAssertionPredicates NoProfiledLoopPredicate
 */

/*
 * @test id=LoopMaxUnroll0
 * @bug 8288981
 * @requires vm.compiler2.enabled
 * @run main/othervm -Xcomp -XX:LoopMaxUnroll=0
 *                   -XX:CompileCommand=compileonly,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=dontinline,compiler.predicates.TestAssertionPredicates::*
 *                   compiler.predicates.TestAssertionPredicates LoopMaxUnroll0
 */

/*
 * @test id=LoopMaxUnroll2
 * @bug 8288981
 * @requires vm.compiler2.enabled
 * @run main/othervm -Xcomp -XX:LoopMaxUnroll=2
 *                   -XX:CompileCommand=compileonly,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=dontinline,compiler.predicates.TestAssertionPredicates::*
 *                   compiler.predicates.TestAssertionPredicates LoopMaxUnroll2
 */

/*
 * @test id=LoopUnrollLimit40
 * @bug 8288981
 * @requires vm.compiler2.enabled
 * @run main/othervm -Xcomp -XX:LoopUnrollLimit=40
 *                   -XX:CompileCommand=compileonly,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=dontinline,compiler.predicates.TestAssertionPredicates::*
 *                   compiler.predicates.TestAssertionPredicates LoopUnrollLimit40
 */

/*
 * @test id=LoopUnrollLimit150
 * @bug 8288981
 * @requires vm.compiler2.enabled
 * @run main/othervm -Xcomp -XX:LoopUnrollLimit=150
 *                   -XX:CompileCommand=compileonly,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=dontinline,compiler.predicates.TestAssertionPredicates::*
 *                   compiler.predicates.TestAssertionPredicates LoopUnrollLimit150
 */

/*
 * @test id=ZGC
 * @key randomness
 * @bug 8288981
 * @requires vm.gc.ZSinglegen
 * @requires vm.compiler2.enabled
 * @run main/othervm -Xcomp -XX:+UnlockDiagnosticVMOptions -XX:+StressGCM -XX:+UseZGC -XX:-ZGenerational
 *                   -XX:CompileCommand=compileonly,compiler.predicates.TestAssertionPredicates::*
 *                   -XX:CompileCommand=dontinline,compiler.predicates.TestAssertionPredicates::*
 *                   compiler.predicates.TestAssertionPredicates ZGCStressGCM
 */


package compiler.predicates;

public class TestAssertionPredicates {
    static int[] iArr = new int[100];
    static int[] iArr2 = new int[100];
    static int[] iArrNull = null;
    static int[][] iArr2D = new int[10][10];
    static short[] sArr = new short[10];
    static float[] fArr = new float[10];
    static float[][] fArr2D = new float[10][10];

    static boolean flag, flag2, flagTrue = true;
    static boolean flagFalse, flagFalse2;
    static int iFld = 34;
    static int iFld2, iFld3;
    static long lFld;
    static float fFld;
    static short sFld;
    static short five = 5;
    static byte byFld;
    volatile byte byFldVol;

    static class Foo {
        int iFld;
    }

    static Foo foo = new Foo();


    public static void main(String[] args) {
        try {
            executeTests(args[0]);
        } catch (ArrayIndexOutOfBoundsException e) {
            // Expected
        }
    }

    static void executeTests(String methods) {
        switch (methods) {
            case "NoProfiledLoopPredicate" -> testWithPartialPeelingFirst();
            case "LoopMaxUnroll0" -> {
                testPeeling();
                testUnswitchingThenPeeling();
                testPeelingThenUnswitchingThenPeeling();
                testPeelingThenUnswitchingThenPeelingThenPreMainPost();
                testDyingRuntimePredicate();
                testDyingNegatedRuntimePredicate();
            }
            case "LoopMaxUnroll2" -> {
                testPeelMainLoopAfterUnrollingThenPreMainPost();
                testPeelMainLoopAfterUnrolling2();
            }
            case "LoopUnrollLimit40" -> testPeelMainLoopAfterUnrollingThenPreMainPostThenUnrolling();
            case "LoopUnrollLimit150" -> {
                testUnrolling8();
                testUnrolling16();
            }
            case "Xcomp" -> {
                testPreMainPost();
                testUnrolling2();
                testUnrolling4();
                testPeelingThenPreMainPost();
                testUnswitchingThenPeelingThenPreMainPost();
                runTestDontCloneParsePredicateUnswitching();
                testDyingInitializedAssertionPredicate();
                test8288981();
                test8288941();
                iFld = -1;
                test8292507();
                TestAssertionPredicates t = new TestAssertionPredicates();
                t.test8296077();
                test8307131();
                test8308392No1();
                iFld = -50000;
                test8308392No2();
                test8308392No3();
                test8308392No4();
                test8308392No5();
                test8308392No6();
                test8308392No7();
                iFld = 0;
                test8308392No8();
                runTest8308392No9();
                test8308392No10();
                testSplitIfCloneDownWithOpaqueAssertionPredicate();
            }
            case "Xbatch" -> {
                for (int i = 0; i < 100000; i++) {
                    flag = !flag;
                    testHaltNotRemovingAssertionPredicate8305428();
                    test8305428();
                }
            }
            case "ZGCStressGCM" -> {
                for (int i = 0; i < 50_000_000; i++) {
                    // Need enough iterations to trigger crash in ZGC.
                    flag = !flag;
                    testDataUpdateUnroll();
                    testDataUpdateUnswitchUnroll();
                    testDataUpdatePeelingUnrolling();
                }
            }
            default -> throw new RuntimeException("invalid methods");
        }
    }

    static void runTestDontCloneParsePredicateUnswitching() {
        iFld = 0;
        try {
            testDontCloneParsePredicateUnswitching(); // Run with C1
        } catch (NullPointerException e) {
        }
        try {
            testDontCloneParsePredicateUnswitching(); // Run with C2
        } catch (NullPointerException e) {
        }
        Object o;
        String s;

        if (iFld != 68) {
            throw new RuntimeException("iFld must be 68 but was " + iFld);
        }
    }

    // -XX:-UseProfiledLoopPredicate -Xcomp -XX:CompileCommand=compileonly,Test*::test*
    static void testWithPartialPeelingFirst() {
        int i = 3;

        if (i > five) {
            while (true) {

                // Found as loop head in ciTypeFlow, but both path inside loop -> head not cloned.
                // As a result, this head has the safepoint as backedge instead of the loop exit test
                // and we cannot create a counted loop (yet). We first need to partial peel.
                if (flag) {
                }

                // Loop exit test.
                if (i <= five) {
                    break;
                }
                // <-- Partial Peeling CUT -->
                // Safepoint
                iFld = iArr[i];
                if (iFld == i) {
                    fFld = 324;
                }
                i--;
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test*::test* -XX:LoopUnrollLimit=40
    static void testPeelMainLoopAfterUnrollingThenPreMainPostThenUnrolling() {
        int zero = 34;
        int limit = 2;

        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
        }

        // 1) Pre/Main/Post
        // 2) Unroll
        // 3) Peel main loop
        // 4) Pre/Main/Post peeled main loop
        // 5) Unroll new main loop
        for (int i = 13; i > five; i -= 2) {
            iFld = iArr[i];
            if (iFld == i) {
                fFld = 324;
            }
            if (i < 13) { // Always true and folded in main loop because of executing pre-loop at least once -> i = [min_short..11]

                int k = iFld2 + i * zero; // Loop variant before CCP
                if (k  == 40) { // After CCP: Loop Invariant -> Triggers Loop Peeling of main loop
                    return;
                }
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test*::test* -XX:LoopMaxUnroll=2
    static void testPeelMainLoopAfterUnrollingThenPreMainPost() {
        int zero = 34;
        int limit = 2;

        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
        }

        for (int i = 9; i > five; i -= 2) {
            iFld = iArr[i];
            if (iFld == i) {
                fFld = 324;
            }
            if (i < 9) { // Always true and folded in main loop because of executing pre-loop at least once -> i = [min_short..7]
                int k = iFld2 + i * zero; // Loop variant before CCP
                if (k  == 40) { // 2) After CCP: Loop Invariant -> Triggers Loop Peeling of main loop
                    return;
                } else {
                    iFld3 = iArr2[i]; // After Peeling Main Loop: Check can be eliminated with Range Check Elimination -> 3) apply Pre/Main/Post and then 4) Range Check Elimination
                }
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test*::test* -XX:LoopMaxUnroll=2
    static void testPeelMainLoopAfterUnrolling2() {
        int zero = 34;
        int limit = 2;

        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
        }

        for (int i = 7; i > five; i -= 2) {
            iFld = iArr[i];
            if (iFld == i) {
                fFld = 324;
            }
            if (i < 7) { // Always true and folded in main loop because of executing pre-loop at least once -> i = [min_short..5]

                int k = iFld2 + i * zero; // Loop variant before CCP
                if (k  == 40) { // 2) After CCP: Loop Invariant -> Triggers Loop Peeling of main loop
                    return;
                }

            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static void testPreMainPost() {
        int x = 0;
        for (int i = 1; i > five; i -= 2) {
            x = iArr[i];
            if (x == i) {
                iFld = 34;
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static void testUnrolling2() {
        for (int i = 3; i > five; i -= 2) {
            int x = 0;
            x = iArr[i];
            if (x == i) {
                iFld = 34;
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static void testUnrolling4() {
        int x = 0;
        for (int i = 7; i > five; i -= 2) {
            x = iArr[i];
            if (x == i) {
                iFld = 34;
            }
        }
    }

    // -Xcomp -XX:LoopUnrollLimit=100 -XX:CompileCommand=compileonly,Test::*
    static void testUnrolling8() {
        int x = 0;
        for (int i = 15; i > five; i -= 2) {
            x = iArr[i];
            if (x == i) {
                iFld = 34;
            }
        }
    }

    // -Xcomp -XX:LoopUnrollLimit=100 -XX:CompileCommand=compileonly,Test::*
    static void testUnrolling16() {
        int x = 0;
        for (int i = 31; i > five; i -= 2) {
            x = iArr[i];
            if (x == i) {
                iFld = 34;
            }
        }
    }


    // -Xcomp -XX:LoopMaxUnroll=0 -XX:CompileCommand=compileonly,Test::*
    static void testPeeling() {
        for (int i = 1; i > five; i -= 2) {
            iFld = iArr[i];

            if (flag) {
                return;
            }

            if (iFld == i) {
                iFld = 34;
            }
        }
    }

    // -Xcomp -XX:LoopMaxUnroll=0 -XX:CompileCommand=compileonly,Test::*
    static void testUnswitchingThenPeeling() {
        for (int i = 1; i > five; i -= 2) {
            iFld = iArr[i];

            if (flag2) {
                iFld2 = 24;
            }

            if (flag) {
                return;
            }

            if (iFld == i) {
                iFld = 34;
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test*::test* -XX:LoopMaxUnroll=0
    static void testPeelingThenUnswitchingThenPeeling() {
        int zero = 34;
        int limit = 2;

        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
        }

        for (int i = 3; i > five; i -= 2) {
            iFld = iArr[i];
            if (iFld == i) {
                fFld = 324;
            }

            if (flag) { // 1) Triggers loop peeling
                return;
            }

            int k = iFld2 + i * zero; // loop variant before CCP

            if (k == 34) { // 2) After CCP loop invariant -> triggers loop unswitching
                iFld = 3;

            } else {
                iFld = iArr2[i]; // 3) After loop unswitching, triggers loop peeling again.
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test*::test* -XX:LoopMaxUnroll=0
    static void testPeelingThenUnswitchingThenPeelingThenPreMainPost() {
        int zero = 34;
        int limit = 2;

        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
        }

        for (int i = 5; i > five; i -= 2) {
            iFld = iArr[i];
            if (iFld == i) {
                fFld = 324;
            }

            if (flag) { // 1) Triggers loop peeling
                return;
            }

            int k = iFld2 + i * zero; // loop variant before CCP

            if (k == 34) { // 2) After CCP loop invariant -> triggers loop unswitching
                iFld = 3;

            } else {
                iFld = iArr2[i]; // 3) After loop unswitching, triggers loop peeling again, then pre/main/post
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static void testPeelingThenPreMainPost() {
        int three = 0;
        int limit = 2;
        long l1 = 34L;
        long l2 = 35L;

        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            three = 3;
        }

        for (int i = 3; i > five; i -= 2) {
            iFld = iArr[i];
            if (iFld == i) {
                iFld = 34;
            }

            if (i > three) {
                // DivLs add 30 to the loop body count and we hit LoopUnrollLimit.
                // After CCP, these statements are folded away and we can unroll this loop.
                l1 /= lFld;
                l2 /= lFld;
            }

            if (flag) {
                return;
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static void testUnswitchingThenPeelingThenPreMainPost() {
        int three = 0;
        int limit = 2;
        long l1 = 34L;
        long l2 = 35L;

        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            three = 3;
        }

        for (int i = 3; i > five; i -= 2) {
            iFld = iArr[i];
            if (iFld == i) {
                iFld = 34;
            }

            if (i > three) {
                // DivLs add 30 to the loop body count and we hit LoopUnrollLimit.
                // After CCP, these statements are folded away and we can unroll this loop.
                l1 /= lFld;
                l2 /= lFld;
            }

            if (flag2) {
                iFld2 = 34;
            }

            if (flag) {
                return;
            }
        }
    }

    public static void testDontCloneParsePredicateUnswitching() {
        int i = 0;

        // Partially peeled to: for (int i = 0; i < 100; i++) {}. Store iFld is peeled and ends up at Parse Predicate:
        //
        //     Parse Predicate
        //       /           \
        // Counted Loop    StoreI
        //
        // When unswitching, we cannot clone the Parse Predicate due to the control dependency of the StoreI that is
        // outside the loop. If we still allowed it, we could create a new Hoisted Predicate for the fast or slow loop
        // and if it is hit during runtime, we could have wrongly executed the StoreI already, even though we jump
        // to the start of the loop in the interpreter:
        //           Old Parse Predicate
        //               /             \
        //         Loop Selector If    StoreI
        //               |
        //         Hoisted Predicate  # Hit at runtime? Jump to interpreter before loop but StoreI might already be executed.
        //               |            # We end up executing it twice.
        //         Parse Predicate
        //               |
        //           Counted Loop
        // After unswitching and allowing Parse Predicates to be cloned, we created a Hoisted Predicate for the null check
        // of iArrNull:
        //

        while (true) {

            // Found as loop head in ciTypeFlow, but both path inside loop -> head not cloned.
            // As a result, this head has the safepoint as backedge instead of the loop exit test
            // and we cannot create a counted loop (yet). We first need to partial peel.
            // Found as loop head in ciTypeFlow, but both path inside loop -> head not cloned.
            // As a result, this head has the safepoint as backedge instead of the loop exit test
            // and we cannot create a counted loop (yet). We first need to partial peel.
            if (flag) {
            }

            iFld += 34; // Only statement that is peeled. StoreI ends up with control input from Parse Predicate before loop.
            // Loop exit test.
            if (i >= 100) {
                break;
            }
            // <-- Partial Peeling CUT -->
            // Safepoint

            if (flagTrue) {
                iArrNull[i] = 34;
            }
            i++;
        }
    }

    // Initialized Assertion Predicate with ConI as bool node is not recognized, and we miss to remove a Template
    // Assertion Predicate from which we later create a wrong Initialized Assertion Predicate (for wrong loop).
    static void testDyingInitializedAssertionPredicate() {
        boolean b = false;
        int i4, i6, i7 = 14, i8, iArr[][] = new int[10][10];
        for (int i = 0; i < iArr.length; i++) {
            inline(iArr[i]);
        }
        for (i4 = 7; i4 < 10; i4++) {
            iArr2[1] += 5;
        }
        for (i6 = 100; i6 > 4; --i6) {
            i8 = 1;
            while (++i8 < 6) {
                sArr[i8] = 3;
                i7 += i8 + i8;
                iArr2[i8] -= 34;
            }
        }
    }

    public static void inline(int[] a) {
        for (int i = 0; i < a.length; i++) {
            a[i] = 4;
        }
    }

    static void testDyingRuntimePredicate() {
        int zero = 34;
        int[] iArrLoc = new int[100];

        int limit = 2;
        int loopInit = -10;
        int four = -10;
        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
            loopInit = 99;
            four = 4;
        }

        // Template + Hoisted Range Check Predicate for iArr[i]. Hoisted Invariant Check Predicate IC for iArr3[index].
        // After CCP: ConI for IC and uncommon proj already killed -> IGVN will fold this away. But Predicate logic
        // need to still recognize this predicate to find the Template above to kill it. If we don't do it, then it
        // will end up at loop below and peeling will clone the template and create a completely wrong Initialized
        // Assertion Predicate, killing some parts of the graph and leaving us with a broken graph.
        for (int i = loopInit; i < 100; i++) {
            iArr[i] = 34;
            iArrLoc[four] = 34;
            //if (-3 > loop) {
            //    iArr3[101] = 34; // Always out of bounds and will be a range_check trap in the graph.
            //}
        }

        int i = -10;
        while (true) {

            // Found as loop head in ciTypeFlow, but both path inside loop -> head not cloned.
            // As a result, this head has the safepoint as backedge instead of the loop exit test
            // and we cannot create a counted loop (yet). We first need to partial peel.
            if (zero * i == 34) {
                iFld2 = 23;
            } else {
                iFld = 2;
            }

            // Loop exit test.
            if (i >= -2) {
                break;
            }
            // <-- Partial Peeling CUT -->
            // Safepoint
            if (zero * i + five == 0) {
                return;
            }
            iFld2 = 34;
            i++;
        }
    }

    static void testDyingNegatedRuntimePredicate() {
        int zero = 34;
        int[] iArrLoc = new int[100];

        int limit = 2;
        int loopInit = -10;
        int four = -10;
        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
            loopInit = 99;
            four = 4;
        }

        // Template + Hoisted Range Check Predicate for iArr[i]. Hoisted Invariant Check Predicate IC for iArr3[index].
        // After CCP: ConI for IC and uncommon proj already killed -> IGVN will fold this away. But Predicate logic
        // need to still recognize this predicate to find the Template above to kill it. If we don't do it, then it
        // will end up at loop below and peeling will clone the template and create a completely wrong Initialized
        // Assertion Predicate, killing some parts of the graph and leaving us with a broken graph.
        for (int i = loopInit; i < 100; i++) {
            iArr[i] = 34;
            if (-3 > loopInit) {
                // Negated Hoisted Invariant Check Predicate.
                iArrLoc[101] = 34; // Always out of bounds and will be a range_check trap in the graph.
            }
        }

        int i = -10;
        while (true) {

            // Found as loop head in ciTypeFlow, but both path inside loop -> head not cloned.
            // As a result, this head has the safepoint as backedge instead of the loop exit test
            // and we cannot create a counted loop (yet). We first need to partial peel.
            if (zero * i == 34) {
                iFld2 = 23;
            } else {
                iFld = 2;
            }

            // Loop exit test.
            if (i >= -2) {
                break;
            }
            // <-- Partial Peeling CUT -->
            // Safepoint
            if (zero * i + five == 0) {
                return;
            }
            iFld2 = 34;
            i++;
        }
    }

    /**
     * Tests to verify correct data dependencies update when splitting loops for which we created Hoisted Predicates.
     * If they are not updated correctly, we could wrongly execute an out-of-bounds load resulting in a segfault.
     */

    // -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-ZGenerational -XX:+UseZGC -XX:+StressGCM
    static void testDataUpdateUnroll() {
        Foo[] arr;
        if (flag) {
            arr = new Foo[] {foo, foo, foo};
        } else {
            arr = new Foo[] {foo, foo, foo, foo};
        }

        for (int i = 0; i < arr.length ; i++) {
            arr[i].iFld += 34;
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-ZGenerational -XX:+UseZGC -XX:+StressGCM
    static void testDataUpdateUnswitchUnroll() {
        Foo[] arr;
        if (flag) {
            arr = new Foo[] {foo, foo, foo};
        } else {
            arr = new Foo[] {foo, foo, foo, foo};
        }

        for (int i = 0; i < arr.length ; i++) {
            arr[i].iFld += 34;
            if (flag) {
                iFld = 23;
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-ZGenerational -XX:+UseZGC -XX:+StressGCM
    static void testDataUpdatePeelingUnrolling() {
        int zero = 34;
        int limit = 2;

        long l1 = 34L;
        long l2 = 566L;

        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
        }

        Foo[] arr;
        if (flag) {
            arr = new Foo[] {foo, foo, foo};
        } else {
            arr = new Foo[] {foo, foo, foo, foo};
        }

        for (int i = 0; i < arr.length; i++) {
            arr[i].iFld &= 0x1f1f1f1f;

            if (zero > i) {
                // DivLs add 30 to the loop body count and we hit LoopUnrollLimit. Add more
                // statements/DivLs if you want to use a higher LoopUnrollLimit value.
                // After CCP, these statements are folded away and we can unroll this loop.
                l1 /= lFld;
                l2 /= lFld;
            }

            if (flagFalse2 && i > 1) { // 1) Triggers Loop Unswitching
                return;
            }

            if (flagFalse) { // 2) Triggers Loop Peeling
                return;
            }
        }
    }


    /**
     * Tests collected in JBS and duplicated issues
     */

    // -Xbatch -XX:CompileCommand=compileonly,Test::*
    public static void testHaltNotRemovingAssertionPredicate8305428() {
        int zero = 34;

        int limit = 2;
        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 1;
        }

        int i = 80;
        for (int j = 0; j < zero; j++) {
            while (true) {

                // Found as loop head in ciTypeFlow, but both path inside loop -> head not cloned.
                // As a result, this head has the safepoint as backedge instead of the loop exit test
                // and we cannot create a counted loop (yet). We first need to partial peel.
                if (flag) {
                }

                // Loop exit test.
                if (i < -5) {
                    break;
                }
                // <-- Partial Peeling CUT -->
                // Safepoint
                fFld = iArr2[i+5];
                i--;
            }
            iArr[j] = 3;
        }
    }

    // -Xbatch -XX:CompileCommand=compileonly,Test::*
    static void test8305428() {
        int j = 1;
        do {
            for (int k = 270; k > 1; --k) {
                iFld++;
            }

            switch (j) {
                case 1:
                    switch (92) {
                        case 92:
                            flag = flag;
                    }
                case 2:
                    iArr[j] = 3;
            }
        } while (++j < 100);
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static void test8288981() {
        int x = 1;
        // Sufficiently many iterations to trigger OSR
        for (int j = 0; j < 50_000; j++) {
            for (int i = 1; i > x; --i) {
                float v = fArr[0] + fFld;
                fArr2D[i + 1][x] = v;
                iFld += v;
            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static int test8288941() {
        int e = 8, g = 5, j;
        int h = 1;
        while (++h < 100000) {
            for (j = 1; j > h; j--) {
                try {
                    iFld = 0;
                    g = iArr[1] / e;
                } catch (ArithmeticException ae) {
                }
                iArr[j + 1] = 4;
                if (e == 9) {
                    iFld2 = 3;
                }
            }
        }
        return g;
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static void test8292507() {
        int i = iFld, j, iArr[] = new int[40];
        while (i > 0) {
            for (j = i; 1 > j; ++j) {
                try {
                    iArr[j] = 0;
                    iArr[1] = iFld = 0;
                } catch (ArithmeticException a_e) {
                }
                switch (i) {
                    case 4:
                    case 43:
                        iFld2 = j;
                }

            }
        }
    }

    // -Xcomp -XX:CompileCommand=compileonly,Test::*
    static void test8307131() {
        int i21 = 6, i, i23 = 3, y, iArr2[] = new int[40];
        for (i = 50000; 3 < i; i--) {
            for (y = 2; y > i; y--) {
                try {
                    i21 = i23 / 416 / iArr2[y];
                } catch (ArithmeticException a_e) {
                }
                i23 -= 3;
            }
        }
        i21 -= 2;
    }

    void test8296077() {
        int i4 = 4, iArr1[] = new int[10];
        float f2;
        for (f2 = 7; f2 > 4; --f2) {}

        float f4 = five;
        while (f4 < 50000) {
            for (int i7 = (int) f4; 1 > i7; i7++) {
                iArr1[i7] = 5;
                if (i4 != 0) {
                    return;
                }
                byFldVol = 2;
                try {
                    iArr1[(int) f4] = i4 = iArr1[(int) f4 + 1] % iArr1[(int) f2];
                } catch (ArithmeticException a_e) {
                }
            }
            f4++;
        }
    }

    static void test8308392No1() {
        int i10, i16;
        try {
            for (i10 = 61; i10 < 50000 ; i10++) {
                for (i16 = 2; i16 > i10; i16--) {
                    sFld *= iArr2D[i16][i16] = byFld *= sFld;
                }
            }
        } catch (NegativeArraySizeException exc3) {
        }
    }

    static void test8308392No2() {
        try {
            int j, k, i19 = 8;

            for (int i = 0; i < 10; i++) {
                for (j = 2; j < 3; ) {
                    if (flagTrue) {
                        iFld++;
                        iFld2 = 34 / iFld; // Will eventually divide by zero and break infinite loop.
                    }
                    for (k = 2; k > j; --k) {
                        i19 *= fArr2D[k][j] += i;
                    }
                }
            }

        } catch (ArithmeticException e) {
            // Expected
        }
    }
    static void test8308392No3() {
        int i18, i19, i21, i22 = 1, iArr2[][] = new int[40][];
        double dArr[] = new double[40];

        i18 = 1;
        for (i19 = 5; i19 < 50000; i19++) {
            for (i21 = i18; i21 < 4; ++i21) {
                switch (i19) {
                    case 4:
                        iArr2[i21 - 1][i18] = 3;
                        try {
                            iFld = 2 % iFld;
                            iFld = i22;
                        } catch (ArithmeticException a_e) {
                        }
                        break;
                    case 45:
                        i22 += dArr[i22];
                }
            }
        }
    }

    static void test8308392No4() {
        int i20, i22 = 6, i25;
        for (i20 = 50000; i20 > 3; i20--) {
            for (i25 = 1; i25 > i22; i25--) {
                iArr2D[i25 + 1][i22] += fFld -= iFld;
            }
        }
    }

    static void test8308392No5() {
        float f1;
        int i20, i23, i25 = 5, i26 = 4, i27;
        long lArr[][] = new long[10][10];
        for (f1 = 40; f1 > 3; --f1) {
            for (i20 = 2; 11 > i20; i20++) {
                for (i23 = 1; i23 < 11; ) {
                    i23++;
                    for (i27 = (int) f1; i27 < 1; ++i27) {
                        iFld = 3;
                        lArr[i27][i25] = 5;
                        if (flag) {
                            i26 += i25;
                        }
                    }
                }
            }
        }
    }

    static void test8308392No6() {
        int i, i1, i2, i23 = 7, i24;
        double dArr1[][] = new double[10][];
        boolean bArr[] = new boolean[10];
        for (i = 9; i < 88; i++) {
            i2 = 1;
            do {
                i1 = Short.reverseBytes((short) 0);
                for (i24 = 1; i2 < i24; --i24) {
                    i1 %= dArr1[i24 + 1][i];
                    switch (i23) {
                        case 0:
                            bArr[i] = false;
                    }
                }
                i2++;
            } while (i2 < 50000);
        }
    }

    static void test8308392No7() {
        int i16 = 2, i17 = 1, i18, i20, i21, i23;
        double d2 = 86.53938;
        long lArr[][] = new long[10][];
        for (i18 = 1; i18 < 10; i18++) {
            i20 = 1;
            while (i20 < 5000) {
                for (i21 = i23 = 1; i23 > i20; --i23) {
                    d2 *= i16 >>= lArr[i23 + 1][i20] >>= i17;
                }
                i20++;
            }
        }
    }
    static void test8308392No8() {
        int i21, i22, i25 = 1, i26 = 032, i28;
        i21 = iFld;
        while (--i21 > 0) {
            for (i22 = 2; i22 < 71; i22++) {
                for (i28 = 2; i28 > i21; --i28) {
                    i25 %= i26;
                    iArr2D[i28][1] ^= 5;
                }
            }
            i21--;
        }
    }

    static void runTest8308392No9() {
        try {
            test8308392No9();
        } catch (ArithmeticException e) {
            // Expected.
        }
    }

    static void test8308392No9() {
        for (int i20 = 60; ; i20--) {
            for (int i22 = 2; i22 > i20; --i22) {
                fFld += 5;
                iFld = iFld / 9 / iArr[i22];
            }
        }
    }

    static void test8308392No10() {
        int i14, i16 = -27148, i18, i21;
        for (i14 = 21; i16 < 9; ++i16) {
            for (i18 = 2; i14 < i18; i18--) {
                iArr2D[i18][i18] -= lFld = i18;
            }
            for (i21 = 1; i21 < 2; i21++) {}
        }
    }

    static void testSplitIfCloneDownWithOpaqueAssertionPredicate() {
        int p = 0, j;
        if (flag) {
            iArr[3] = 3;
            dontInline();
        }
        int i = 1;
        while (++i < 4) {
            if (flag) {
                p = 8;
            }
            iArr[i - 1] = 4;
            for (j = 1; j < 3; ++j) {
                iArr[j] = 34;
            }
        }
        long n = p;
    }

    static void dontInline() {
    }

}
