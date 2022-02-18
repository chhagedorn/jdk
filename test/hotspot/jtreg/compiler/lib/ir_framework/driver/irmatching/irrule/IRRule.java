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

import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.IRNode;
import compiler.lib.ir_framework.driver.irmatching.Matching;
import compiler.lib.ir_framework.driver.irmatching.irmethod.IRMethod;
import compiler.lib.ir_framework.driver.irmatching.irrule.phase.CompilePhaseIRRule;
import compiler.lib.ir_framework.driver.irmatching.irrule.phase.CompilePhaseIRRuleBuilder;
import compiler.lib.ir_framework.driver.irmatching.irrule.phase.CompilePhaseMatchResult;

import java.util.List;

/**
 * This class represents a generic IR rule of an IR method. It contains a list of compile phase specific IR rule
 * versions where placeholder strings of {@link IRNode} are replaced by default regexes matching the compile phase.
 *
 * @see CompilePhaseIRRule
 */
public class IRRule implements Matching {
    private final int ruleId;
    private final IR irAnno;
    private final List<CompilePhaseIRRule> compilePhaseIRRules;

    public IRRule(IRMethod irMethod, int ruleId, IR irAnno) {
        this.ruleId = ruleId;
        this.irAnno = irAnno;
        this.compilePhaseIRRules = CompilePhaseIRRuleBuilder.create(irAnno, irMethod);
    }

    public int getRuleId() {
        return ruleId;
    }

    public IR getIRAnno() {
        return irAnno;
    }

    /**
     * Apply this IR rule by checking any failOn and counts attributes.
     */
    @Override
    public IRRuleMatchResult match() {
        IRRuleMatchResult irRuleMatchResult = new IRRuleMatchResult(this);
        for (CompilePhaseIRRule compilePhaseIRRule : compilePhaseIRRules) {
            CompilePhaseMatchResult compilePhaseMatchResult = compilePhaseIRRule.match();
            if (compilePhaseMatchResult.fail()) {
                irRuleMatchResult.addCompilePhaseMatchResult(compilePhaseMatchResult);
            }
        }
        return irRuleMatchResult;
    }
}
