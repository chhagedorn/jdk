/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates. All rights reserved.
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

package ir_framework.examples;

import compiler.lib.ir_framework.*;
import compiler.lib.ir_framework.driver.irmatching.IRViolationException;

/*
 * @test
 * @summary Example test to use the new test framework.
 * @library /test/lib /
 * @run driver ir_framework.examples.IRExample
 */

/**
 * The file shows some examples how IR verification can be done by using the {@link IR @IR} annotation. Additional
 * comments are provided at the IR rules to explain their purpose. A more detailed and complete description about
 * IR verification and the possibilities to write IR tests with {@link IR @IR} annotations can be found in the
 * IR framework README.md file.
 *
 * @see IR
 * @see Test
 * @see TestFramework
 */
public class IRExample {
    int iFld, iFld2, iFld3;

    public static void main(String[] args) {
        TestFramework.run(); // First run tests from IRExample. No failure.
    }

    // Rules with failOn constraint which all pass.
    @Test
    @IR(failOn = IRNode.LOAD) // 1 (pre-defined) IR node
    @IR(failOn = {IRNode.LOAD, IRNode.LOOP}) // 2 IR nodes
    @IR(failOn = {IRNode.LOAD, "some regex that does not occur"}, // 1 IR node with a user-defined regex
            phase = CompilePhase.PRINT_IDEAL)
    // Rule with special configurable IR nodes. All IR nodes with a "_OF" postfix expect a second string specifying an
    // additional required information.
    @IR(failOn = {IRNode.STORE_OF_FIELD, "iFld2", IRNode.LOAD, IRNode.STORE_OF_CLASS, "Foo"})
    // Only apply this rule if the VM flag UseZGC is true
    @IR(applyIf = {"UseZGC", "true"}, failOn = IRNode.LOAD)
    // We can also use comparators (<, <=, >, >=, !=, =) to restrict the rules.
    // This rule is only applied if TypeProfileLevel is 100 or greater.
    @IR(applyIf = {"TypeProfileLevel", ">= 100"}, failOn = IRNode.LOAD)
    public void goodFailOn() {
        iFld = 42; // No load, no loop, no store to iFld2, no store to class Foo
    }
}