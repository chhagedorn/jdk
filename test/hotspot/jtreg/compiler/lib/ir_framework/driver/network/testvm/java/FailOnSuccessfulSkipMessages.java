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

package compiler.lib.ir_framework.driver.network.testvm.java;

import compiler.lib.ir_framework.test.network.MessageTag;

import java.util.List;


/**
 * Class to collect all Java Messages sent with tag {@link MessageTag#FAILED_SKIP_ANNOTATED_TESTS},
 * {@link MessageTag#FAILED_NON_SKIP_ANNOTATED_TESTS}, and {@link MessageTag#SUCCESSFUL_SKIP_ANNOTATED_TESTS}.
 * These are only used when the user runs with {@code -DFailOnSuccessfulSkip=true}.
 */
class FailOnSuccessfulSkipMessages implements JavaMessage {
    private final List<String> failedSkipAnnotatedTests;
    private final List<String> failedNonSkipAnnotatedTests;
    private final List<String> successfulSkipAnnotatedTests;

    public FailOnSuccessfulSkipMessages(List<String> failedSkipAnnotatedTests,
                                        List<String> failedNonSkipAnnotatedTests,
                                        List<String> successfulSkipAnnotatedTests) {
        this.failedSkipAnnotatedTests = failedSkipAnnotatedTests;
        this.failedNonSkipAnnotatedTests = failedNonSkipAnnotatedTests;
        this.successfulSkipAnnotatedTests = successfulSkipAnnotatedTests;
    }

    @Override
    public void print() {
        if (failedSkipAnnotatedTests.isEmpty() &&
            failedNonSkipAnnotatedTests.isEmpty() &&
            successfulSkipAnnotatedTests.isEmpty()) {
            return;
        }

        // It is tempting to assert here that -DFailOnSuccessfulSkip is set but this won't work when the user only
        // passes -DFailOnSuccessfulSkip=true with TestFramework.runWithFlags() but does not run the Driver VM with it.

        System.out.println();
        print("(Unexpected) successful @Skip-annotated @Test or associated @Run methods", successfulSkipAnnotatedTests);
        print("(Unexpected) failed non-@Skip-annotated @Test or associated @Run methods", failedNonSkipAnnotatedTests);
        print("(Expected) failed @Skip-annotated @Test methods or associated @Run methods", failedSkipAnnotatedTests);
        System.out.println();
    }

    private void print(String title, List<String> tests) {
        int count = tests.size();
        if (count == 0) {
            return;
        }

        tests.sort(String.CASE_INSENSITIVE_ORDER);
        System.out.println(title + " (" + count + ")");
        System.out.println("-".repeat(title.length() + String.valueOf(count).length() + 3));

        for (String method : tests) {
            System.out.println("- " + method);
        }
        System.out.println();
    }

    boolean foundNonFailingSkippedTests() {
        return !successfulSkipAnnotatedTests.isEmpty();
    }
}
