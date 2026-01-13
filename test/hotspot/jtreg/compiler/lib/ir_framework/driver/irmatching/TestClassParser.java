/*
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
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
import compiler.lib.ir_framework.driver.network.TestVmData;
import compiler.lib.ir_framework.driver.network.testvm.hotspot.MethodDumpHistory;
import compiler.lib.ir_framework.driver.network.testvm.hotspot.MethodDumps;
import compiler.lib.ir_framework.driver.network.testvm.java.IrEncoding;
import compiler.lib.ir_framework.driver.network.testvm.java.IrRuleIds;
import compiler.lib.ir_framework.driver.network.testvm.java.VmInfo;
import compiler.lib.ir_framework.shared.TestFormat;

import java.lang.reflect.Method;
import java.util.SortedSet;
import java.util.TreeSet;

/**
 * Class to parse the ideal compile phase and PrintOptoAssembly outputs of the test class and store them into a
 * collection of dedicated IRMethod objects used throughout IR matching.
 *
 * @see IRMethod
 */
public class TestClassParser {
    private final Class<?> testClass;
    private final IrEncoding irEncoding;
    private final MethodDumps methodDumps;
    private final boolean allowNotCompilable;
    private final VmInfo vmInfo;

    public TestClassParser(Class<?> testClass, TestVmData testVmData) {
        this.testClass = testClass;
        this.irEncoding = testVmData.irEncoding();
        this.methodDumps = testVmData.methodDumps();
        this.allowNotCompilable = testVmData.allowNotCompilable();
        this.vmInfo = testVmData.vmInfo();
    }

    /**
     * Parse the IR encoding and hotspot_pid* file to create a collection of {@link IRMethod} objects.
     * Return a default/empty TestClass object if there are no applicable @IR rules in any method of the test class.
     */
    public Matchable parse() {
        if (irEncoding.hasNoMethods()) {
            return new NonIRTestClass();
        }

        SortedSet<IRMethodMatchable> irMethods = new TreeSet<>();

        for (Method method : testClass.getDeclaredMethods()) {
            String methodName = method.getName();
            IrRuleIds irRuleIds = irEncoding.ruleIds(methodName);
            if (irRuleIds.isEmpty()) {
                continue;
            }
            MethodDumpHistory methodDumpHistory = methodDumps.methodDump(methodName);
            if (methodDumpHistory.isEmpty()) {
                Test[] testAnnos = method.getAnnotationsByType(Test.class);
                boolean allowNotCompilable = this.allowNotCompilable || testAnnos[0].allowNotCompilable();
                if (allowNotCompilable) {
                    irMethods.add(new NotCompilableIRMethod(method, irRuleIds.count()));
                } else {
                    irMethods.add(new NotCompiledIRMethod(method, irRuleIds.count()));
                }
                continue;
            }
            irMethods.add(new IRMethod(method, irRuleIds, methodDumpHistory, vmInfo));
        }
        TestFormat.throwIfAnyFailures();
        return new TestClass(irMethods);
    }
}
