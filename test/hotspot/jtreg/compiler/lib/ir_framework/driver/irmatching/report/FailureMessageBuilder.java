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

package compiler.lib.ir_framework.driver.irmatching.report;

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.driver.irmatching.report.failon.successful.skip.UnskippedFailCountVisitor;
import compiler.lib.ir_framework.shared.TestFrameworkException;
import compiler.lib.ir_framework.driver.irmatching.MatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.CheckAttributeType;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.CountsConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.FailOnConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.visitor.AcceptChildren;
import compiler.lib.ir_framework.driver.irmatching.visitor.MatchResultVisitor;

import java.lang.reflect.Method;

/**
 * This class creates the complete failure message of each IR matching failure by visiting each match result element.
 */
public class FailureMessageBuilder implements MatchResultVisitor {
    private static final boolean FAIL_ON_SUCCESSFUL_SKIP =
            Boolean.parseBoolean(System.getProperty("FailOnSuccessfulSkip", "false"));

    private final FailureMessage failureMessage;
    private final MatchResult testClassMatchResult;
    public FailureMessageBuilder(MatchResult testClassMatchResult) {
        this.testClassMatchResult = testClassMatchResult;
        this.failureMessage = new FailureMessage(testClassMatchResult);
    }

    @Override
    public void visitTestClass(AcceptChildren acceptChildren) {
        FailCountVisitor failCountVisitor;
        String title;
        if (FAIL_ON_SUCCESSFUL_SKIP) {
            failCountVisitor = new UnskippedFailCountVisitor();
            title = "One or more non-skipped @IR rules failed";
        } else {
            failCountVisitor = new NormalFailCountVisitor();
            title = "One or more @IR rules failed";
        }
        failureMessage.addSummary(title, "Failed", failCountVisitor);
        acceptChildren.accept(this);
    }

    @Override
    public void visitIRMethod(AcceptChildren acceptChildren, Method method, int failedIRRules) {
        failureMessage.addFailedIRMethodHeader(method, failedIRRules);
        acceptChildren.accept(this);
    }

    @Override
    public void visitMethodNotCompiled(Method method, int failedIRRules) {
        failureMessage.addFailedIRMethodHeader(method, failedIRRules);
        failureMessage.indent();
        failureMessage
           .printIndented("* Method was not compiled. Did you specify a @Run method in STANDALONE mode? In this case, " +
                                  "make sure to always trigger a C2 compilation by invoking the test enough times.");
        failureMessage.dedent();
    }

    public void visitMethodNotCompilable(Method method, int failedIRRules) {
        throw new TestFrameworkException("Should not reach here");
    }

    @Override
    public void visitIRRule(AcceptChildren acceptChildren, int irRuleId, IR irAnno) {
        if (irAnno.skip()) {
            return;
        }
        failureMessage.indent();
        failureMessage.addIRRuleHeader(irRuleId, irAnno);
        acceptChildren.accept(this);
        failureMessage.dedent();
    }

    @Override
    public void visitCompilePhaseIRRule(AcceptChildren acceptChildren, CompilePhase compilePhase, String compilationOutput) {
        failureMessage.indent();
        appendCompilePhaseIRRule(compilePhase);
        acceptChildren.accept(this);
        failureMessage.dedent();
    }

    private void appendCompilePhaseIRRule(CompilePhase compilePhase) {
        failureMessage.printIndented("> Phase \"").print(compilePhase.getName()).println("\":");
    }

    @Override
    public void visitNoCompilePhaseCompilation(CompilePhase compilePhase) {
        failureMessage.indent();
        appendCompilePhaseIRRule(compilePhase);
        failureMessage.indent();
        failureMessage
           .printIndented("- NO compilation output found for this phase! Make sure this phase is emitted or remove it from ")
           .println("the list of compile phases in the @IR rule to match on.");
        failureMessage.dedent();
        failureMessage.dedent();
    }

    @Override
    public void visitCheckAttribute(AcceptChildren acceptChildren, CheckAttributeType checkAttributeType) {
        failureMessage.indent();
        String checkAttributeFailureMsg;
        switch (checkAttributeType) {
            case FAIL_ON -> checkAttributeFailureMsg = "failOn: Graph contains forbidden nodes";
            case COUNTS -> checkAttributeFailureMsg = "counts: Graph contains wrong number of nodes";
            default ->
                    throw new IllegalStateException("Unexpected value: " + checkAttributeType);
        }
        failureMessage.printIndented("- ").print(checkAttributeFailureMsg).println(":");
        acceptChildren.accept(this);
        failureMessage.dedent();
    }

    @Override
    public void visitFailOnConstraint(FailOnConstraintFailure matchResult) {
        failureMessage.indent();
        ConstraintFailureMessageBuilder constrainFailureMessageBuilder =
                new ConstraintFailureMessageBuilder(matchResult, failureMessage.indentation());
        String msg = constrainFailureMessageBuilder.buildConstraintHeader() +
                     constrainFailureMessageBuilder.buildMatchedNodesMessage("Matched forbidden");
        failureMessage.print(msg);
        failureMessage.dedent();
    }

    @Override
    public void visitCountsConstraint(CountsConstraintFailure matchResult) {
        failureMessage.indent();
        failureMessage.print(new CountsConstraintFailureMessageBuilder(matchResult, failureMessage.indentation()).build());
        failureMessage.dedent();
    }

    public String build() {
        testClassMatchResult.accept(this);
        failureMessage.println()
           .println(">>> Check stdout for compilation output of the failed methods")
           .println();
        return failureMessage.build();
    }

}
