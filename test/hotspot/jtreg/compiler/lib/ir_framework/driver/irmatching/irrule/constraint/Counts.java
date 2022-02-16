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

package compiler.lib.ir_framework.driver.irmatching.irrule.constraint;

import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.shared.Comparison;

import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This class represents a fully parsed {@link IR#counts()} attribute of an IR rule for a compile phase.
 *
 * @see IR#counts()
 * @see CheckAttribute
 */
public class Counts extends CheckAttribute<CountsConstraint> {

    public Counts(List<CountsConstraint> constraints) {
        super(constraints);
    }

    @Override
    public CountsMatchResult apply(String compilation) {
        CountsMatchResult result = new CountsMatchResult();
        List<ConstraintFailure> failures = checkConstraints(compilation);
        if (!failures.isEmpty()) {
            result.setFailures(failures);
        }
        return result;
    }

    @Override
    protected void checkConstraint(List<ConstraintFailure> constraintFailures, CountsConstraint constraint, String compilation) {
        long foundCount = getFoundCount(compilation, constraint);
        Comparison<Long> comparison = constraint.getComparison();
        if (!comparison.compare(foundCount)) {
            constraintFailures.add(createRegexFailure(compilation, constraint, foundCount));
        }
    }

    private long getFoundCount(String compilation, CountsConstraint constraint) {
        Pattern pattern = Pattern.compile(constraint.getRegex());
        Matcher matcher = pattern.matcher(compilation);
        return matcher.results().count();
    }

    private CountsConstraintFailure createRegexFailure(String compilation, CountsConstraint constraint, long foundCount) {
        List<String> matches = getMatchedNodes(constraint, compilation);
        TestFramework.check(foundCount == matches.size(), "must find same number: " + foundCount + " vs. " + matches.size());
        return new CountsConstraintFailure(constraint.getRegex(), constraint.getIndex(), constraint.getComparison(), matches);
    }
}
