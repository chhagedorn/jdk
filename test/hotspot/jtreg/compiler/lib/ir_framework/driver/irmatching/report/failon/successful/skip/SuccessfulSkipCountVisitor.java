/*
 * Copyright (c) 2022, 2025, Oracle and/or its affiliates. All rights reserved.
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

import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.driver.irmatching.report.FailCountVisitor;
import compiler.lib.ir_framework.driver.irmatching.visitor.AcceptChildren;

import java.lang.reflect.Method;
import java.util.Arrays;

/**
 * Visitor to collect the number of IR method and IR rule failures.
 */
public class SuccessfulSkipCountVisitor implements FailCountVisitor {
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
        if (irAnno.skip()) {
            seenFailure = true;
            irRuleCount++;
        }
        // Do not need to visit compile phase IR rules
    }

    @Override
    public void visitMethodNotCompiled(Method method, int failedIRRules) {}

    @Override
    public void visitMethodNotCompilable(Method method, int failedIRRules) {}

    @Override
    public int irRuleCount() {
        return irRuleCount;
    }

    @Override
    public int irMethodCount() {
        return irMethodCount;
    }
}
