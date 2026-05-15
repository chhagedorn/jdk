package compiler.lib.ir_framework.driver.irmatching.report;

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.CheckAttributeType;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.CountsConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.FailOnConstraintFailure;
import compiler.lib.ir_framework.driver.irmatching.visitor.AcceptChildren;
import compiler.lib.ir_framework.driver.irmatching.visitor.MatchResultVisitor;

public interface FailCountVisitor extends MatchResultVisitor {
    int irMethodCount();
    int irRuleCount();

    @Override
    default void visitTestClass(AcceptChildren acceptChildren) {
        acceptChildren.accept(this);
    }

    @Override
    default void visitCompilePhaseIRRule(AcceptChildren acceptChildren, CompilePhase compilePhase, String compilationOutput) {}

    @Override
    default void visitNoCompilePhaseCompilation(CompilePhase compilePhase) {}

    @Override
    default void visitCheckAttribute(AcceptChildren acceptChildren, CheckAttributeType checkAttributeType) {}

    @Override
    default void visitFailOnConstraint(FailOnConstraintFailure failOnConstraintFailure) {}

    @Override
    default void visitCountsConstraint(CountsConstraintFailure countsConstraintFailure) {}
}
