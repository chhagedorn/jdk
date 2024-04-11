/*
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 8328480
 * @summary Test that SubTypeCheckNode takes improved unique concrete klass constant in order to fold consecutive sub
 *          type checks.
 * @library /test/lib /
 * @run driver compiler.types.TestSubTypeCheckUniqueSubclass
 */

package compiler.types;

import compiler.lib.ir_framework.*;

public class TestSubTypeCheckUniqueSubclass {
    static Object o = new C(); // Make sure C is loaded.
    static Object o2 = new C2(); // Make sure C2 is loaded while NeverLoaded is not.

    static Object[] oArr = new Object[100];
    static Object[] oArr2 = new Object[100];
    static X[] xArr = new X[2];
    static Integer[] iArr = new Integer[2];
    static X x = new Y();

    public static void main(String[] args) {
        TestFramework.runWithFlags("-XX:+IgnoreUnrecognizedVMOptions", "-XX:-MonomorphicArrayCheck");
        TestFramework.run();
    }

   @Test
   @Warmup(0)
   @IR(counts = {IRNode.SUBTYPE_CHECK, "1"},
       phase = CompilePhase.ITER_GVN1)
   static void testAbstractAbstract() {
        A a = (A)o;
        A a2 = (B)o;
   }

   @Test
   @Warmup(0)
   @IR(counts = {IRNode.SUBTYPE_CHECK, "1"},
       phase = CompilePhase.ITER_GVN1)
   static void testAbstractAbstractWithUnloaded() {
       A2 a = (A2)o2;
       A2 a2 = (B2)o2;
   }

    @Run(test = "testImproveLoadKlass")
    static void runTestImproveLoadKlass(RunInfo runInfo) {
        if (runInfo.isWarmUp()) {
            // Add some profiling that store() is only called with Objects
            store(o, oArr);
            store(o2, oArr2);
        } else {
            testImproveLoadKlass(); // First called after compilation -> as if compiled test with -Xcomp.
        }
    }

    @Test
    @IR(failOn = IRNode.SUBTYPE_CHECK, phase = CompilePhase.BEFORE_MACRO_EXPANSION)
    static void testImproveLoadKlass() {
        int zero = 34;

        int limit = 2;
        for (; limit < 4; limit *= 2);
        for (int i = 2; i < limit; i++) {
            zero = 0;
        }

        // Array store is inlined.
        // After parsing: SubTypeCheckNode(CastPP[exact Y], LoadKlass[Obj]) -> CastPP could already be improved.
        // After CCP: SubTypeCheckNode(CastPP[exact Y], LoadKlass[X])
        // -> since X is not exact, we cannot fold the sub type check.
        // => With new fix: We can improve LoadKlass[X] to LoadKlass[exact Y] and fold away the sub type check.
        store(x, zero == 0 ? xArr : iArr);
    }

    @ForceInline
    static void store(Object a, Object[] arr) {
        arr[1] = a;
    }
}

abstract class A {}
abstract class B extends A {}
class C extends B {}

abstract class A2 {}
abstract class B2 extends A2 {}
class C2 extends B2 {}

// Class never loaded -> C2 looks like unique sub class.
class NeverLoaded extends B2 {}

abstract class X {}
final class Y extends X {}