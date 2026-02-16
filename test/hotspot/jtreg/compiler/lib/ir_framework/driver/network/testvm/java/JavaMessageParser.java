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

import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.test.network.MessageTag;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static compiler.lib.ir_framework.test.network.MessageTag.*;

/**
 * Dedicated parser for {@link JavaMessages} received from the Test VM. Depending on the parsed {@link MessageTag}, the
 * message is parsed differently.
 */
public class JavaMessageParser {
    private static final Pattern TAG_PATTERN = Pattern.compile("^(\\[[^]]+])\\s*(.*)$");
    private static final StringBuilder EMPTY_BUILDER = new StringBuilder();

    private final JavaMessages javaMessages;
    private StringBuilder currentBuilder;
    private final StringBuilder vmInfoBuilder;
    private final StringBuilder applicableIrRules;

    public JavaMessageParser() {
        this.javaMessages = new JavaMessages();
        this.currentBuilder = EMPTY_BUILDER;
        this.vmInfoBuilder = new StringBuilder();
        this.applicableIrRules = new StringBuilder();
    }

    public void parseLine(String line) {
        line = line.trim();
        Matcher tagLineMatcher = TAG_PATTERN.matcher(line);
        if (tagLineMatcher.matches()) {
            assertNoActiveParser();
            parseTagLine(tagLineMatcher);
            return;
        }
        assertActiveParser();
        if (line.equals(END_MARKER)) {
            parseEndTag();
            return;
        }
        currentBuilder.append(line).append(System.lineSeparator());
    }

    private void assertNoActiveParser() {
        TestFramework.check(currentBuilder == EMPTY_BUILDER, "Unexpected new tag while parsing block");
    }

    private void parseTagLine(Matcher tagLineMatcher) {
        String tag = tagLineMatcher.group(1);
        String message = tagLineMatcher.group(2);
        switch (tag) {
            case STDOUT -> javaMessages.addStdoutLine(message);
            case TEST_LIST -> javaMessages.addExecutedTest(message);
            case PRINT_TIMES -> javaMessages.addMethodTime(message);
            case VM_INFO -> currentBuilder = vmInfoBuilder;
            case APPLICABLE_IR_RULES -> currentBuilder = applicableIrRules;
        }
    }

    private void assertActiveParser() {
        TestFramework.check(currentBuilder != EMPTY_BUILDER, "unexpected new tag while parsing block");
    }

    private void parseEndTag() {
        currentBuilder = EMPTY_BUILDER;
    }

    public JavaMessages output() {
        javaMessages.addVmInfo(vmInfoBuilder.toString());
        javaMessages.addApplicableIRRules(applicableIrRules.toString());
        return javaMessages;
    }
}
