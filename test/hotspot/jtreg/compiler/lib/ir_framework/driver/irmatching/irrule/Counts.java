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
import compiler.lib.ir_framework.shared.Comparison;

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Class representing a counts attribute of an IR rule.
 *
 * @see IR#counts()
 */
class Counts extends CheckAttribute {
    private final List<CountsConstraint> constraints;

    public Counts(List<CountsConstraint> constraints, CompilePhase compilePhase) {
        super(constraints, compilePhase);
        this.constraints = constraints;
    }


    @Override
    public CountsMatchResult apply(String compilation) {
        CountsMatchResult result = new CountsMatchResult();
        checkConstraints(result, compilation);
        return result;
    }

    private void checkConstraints(CountsMatchResult result, String compilation) {
        for (CountsConstraint constraint : constraints) {
            checkConstraint(result, compilation, constraint);
        }
    }

    private void checkConstraint(CountsMatchResult result, String compilation, CountsConstraint constraint) {
        long foundCount = getFoundCount(compilation, constraint);
        Comparison<Long> comparison = constraint.getComparison();
        if (!comparison.compare(foundCount)) {
            result.addFailure(createRegexFailure(compilation, constraint, foundCount));
        }
    }

    private long getFoundCount(String compilation, CountsConstraint constraint) {
        Pattern pattern = Pattern.compile(constraint.getNode());
        Matcher matcher = pattern.matcher(compilation);
        return matcher.results().count();
    }

    private CountsRegexFailure createRegexFailure(String compilation, CountsConstraint constraint, long foundCount) {
        Pattern p = Pattern.compile(constraint.getNode());
        Matcher m = p.matcher(compilation);
        List<String> matches;
        if (m.find()) {
            matches = getMatchedNodes(m);
        } else {
            matches = new ArrayList<>();
        }
        return new CountsRegexFailure(constraint.getNode(), constraint.getRegexNodeId(), foundCount,
                                      constraint.getComparison(), matches);
    }
}
