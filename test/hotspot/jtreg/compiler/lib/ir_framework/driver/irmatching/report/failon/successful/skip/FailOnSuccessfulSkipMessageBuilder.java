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

package compiler.lib.ir_framework.driver.irmatching.report.failon.successful.skip;

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.driver.irmatching.MatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.CheckAttributeType;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.CountsConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.FailOnConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.report.FailureMessage;
import compiler.lib.ir_framework.driver.irmatching.visitor.AcceptChildren;
import compiler.lib.ir_framework.driver.irmatching.visitor.MatchResultVisitor;
import compiler.lib.ir_framework.shared.TestFrameworkException;

import java.lang.reflect.Method;
import java.util.Arrays;

/**
 * This class creates the complete failure message of each IR matching failure by visiting each match result element.
 */
public class FailOnSuccessfulSkipMessageBuilder implements MatchResultVisitor {
    private static final boolean FAIL_ON_SUCCESSFUL_SKIP =
            Boolean.parseBoolean(System.getProperty("FailOnSuccessfulSkip", "false"));

    private final FailureMessage failureMessage;
    private final MatchResult testClassMatchResult;
    private boolean hasNoFailures = true;

    public FailOnSuccessfulSkipMessageBuilder(MatchResult testClassMatchResult) {
        this.testClassMatchResult = testClassMatchResult;
        this.failureMessage = new FailureMessage(testClassMatchResult);
    }

    @Override
    public void visitTestClass(AcceptChildren acceptChildren) {
        failureMessage.addSummary("One or more skipped IR rules were successful (should they be re-enabled?):",
                                  "Skipped successful", new SuccessfulSkipCountVisitor());
        acceptChildren.accept(this);
    }

    @Override
    public void visitIRMethod(AcceptChildren acceptChildren, Method method, int failedIRRules) {
        if (hasMethodNoSkippedIRRules(method)) {
            return;
        }

        hasNoFailures = false;
        failureMessage.addSuccessfulIRMethodHeader(method);
        acceptChildren.accept(this);
    }

    private static boolean hasMethodNoSkippedIRRules(Method method) {
        if (!FAIL_ON_SUCCESSFUL_SKIP) {
            return true;
        }

        IR[] irAnno = method.getAnnotationsByType(IR.class);
        return Arrays.stream(irAnno).noneMatch(IR::skip);
    }

    @Override
    public void visitMethodNotCompiled(Method method, int failedIRRules) {}

    @Override
    public void visitMethodNotCompilable(Method method, int failedIRRules) {
        throw new TestFrameworkException("Should not reach here");
    }

    @Override
    public void visitIRRule(AcceptChildren acceptChildren, int irRuleId, IR irAnno) {
        if (irAnno.skip()) {
            failureMessage.indent();
            failureMessage.addIRRuleHeader(irRuleId, irAnno);
            failureMessage.dedent();
        }
    }

    @Override
    public void visitCompilePhaseIRRule(AcceptChildren acceptChildren, CompilePhase compilePhase, String compilationOutput) {
        acceptChildren.accept(this);
    }

    @Override
    public void visitNoCompilePhaseCompilation(CompilePhase compilePhase) {}

    @Override
    public void visitCheckAttribute(AcceptChildren acceptChildren, CheckAttributeType checkAttributeType) {}

    @Override
    public void visitFailOnConstraint(FailOnConstraintFailure failOnConstraintFailure) {}

    @Override
    public void visitCountsConstraint(CountsConstraintFailure countsConstraintFailure) {}

    public String build() {
        testClassMatchResult.accept(this);
        if (hasNoFailures) {
            return "";
        }

        failureMessage.println()
                .println(">>> Note that non-skipped normal IR rule failures are not in this list but are shown below!")
                .println()
                .println();
        return failureMessage.build();
    }
}
