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
import compiler.lib.ir_framework.driver.network.testvm.TestVmMessageParser;
import compiler.lib.ir_framework.driver.network.testvm.java.multiline.ApplicableIRRulesStrategy;
import compiler.lib.ir_framework.driver.network.testvm.java.multiline.MultiLineParser;
import compiler.lib.ir_framework.driver.network.testvm.java.multiline.VMInfoStrategy;
import compiler.lib.ir_framework.test.network.MessageTag;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static compiler.lib.ir_framework.test.network.MessageTag.*;

/**
 * Dedicated parser for {@link JavaMessages} received from the Test VM. Depending on the parsed{@link MessageTag}, the
 * message is parsed differently.
 */
public class JavaMessageParser implements TestVmMessageParser<JavaMessages> {
    private static final Pattern TAG_PATTERN = Pattern.compile("^(\\[[^]]+])\\s*(.*)$");
    private static final MultiLineParser<? extends JavaMessage> EMPTY_PARSER = new MultiLineParser<>(null);

    private final JavaMessages javaMessages;
    private MultiLineParser<? extends JavaMessage> multiLineParser;
    private final MultiLineParser<VMInfo> vmInfoParser;
    private final MultiLineParser<ApplicableIRRules> applicableIRRulesParser;

    public JavaMessageParser() {
        this.javaMessages = new JavaMessages();
        this.multiLineParser = EMPTY_PARSER;
        this.vmInfoParser = new MultiLineParser<>(new VMInfoStrategy());
        this.applicableIRRulesParser = new MultiLineParser<>(new ApplicableIRRulesStrategy());
    }

    @Override
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
        multiLineParser.parseLine(line);
    }

    private void assertNoActiveParser() {
        TestFramework.check(multiLineParser.equals(EMPTY_PARSER), "Unexpected new tag while parsing block");
    }

    private void parseTagLine(Matcher tagLineMatcher) {
        String tag = tagLineMatcher.group(1);
        String message = tagLineMatcher.group(2);
        switch (tag) {
            case STDOUT -> javaMessages.addStdoutLine(message);
            case TEST_LIST -> javaMessages.addExecutedTest(message);
            case PRINT_TIMES -> javaMessages.addMethodTime(message);
            case VM_INFO -> multiLineParser = vmInfoParser;
            case APPLICABLE_IR_RULES -> multiLineParser = applicableIRRulesParser;
        }
    }

    private void assertActiveParser() {
        TestFramework.check(!multiLineParser.equals(EMPTY_PARSER), "unexpected new tag while parsing block");
    }

    private void parseEndTag() {
        multiLineParser.markFinished();
        multiLineParser = EMPTY_PARSER;
    }

    @Override
    public JavaMessages output() {
        javaMessages.addVmInfo(vmInfoParser.output());
        javaMessages.addApplicableIRRules(applicableIRRulesParser.output());
        return javaMessages;
    }
}
