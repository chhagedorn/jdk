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

package compiler.lib.ir_framework.driver.irmatching.irrule.phase;

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.IRNode;
import compiler.lib.ir_framework.driver.irmatching.Matchable;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.Counts;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.FailOn;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.parsing.RawCounts;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.parsing.RawFailOn;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.Constraint;
import compiler.lib.ir_framework.driver.irmatching.irrule.constraint.raw.RawConstraint;
import compiler.lib.ir_framework.driver.network.testvm.c2.MethodDumpHistory;
import compiler.lib.ir_framework.driver.network.testvm.c2.CompilePhaseDump;
import compiler.lib.ir_framework.driver.network.testvm.java.VMInfo;
import compiler.lib.ir_framework.shared.TestFormat;

import java.util.*;

/**
 * This class creates a list of {@link CompilePhaseIRRule} for each specified compile phase in {@link IR#phase()} of an
 * IR rule. Default compile phases of {@link IRNode} placeholder strings as found in {@link RawConstraint} objects are
 * replaced by the actual default phases. The resulting parsed {@link Constraint} objects which now belong to a
 * non-default compile phase are moved to the check attribute matchables which represent these compile phases.
 *
 * @see CompilePhaseIRRule
 */
public class CompilePhaseIrRuleBuilder {
    private final IR irAnno;
    private final List<RawConstraint> rawFailOnConstraints;
    private final List<RawConstraint> rawCountsConstraints;
    private final MethodDumpHistory methodDumpHistory;
    private final SortedSet<CompilePhaseIRRuleMatchable> compilePhaseIrRules = new TreeSet<>();

    public CompilePhaseIrRuleBuilder(IR irAnno, MethodDumpHistory methodDumpHistory) {
        this.irAnno = irAnno;
        this.methodDumpHistory = methodDumpHistory;
        this.rawFailOnConstraints = new RawFailOn(irAnno.failOn()).createRawConstraints();
        this.rawCountsConstraints = new RawCounts(irAnno.counts()).createRawConstraints();
    }

    public SortedSet<CompilePhaseIRRuleMatchable> build(VMInfo vmInfo) {
        CompilePhase[] compilePhases = irAnno.phase();
        TestFormat.checkNoReport(new HashSet<>(List.of(compilePhases)).size() == compilePhases.length,
                                 "Cannot specify a compile phase twice");
        for (CompilePhase compilePhase : compilePhases) {
            if (compilePhase == CompilePhase.DEFAULT) {
                createCompilePhaseIrRulesForDefault(vmInfo);
            } else {
                CompilePhaseDump compilePhaseDump = methodDumpHistory.methodDump(compilePhase);
                createCompilePhaseIrRule(compilePhaseDump, vmInfo);
            }
        }
        return compilePhaseIrRules;
    }

    private void createCompilePhaseIrRulesForDefault(VMInfo vmInfo) {
        DefaultPhaseRawConstraintParser parser = new DefaultPhaseRawConstraintParser(methodDumpHistory);
        Map<CompilePhase, List<Matchable>> checkAttributesForCompilePhase =
                parser.parse(rawFailOnConstraints, rawCountsConstraints, vmInfo);
        checkAttributesForCompilePhase.forEach((compilePhase, constraints) -> {
            CompilePhaseDump compilePhaseDump = methodDumpHistory.methodDump(compilePhase);
            if (compilePhaseDump.isEmpty()) {
                compilePhaseIrRules.add(new CompilePhaseNoCompilationIRRule(compilePhase));
            } else {
                compilePhaseIrRules.add(new CompilePhaseIRRule(constraints, compilePhaseDump));
            }
        });
    }

    private void createCompilePhaseIrRule(CompilePhaseDump compilePhaseDump, VMInfo vmInfo) {
        List<Constraint> failOnConstraints = parseRawConstraints(rawFailOnConstraints, compilePhaseDump, vmInfo);
        List<Constraint> countsConstraints = parseRawConstraints(rawCountsConstraints, compilePhaseDump, vmInfo);
        if (compilePhaseDump.isEmpty()) {
            compilePhaseIrRules.add(new CompilePhaseNoCompilationIRRule(compilePhaseDump.compilePhase()));
        } else {
            createValidCompilePhaseIRRule(compilePhaseDump, failOnConstraints, countsConstraints);
        }
    }

    private void createValidCompilePhaseIRRule(CompilePhaseDump compilePhaseDump, List<Constraint> failOnConstraints,
                                               List<Constraint> countsConstraints) {
        String compilationOutput = compilePhaseDump.dump();
        List<Matchable> checkAttributes = new ArrayList<>();
        if (!failOnConstraints.isEmpty()) {
            checkAttributes.add(new FailOn(failOnConstraints, compilationOutput));
        }

        if (!countsConstraints.isEmpty()) {
            checkAttributes.add(new Counts(countsConstraints));
        }
        compilePhaseIrRules.add(new CompilePhaseIRRule(checkAttributes, compilePhaseDump));
    }

    private List<Constraint> parseRawConstraints(List<RawConstraint> rawConstraints,
                                                 CompilePhaseDump compilePhaseDump,
                                                 VMInfo vmInfo) {
        List<Constraint> constraintResultList = new ArrayList<>();
        for (RawConstraint rawConstraint : rawConstraints) {
            constraintResultList.add(rawConstraint.parse(compilePhaseDump, vmInfo));
        }
        return constraintResultList;
    }
}
