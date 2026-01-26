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

package compiler.lib.ir_framework.driver.network.testvm.java;

import compiler.lib.ir_framework.TestFramework;

/**
 * Class to collect all Java messages sent from the Test VM to the Driver VM.
 */
public class JavaMessages {
    private static final boolean PRINT_APPLICABLE_IR_RULES = Boolean.parseBoolean(System.getProperty("PrintApplicableIRRules", "false"));

    private final StdoutMessages stdoutMessages;
    private final MethodTimes methodTimes;
    private final ExecutedTests executedTests;
    private String applicableIrRules;
    private String vmInfo;

    JavaMessages() {
        this.stdoutMessages = new StdoutMessages();
        this.executedTests = new ExecutedTests();
        this.methodTimes = new MethodTimes();
        this.applicableIrRules = "";
    }

    public String applicableIRRules() {
        return applicableIrRules;
    }

    public String vmInfo() {
        return vmInfo;
    }

    void addStdoutLine(String line) {
        stdoutMessages.add(line);
    }

    void addExecutedTest(String test) {
        executedTests.add(test);
    }

    void addMethodTime(String methodTime) {
        methodTimes.add(methodTime);
    }

    void addApplicableIRRules(String applicableIrRules) {
        this.applicableIrRules = applicableIrRules;
    }

    void addVmInfo(String vmInfo) {
        this.vmInfo = vmInfo;
    }

    public void print() {
        stdoutMessages.print();
        methodTimes.print();
        executedTests.print();
        if (TestFramework.VERBOSE || PRINT_APPLICABLE_IR_RULES) {
            System.out.println("Read Applicable IR Rules from Test VM:");
            System.out.println(applicableIrRules);
        }
    }
}
