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
import compiler.lib.ir_framework.driver.irmatching.IRMatcher;
import compiler.lib.ir_framework.driver.irmatching.TestClassMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irmethod.IRMethodMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irmethod.NotCompilableIRMethodMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irmethod.NotCompiledIRMethodMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.IRRuleMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.CheckAttributeMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.phase.CompilePhaseIRRuleMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.phase.CompilePhaseNoCompilationIRRuleMatchResult;
import compiler.lib.ir_framework.shared.TestFrameworkException;
import compiler.lib.ir_framework.driver.irmatching.MatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.CheckAttributeType;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.CountsConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.FailOnConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.visitor.MatchResultVisitor;

/**
 * This class creates the complete failure message of each IR matching failure by visiting each match result element.
 */
public class FailureMessageBuilder implements MatchResultVisitor {
    private final FailureMessage failureMessage;
    private final MatchResult testClassMatchResult;
    private final FailCountVisitor failCountVisitor;

    public FailureMessageBuilder(MatchResult testClassMatchResult) {
        this.testClassMatchResult = testClassMatchResult;
        this.failureMessage = new FailureMessage();
        this.failCountVisitor = IRMatcher.FAIL_ON_SUCCESSFUL_SKIP ?
                new UnskippedFailCountVisitor() : new NormalFailCountVisitor();
    }

    @Override
    public void enter(TestClassMatchResult result) {
        failCountVisitor.run(result);
        String title;
        if (IRMatcher.FAIL_ON_SUCCESSFUL_SKIP) {
            if (failCountVisitor.irMethodCount() == 0) {
                // Only skipped IR rules failed - nothing to report normally.
                return;
            }
            title = "One or more non-skipped @IR rules failed";
        } else {
            title = "One or more @IR rules failed";
        }
        failureMessage.addSummary(title, "Failed", failCountVisitor);
    }

    @Override
    public void enter(IRMethodMatchResult result) {
        if (IRMatcher.FAIL_ON_SUCCESSFUL_SKIP) {
            failCountVisitor.run(result);
            if (failCountVisitor.irMethodCount() == 0) {
                return;
            }
        }
        failureMessage.addFailedIRMethodHeader(result.method(), result.subResults().failCount());
    }

    @Override
    public void visitLeaf(NotCompiledIRMethodMatchResult result) {
        failureMessage.addFailedIRMethodHeader(result.method(), result.irRuleCount());
        failureMessage.indent();
        failureMessage
                .printIndented("* Method was not compiled. Did you specify a @Run method in STANDALONE mode? In this case, " +
                               "make sure to always trigger a C2 compilation by invoking the test enough times.");
        failureMessage.dedent();
    }

    @Override
    public void visitLeaf(NotCompilableIRMethodMatchResult result) {
        throw new TestFrameworkException("Should not reach here");
    }

    @Override
    public void visit(IRRuleMatchResult result) {
        if (IRMatcher.IGNORE_SKIP_IR && result.irAnno().skip()) {
            // No need to visit further sub results.
            return;
        }
        if (IRMatcher.FAIL_ON_SUCCESSFUL_SKIP) {
            failCountVisitor.run(result);
            if (failCountVisitor.irRuleCount() == 0) {
                return;
            }
        }
        MatchResultVisitor.super.visit(result);
    }

    @Override
    public void enter(IRRuleMatchResult result) {
        failureMessage.indent();
        failureMessage.addIRRuleHeader(result.irRuleId(), result.irAnno());
    }

    @Override
    public void leave(IRRuleMatchResult result) {
        failureMessage.dedent();
    }

    @Override
    public void enter(CompilePhaseIRRuleMatchResult result) {
        failureMessage.indent();
        appendCompilePhaseIRRule(result.compilePhase());
    }

    @Override
    public void leave(CompilePhaseIRRuleMatchResult result) {
        failureMessage.dedent();
    }

    @Override
    public void visitLeaf(CompilePhaseNoCompilationIRRuleMatchResult result) {
        failureMessage.indent();
        appendCompilePhaseIRRule(result.compilePhase());
        failureMessage.indent();
        failureMessage
                .printIndented("- NO compilation output found for this phase! Make sure this phase is emitted or remove it from ")
                .println("the list of compile phases in the @IR rule to match on.");
        failureMessage.dedent();
        failureMessage.dedent();
    }

    private void appendCompilePhaseIRRule(CompilePhase compilePhase) {
        failureMessage.printIndented("> Phase \"").print(compilePhase.getName()).println("\":");
    }

    @Override
    public void enter(CheckAttributeMatchResult result) {
        failureMessage.indent();
        CheckAttributeType checkAttributeType = result.checkAttributeType();
        String checkAttributeFailureMsg;
        switch (checkAttributeType) {
            case FAIL_ON -> checkAttributeFailureMsg = "failOn: Graph contains forbidden nodes";
            case COUNTS -> checkAttributeFailureMsg = "counts: Graph contains wrong number of nodes";
            default ->
                    throw new IllegalStateException("Unexpected value: " + checkAttributeType);
        }
        failureMessage.printIndented("- ").print(checkAttributeFailureMsg).println(":");
    }

    @Override
    public void leave(CheckAttributeMatchResult result) {
        failureMessage.dedent();
    }

    @Override
    public void visitLeaf(FailOnConstraintFailure result) {
        failureMessage.indent();
        ConstraintFailureMessageBuilder constrainFailureMessageBuilder =
                new ConstraintFailureMessageBuilder(result, failureMessage.indentation());
        String msg = constrainFailureMessageBuilder.buildConstraintHeader() +
                     constrainFailureMessageBuilder.buildMatchedNodesMessage("Matched forbidden");
        failureMessage.print(msg);
        failureMessage.dedent();
    }

    @Override
    public void visitLeaf(CountsConstraintFailure result) {
        failureMessage.indent();
        failureMessage.print(new CountsConstraintFailureMessageBuilder(result, failureMessage.indentation()).build());
        failureMessage.dedent();
    }

    public String build() {
        testClassMatchResult.accept(this);
        if (IRMatcher.FAIL_ON_SUCCESSFUL_SKIP && failureMessage.isEmpty()) {
            return "";
        }
        failureMessage.println()
                .println(">>> Check stdout for compilation output of the failed methods")
                .println();
        return failureMessage.build();
    }
}
