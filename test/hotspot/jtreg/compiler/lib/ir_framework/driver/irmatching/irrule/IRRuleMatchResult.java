/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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

package compiler.lib.ir_framework.driver.irmatching.irrule;

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.driver.irmatching.MatchResult;
import compiler.lib.ir_framework.driver.irmatching.irrule.phase.CompilePhaseIRRuleMatchResult;
import compiler.lib.ir_framework.driver.irmatching.visitor.AcceptChildren;
import compiler.lib.ir_framework.driver.irmatching.visitor.MatchResultVisitor;

import java.util.List;

/**
 * This class represents a match result of an {@link IRRule} (applied to all compile phases specified in
 * {@link IR#phase()}). The {@link CompilePhaseIRRuleMatchResult} are kept in the definition order of the compile phases
 * in {@link CompilePhase}.
 *
 * @see IRRule
 */
public class IRRuleMatchResult implements MatchResult {
    private static final boolean FAIL_ON_SUCCESSFUL_SKIP =
            Boolean.parseBoolean(System.getProperty("FailOnSuccessfulSkip", "false"));

    private final AcceptChildren acceptChildren;
    private final boolean failed;
    private final int irRuleId;
    private final IR irAnno;

    public IRRuleMatchResult(int irRuleId, IR irAnno, List<MatchResult> matchResults) {
        this.acceptChildren = new AcceptChildren(matchResults);
        this.failed = isFail(matchResults, irAnno);
        this.irRuleId = irRuleId;
        this.irAnno = irAnno;
    }

    private boolean isFail(List<MatchResult> matchResults, IR irAnno) {
        boolean failed = !matchResults.isEmpty();
        if (!FAIL_ON_SUCCESSFUL_SKIP) {
            return failed;
        }
        if (irAnno.skip()) {
            // When using -DFailOnSuccessfulSkip, we treat a failure of a skipped IR rule as success and vice versa.
            return !failed;
        }
        // TODO: All other results are ignored in this mode.
        return failed;
    }

    @Override
    public boolean fail() {
        return failed;
    }

    @Override
    public void accept(MatchResultVisitor visitor) {
        visitor.visitIRRule(acceptChildren, irRuleId, irAnno);
    }
}
