package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.TestFramework;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static compiler.lib.ir_framework.test.Tag.*;

class TestVmMessageParser {
    private static final Pattern TAG_PATTERN = Pattern.compile("^(\\[[^]]+])\\s*(.*)$");
    private static final TestVmParser<?> EMPTY_PARSER = new TestVmParser<>() { // TODO: Separate class?
        @Override
        public void parse(String line) {
        }

        @Override
        public void finish() {
        }

        @Override
        public Object output() {
            return null;
        }
    };

    private final TestVmMessages testVmMessages;
    private TestVmParser<?> testVmParser;
    private final VmInfoParser vmInfoParser;
    private final IrEncodingParser irEncodingParser;

    public TestVmMessageParser() {
        this.testVmMessages = new TestVmMessages();
        this.testVmParser = EMPTY_PARSER;
        this.vmInfoParser = new VmInfoParser();
        this.irEncodingParser = new IrEncodingParser();
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
