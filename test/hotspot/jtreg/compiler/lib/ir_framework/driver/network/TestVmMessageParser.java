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

package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.TestFramework;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static compiler.lib.ir_framework.test.Tag.*;

class TestVmMessageParser {
    private static final Pattern TAG_PATTERN = Pattern.compile("^(\\[[^]]+])\\s*(.*)$");
    private static final TestVmLineParser<?> EMPTY_PARSER = new TestVmLineParser<>(null);

    private final TestVmMessages testVmMessages;
    private TestVmLineParser<?> testVmParser;
    private final TestVmLineParser<VmInfo> vmInfoParser;
    private final TestVmLineParser<IrEncoding> irEncodingParser;

    public TestVmMessageParser() {
        this.testVmMessages = new TestVmMessages();
        this.testVmParser = EMPTY_PARSER;
        this.vmInfoParser = new TestVmLineParser<>(new VmInfoStrategy());
        this.irEncodingParser = new TestVmLineParser<>(new IrEncodingStrategy());
    }

    public TestVmMessages testVmMessages() {
        testVmMessages.addVmInfo(vmInfoParser.output());
        testVmMessages.addIrEncoding(irEncodingParser.output());
        return testVmMessages;
    }

    public void parse(String line) {
        System.out.println(line);
        Matcher m = TAG_PATTERN.matcher(line);
        if (m.matches()) {
            String tag = m.group(1);
            String message = m.group(2);
            parseTagLine(tag, message);
        } else {
            parseLine(line);
        }
    }

    private void parseTagLine(String tag, String message) {
        switch (tag) {
            case STDOUT_TAG -> {
                assertNoActiveParser();
                testVmMessages.addStdoutLine(message);
            }
            case TEST_LIST_TAG -> {
                assertNoActiveParser();
                testVmMessages.addExecutedTest(message);
            }
            case PRINT_TIMES_TAG -> {
                assertNoActiveParser();
                testVmMessages.addMethodTime(message);
            }
            case VM_INFO -> {
                assertNoActiveParser();
                testVmParser = vmInfoParser;
            }
            case IR_ENCODING -> {
                assertNoActiveParser();
                testVmParser = irEncodingParser;
            }
        }
    }

    private void assertNoActiveParser() {
        TestFramework.check(testVmParser.equals(EMPTY_PARSER), "Unexpected new tag while parsing block");
    }

    private void parseLine(String line) {
        assertActiveParser();
        if (line.equals(END_TAG)) {
            testVmParser.finish();
            testVmParser = EMPTY_PARSER;
            return;
        }
        testVmParser.parse(line);
    }

    private void assertActiveParser() {
        TestFramework.check(!testVmParser.equals(EMPTY_PARSER), "unexpected new tag while parsing block");
    }
}
