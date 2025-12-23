/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
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

package compiler.lib.ir_framework.driver.irmatching;

import compiler.lib.ir_framework.Test;
import compiler.lib.ir_framework.driver.irmatching.irmethod.IRMethod;
import compiler.lib.ir_framework.driver.irmatching.irmethod.IRMethodMatchable;
import compiler.lib.ir_framework.driver.irmatching.irmethod.NotCompilableIRMethod;
import compiler.lib.ir_framework.driver.irmatching.irmethod.NotCompiledIRMethod;
import compiler.lib.ir_framework.driver.network.testVMData;
import compiler.lib.ir_framework.driver.network.testvm.c2.MethodDumpHistory;
import compiler.lib.ir_framework.driver.network.testvm.c2.MethodDumps;
import compiler.lib.ir_framework.driver.network.testvm.java.IREncoding;
import compiler.lib.ir_framework.driver.network.testvm.java.IRRuleIds;
import compiler.lib.ir_framework.driver.network.testvm.java.VMInfo;
import compiler.lib.ir_framework.shared.TestFormat;

import java.lang.reflect.Method;
import java.util.SortedSet;
import java.util.TreeSet;

/**
 * Class to build a matchable {@link TestClass} object by going over all IR tests, collecting the outputs for all
 * requested compile phases and storing them in dedicated {@link IRMethod} objects. These are later used by the
 * {@link IRMatcher}.
 */
public class TestClassBuilder {
    private final Class<?> testClass;
    private final IREncoding irEncoding;
    private final MethodDumps methodDumps;
    private final boolean allowNotCompilable;
    private final VMInfo vmInfo;

    public TestClassBuilder(Class<?> testClass, testVMData testVmData) {
        this.testClass = testClass;
        this.irEncoding = testVmData.irEncoding();
        this.methodDumps = testVmData.methodDumps();
        this.allowNotCompilable = testVmData.allowNotCompilable();
        this.vmInfo = testVmData.vmInfo();
    }

    /**
     * Build a matchable {@link TestClass} object. Returns a default/empty {@link TestClass} object if there are no
     * applicable @IR rules in any method of the test class.
     */
    public Matchable build() {
        if (irEncoding.hasNoMethods()) {
            return new NonIRTestClass();
        }

        SortedSet<IRMethodMatchable> irMethods = new TreeSet<>();
        for (Method method : testClass.getDeclaredMethods()) {
            buildForMethod(method, irMethods);
        }
        TestFormat.throwIfAnyFailures();
        return new TestClass(irMethods);
    }

    private void buildForMethod(Method method, SortedSet<IRMethodMatchable> irMethods) {
        IRRuleIds irRuleIds = irEncoding.ruleIds(method.getName());
        if (irRuleIds.isEmpty()) {
            // Not an IR test or not interested.
            return;
        }
        IRMethodMatchable irMethod = createIrMethod(method, irRuleIds);
        irMethods.add(irMethod);
    }

    private IRMethodMatchable createIrMethod(Method method, IRRuleIds irRuleIds) {
        MethodDumpHistory methodDumpHistory = methodDumps.methodDump(method.getName());
        if (methodDumpHistory.isEmpty()) {
            return createIrMethodForEmptyDump(method, irRuleIds);
        }
        return new IRMethod(method, irRuleIds, methodDumpHistory, vmInfo);
    }

    private IRMethodMatchable createIrMethodForEmptyDump(Method method, IRRuleIds irRuleIds) {
        Test[] testAnnos = method.getAnnotationsByType(Test.class);
        boolean allowNotCompilable = this.allowNotCompilable || testAnnos[0].allowNotCompilable();
        if (allowNotCompilable) {
            return new NotCompilableIRMethod(method, irRuleIds.count());
        } else {
            return new NotCompiledIRMethod(method, irRuleIds.count());
        }
    }
}
