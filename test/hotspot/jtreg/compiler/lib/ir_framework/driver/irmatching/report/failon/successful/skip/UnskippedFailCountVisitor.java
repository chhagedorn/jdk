package compiler.lib.ir_framework.driver.irmatching.report.failon.successful.skip;

import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.driver.irmatching.report.FailCountVisitor;
import compiler.lib.ir_framework.driver.irmatching.visitor.AcceptChildren;

import java.lang.reflect.Method;
import java.util.Arrays;

public class UnskippedFailCountVisitor implements FailCountVisitor  {
    private int irMethodCount;
    private int irRuleCount;
    private boolean seenFailure;

    @Override
    public void visitIRMethod(AcceptChildren acceptChildren, Method method, int failedIRRules) {
        seenFailure = false;
        acceptChildren.accept(this);
        if (seenFailure) {
            irMethodCount++;
        }
    }


    @Override
    public void visitIRRule(AcceptChildren acceptChildren, int irRuleId, IR irAnno) {
        if (!irAnno.skip()) {
            seenFailure = true;
            irRuleCount++;
        }
        // Do not need to visit compile phase IR rules
    }

    @Override
    public void visitMethodNotCompiled(Method method, int failedIRRules) {
        irMethodCount++;
        irRuleCount += failedIRRules;
    }

    @Override
    public void visitMethodNotCompilable(Method method, int failedIRRules) {
        irMethodCount++;
    }

    @Override
    public int irRuleCount() {
        return irRuleCount;
    }

    @Override
    public int irMethodCount() {
        return irMethodCount;
    }
}
