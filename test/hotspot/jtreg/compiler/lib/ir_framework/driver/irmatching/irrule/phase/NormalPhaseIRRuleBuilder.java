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


package compiler.lib.ir_framework.driver.irmatching.irrule.phase;

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.driver.irmatching.irmethod.IRMethod;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.Constraint;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.Counts;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.CountsConstraint;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.FailOn;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.parser.RawConstraint;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.parser.RawCountsConstraint;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.parser.RawCountsConstraintParser;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.parser.RawFailOnConstraintParser;

import java.util.List;

/**
 * Class to build a {@link CompilePhaseIRRule} instance for a non-default compile phase.
 *
 * @see CompilePhaseIRRule
 */
class NormalPhaseIRRuleBuilder extends CompilePhaseIRRuleBuilder {

    public NormalPhaseIRRuleBuilder(List<RawConstraint> rawFailOnConstraints, List<RawCountsConstraint> rawCountsConstraints, IRMethod irMethod) {
        super(rawFailOnConstraints, rawCountsConstraints, irMethod);
    }

    public CompilePhaseIRRule create(CompilePhase compilePhase) {
        List<Constraint> failOnConstraints = RawFailOnConstraintParser.parse(rawFailOnConstraints, compilePhase);
        List<CountsConstraint> countsConstraints = RawCountsConstraintParser.parse(rawCountsConstraints, compilePhase);
        return createCompilePhaseIRRule(compilePhase, failOnConstraints, countsConstraints);
    }
    private CompilePhaseIRRule createCompilePhaseIRRule(CompilePhase compilePhase, List<Constraint> failOnConstraints,
                                                          List<CountsConstraint> countsConstraints) {
        FailOn failOn = createFailOn(failOnConstraints, compilePhase);
        Counts counts = createCounts(countsConstraints, compilePhase);
        return new CompilePhaseIRRule(compilePhase, failOn, counts);
    }
}
