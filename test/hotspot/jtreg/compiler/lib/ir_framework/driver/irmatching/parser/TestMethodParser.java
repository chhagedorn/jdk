/*
 * Copyright (c) 2022, 2023, Oracle and/or its affiliates. All rights reserved.
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

package compiler.lib.ir_framework.driver.irmatching.parser;

import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.driver.irmatching.parser.hotspot.HotSpotPidFileParser;
import compiler.lib.ir_framework.driver.network.IrEncoding;
import compiler.lib.ir_framework.shared.TestFormat;
import compiler.lib.ir_framework.test.IrEncodingPrinter;

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Class to parse the IR encoding emitted by the test VM and creating {@link TestMethod} objects for each entry.
 *
 * @see TestMethod
 */
public class TestMethodParser {
    private final Map<String, TestMethod> testMethods;
    private final Class<?> testClass;

    public TestMethodParser(Class<?> testClass) {
        this.testClass = testClass;
        this.testMethods = new HashMap<>();
    }

    /**
     * Parse the IR encoding passed as parameter and return a "test name" -> TestMethod map that contains an entry
     * for each method that needs to be IR matched on.
     */
    public TestMethods parse(IrEncoding irEncoding) {
        createTestMethodMap(irEncoding, testClass);
        // We could have found format errors in @IR annotations. Report them now with an exception.
        TestFormat.throwIfAnyFailures();
        return new TestMethods(testMethods);
    }

    /**
     * Sets up a map testname -> TestMethod map. The TestMethod object will later be filled with the ideal and opto
     * assembly output in {@link HotSpotPidFileParser}.
     */
    private void createTestMethodMap(IrEncoding irEncoding, Class<?> testClass) {
        Map<String, List<Integer>> irRulesMap = irEncoding.methods();
        createTestMethodsWithEncoding(testClass, irRulesMap);
    }

    private void createTestMethodsWithEncoding(Class<?> testClass, Map<String, List<Integer>> irRulesMap) {
        for (Method m : testClass.getDeclaredMethods()) {
            IR[] irAnnos = m.getAnnotationsByType(IR.class);
            if (irAnnos.length > 0) {
                // Validation of legal @IR attributes and placement of the annotation was already done in Test VM.
                List<Integer> irRuleIds = irRulesMap.get(m.getName());
                validateIRRuleIds(m, irAnnos, irRuleIds);
                if (hasAnyApplicableIRRules(irRuleIds)) {
                    testMethods.put(m.getName(), new TestMethod(m, irAnnos, irRuleIds));
                }
            }
        }
    }

    private void validateIRRuleIds(Method m, IR[] irAnnos, List<Integer> ids) {
        TestFramework.check(ids != null, "Should find method name in validIrRulesMap for " + m);
        TestFramework.check(!ids.isEmpty(), "Did not find any rule indices for " + m);
        TestFramework.check((ids.getFirst() >= 1 || ids.getFirst() == IrEncodingPrinter.NO_RULE_APPLIED)
                            && ids.getLast() <= irAnnos.length,
                            "Invalid IR rule index found in validIrRulesMap for " + m);
    }

    /**
     * Does the list of IR rules contain any applicable IR rules for the given conditions?
     */
    private boolean hasAnyApplicableIRRules(List<Integer> irRuleIds) {
        return irRuleIds.getFirst() != IrEncodingPrinter.NO_RULE_APPLIED;
    }
}
