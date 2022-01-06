/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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
package compiler.c2.irTests.loopOpts;

import compiler.lib.ir_framework.*;

/*
 * @test
 * @library /test/lib /
 * @run driver compiler.c2.irTests.loopOpts.CodeGenTest
 */
public class CodeGenTest {
    public static void main(String[] args) {
        TestFramework.run();
    }

    private int iFld;

    @DontInline
    private void blackhole() { }

    @Test
    @IR(failOn = {IRNode.CALL_OF_METHOD, "blackhole"}, phase = Phase.AFTER_OPTIMIZATIONS) // fail, matching CallStaticJava
    @IR(failOn = {IRNode.CALL_OF_METHOD, "blackhole"}, phase = Phase.AFTER_CODE_GEN) // fail, matching CallStaticJavaDirect
    @IR(failOn = IRNode.MACH_PROJ, phase = Phase.AFTER_OPTIMIZATIONS) // work, no MachProj, yet
    @IR(failOn = IRNode.MACH_PROJ, phase = Phase.AFTER_CODE_GEN) // fail, MachProj after code gen
    public void test(int x) {
        if (x == 23) {
            blackhole();
        }
        iFld = 34;
    }

    @Run(test = "test")
    @Warmup(1)
    public void run(RunInfo info) {
        for (int i = 24; i < 10000; i++) {
            test(i); // Compiled with UCT, no blackhole()
        }
        test(23); // Compiled with call to blackhole()
    }
}
