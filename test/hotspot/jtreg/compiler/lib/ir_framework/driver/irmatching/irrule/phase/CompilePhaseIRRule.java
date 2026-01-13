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
import compiler.lib.ir_framework.IRNode;
import compiler.lib.ir_framework.driver.irmatching.MatchResult;
import compiler.lib.ir_framework.driver.irmatching.Matchable;
import compiler.lib.ir_framework.driver.irmatching.MatchableMatcher;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.Counts;
import compiler.lib.ir_framework.driver.irmatching.irrule.checkattribute.FailOn;
import compiler.lib.ir_framework.driver.network.testvm.hotspot.PhaseDump;

import java.util.List;

/**
 * This class represents an IR rule of an IR method for a specific compile phase. It contains fully parsed (i.e.
 * all placeholder strings of {@link IRNode} replaced and composite nodes merged) {@link FailOn} and/or {@link Counts}
 * check attributes which are ready to be matched on.
 *
 * @see FailOn
 * @see Counts
 * @see CompilePhaseIRRuleMatchResult
 */
public class CompilePhaseIRRule implements CompilePhaseIRRuleMatchable {
    private final CompilePhase compilePhase;
    private final MatchableMatcher matcher;
    private final PhaseDump phaseDump;

    public CompilePhaseIRRule(List<Matchable> checkAttributes, PhaseDump phaseDump) {
        this.compilePhase = phaseDump.compilePhase();
        this.matcher = new MatchableMatcher(checkAttributes);
        this.phaseDump = phaseDump;
    }

    @Override
    public MatchResult match() {
        return new CompilePhaseIRRuleMatchResult(phaseDump, matcher.match());
    }

    @Override
    public CompilePhase compilePhase() {
        return compilePhase;
    }
}
