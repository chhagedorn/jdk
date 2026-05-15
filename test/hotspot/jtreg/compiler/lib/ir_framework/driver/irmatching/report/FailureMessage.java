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

package compiler.lib.ir_framework.driver.irmatching.report;

import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.driver.irmatching.MatchResult;

import java.lang.reflect.Method;

public class FailureMessage {
    private final StringBuilder msg;
    private final MatchResult testClassMatchResult;
    private Indentation indentation;
    private int methodIndex;


    public FailureMessage(MatchResult testClassMatchResult) {
        this.msg = new StringBuilder();
        this.methodIndex = 0;
        this.testClassMatchResult = testClassMatchResult;
    }

    public FailureMessage print(Object s) {
        msg.append(s);
        return this;
    }

    public FailureMessage printIndented(Object s) {
        msg.append(indentation).append(s);
        return this;
    }

    public FailureMessage println(Object s) {
        msg.append(s).append(System.lineSeparator());
        return this;
    }

    public FailureMessage printlnIndented(Object s) {
        msg.append(indentation).append(s).append(System.lineSeparator());
        return this;
    }

    public FailureMessage println() {
        msg.append(System.lineSeparator());
        return this;
    }

    public void indent() {
        indentation.add();
    }

    public void dedent() {
        indentation.sub();
    }

    public Indentation indentation() {
        return indentation;
    }

    public void addSummary(String title, String irRuleType, FailCountVisitor failCountVisitor) {
        testClassMatchResult.accept(failCountVisitor);
        int failedMethodCount = failCountVisitor.irMethodCount();
        int failedIRRulesCount = failCountVisitor.irRuleCount();
        msg.append(title).append(":")
                .append(System.lineSeparator())
                .append(System.lineSeparator())
                .append(irRuleType)
                .append(" IR Rules (").append(failedIRRulesCount).append(") of Methods (").append(failedMethodCount)
                .append(")").append(System.lineSeparator())
                .append(getTitleSeparator(irRuleType, failedMethodCount, failedIRRulesCount))
                .append(System.lineSeparator());
    }

    private static String getTitleSeparator(String irRuleType, int failedMethodCount, int failedIRRulesCount) {
        return "-".repeat(irRuleType.length() + 26 + digitCount(failedIRRulesCount) + digitCount(failedMethodCount));
    }

    public void addFailedIRMethodHeader(Method method, int failedIRRules) {
        addIRMethodHeader(method);
        msg.append("\" - [Failed IR rules: ").append(failedIRRules).append("]:")
        .append(System.lineSeparator());
    }

    public void addSuccessfulIRMethodHeader(Method method) {
        addIRMethodHeader(method);
        msg.append(":").append(System.lineSeparator());
    }

    private void addIRMethodHeader(Method method) {
        methodIndex++;
        indentation = new Indentation(digitCount(methodIndex));
        if (methodIndex > 1) {
            msg.append(System.lineSeparator());
        }
        msg.append(methodIndex).append(") ");
        msg.append("Method \"")
                .append(method.getDeclaringClass().getTypeName()).append("::").append(method.getName());
    }

    private static int digitCount(int digit) {
        return String.valueOf(digit).length();
    }

    public void addIRRuleHeader(int irRuleId, IR irAnno) {
        msg.append(indentation).append("* @IR rule ").append(irRuleId).append(": \"")
                .append(irAnno).append("\"").append(System.lineSeparator());
    }

    public String build() {
        return msg.toString();
    }
}
