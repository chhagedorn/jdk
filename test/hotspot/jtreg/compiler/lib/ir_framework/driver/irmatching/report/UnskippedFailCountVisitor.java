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

import compiler.lib.ir_framework.driver.irmatching.MatchResult;
import compiler.lib.ir_framework.driver.irmatching.irmethod.IRMethodMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irmethod.NotCompilableIRMethodMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irmethod.NotCompiledIRMethodMatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.IRRuleMatchResult;

public class UnskippedFailCountVisitor implements FailCountVisitor  {
    private int irMethodCount;
    private int irRuleCount;
    private boolean seenFailure;

    @Override
    public int irRuleCount() {
        return irRuleCount;
    }

    @Override
    public int irMethodCount() {
        return irMethodCount;
    }

    public void run(MatchResult matchResult) {
        irMethodCount = 0;
        irRuleCount = 0;
        matchResult.accept(this);
    }

    @Override
    public void enter(IRMethodMatchResult result) {
        seenFailure = false;
    }

    @Override
    public void leave(IRMethodMatchResult result) {
        if (seenFailure) {
            irMethodCount++;
        }
    }

    /**
     * Directly override visit to not visit compile phase IR rules sub results.
     */
    @Override
    public void visit(IRRuleMatchResult result) {
        if (!result.irAnno().skip()) {
            // We only count the failures at unskipped IR rules.
            seenFailure = true;
            irRuleCount++;
        }
    }

    @Override
    public void visitLeaf(NotCompiledIRMethodMatchResult result) {
        irMethodCount++;
        irRuleCount += result.irRuleCount();
    }

    @Override
    public void visitLeaf(NotCompilableIRMethodMatchResult result) {
        irMethodCount++;
    }
}
