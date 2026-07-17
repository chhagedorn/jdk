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
import compiler.lib.ir_framework.driver.irmatching.TestClassMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irmethod.IRMethodMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irmethod.NotCompilableIRMethodMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.IRRuleMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.CountsConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.FailOnConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.visitor.MatchResultVisitor;
import compiler.lib.ir_framework.shared.TestFrameworkException;

import java.lang.reflect.Method;
import java.util.Arrays;

/**
 * This class creates the complete failure message of each IR matching failure by visiting each match result element.
 */
public class FailOnSuccessfulSkipMessageBuilder implements MatchResultVisitor {
    private final FailureMessage failureMessage;
    private final MatchResult testClassMatchResult;
    private boolean noIRMethodHeader = true;
    private Method currentIRMethod;
    private boolean hasSuccess = false;

    public FailOnSuccessfulSkipMessageBuilder(MatchResult testClassMatchResult) {
        this.testClassMatchResult = testClassMatchResult;
        this.failureMessage = new FailureMessage();
    }

    @Override
    public boolean shouldVisit(MatchResult result) {
        return true;
    }

    @Override
    public void enter(TestClassMatchResult result) {
        FailCountVisitor failCountVisitor = new SuccessfulSkipCountVisitor();
        failCountVisitor.run(result);
        failureMessage.addSummary("One or more skipped IR rules were successful (should they be re-enabled?):",
                                  "Skipped successful", failCountVisitor);
    }

    @Override
    public void enter(IRMethodMatchResult result) {
        Method method = result.method();
        if (hasMethodNoSkippedIRRules(method)) {
            return;
        }
        currentIRMethod = method;
        noIRMethodHeader = true;
    }

    private static boolean hasMethodNoSkippedIRRules(Method method) {
        IR[] irAnno = method.getAnnotationsByType(IR.class);
        return Arrays.stream(irAnno).noneMatch(IR::skip);
    }

    @Override
    public void visitLeaf(NotCompilableIRMethodMatchResult result) {
        throw new TestFrameworkException("Should not reach here");
    }

    @Override
    public void visit(IRRuleMatchResult result) {
        if (!result.irAnno().skip()) {
            // No need to visit further sub results.
            return;
        }
        MatchResultVisitor.super.visit(result);
    }

    @Override
    public void enter(IRRuleMatchResult result) {
        hasSuccess = true;
    }

    @Override
    public void leave(IRRuleMatchResult result) {
        if (!hasSuccess) {
            return;
        }

        if (noIRMethodHeader) {
            failureMessage.addSuccessfulIRMethodHeader(currentIRMethod);
            noIRMethodHeader = false;
        }
        failureMessage.indent();
        failureMessage.addIRRuleHeader(result.irRuleId(), result.irAnno());
        failureMessage.dedent();
    }

    @Override
    public void visitLeaf(FailOnConstraintFailure result) {
        hasSuccess = false;
    }

    @Override
    public void visitLeaf(CountsConstraintFailure result) {
        hasSuccess = false;
    }

    public String build() {
        testClassMatchResult.accept(this);
        if (failureMessage.isEmpty()) {
            return "";
        }

        failureMessage.println()
                .println(">>> Note that non-skipped normal IR rule failures are not in this list but are shown below!")
                .println()
                .println();
        return failureMessage.build();
    }
}
