package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.shared.TestFrameworkException;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static compiler.lib.ir_framework.test.Tag.*;

class TestVmMessageParser {
    private static final Pattern TAG_PATTERN = Pattern.compile("^(\\[[^]]+])\\s*(.*)$", Pattern.DOTALL);

    private final TestVmMessages testVmMessages;
    private VmInfo vmInfo;
    private IrEncoding irEncoding;

    public TestVmMessageParser() {
        this.testVmMessages = new TestVmMessages();
        this.vmInfo = new VmInfo(); // TODO: Default?
        this.irEncoding = new IrEncoding(); // TODO: Default:
    }

    public TestVmMessages testVmMessages() {
        testVmMessages.addVmInfo(vmInfo);
        testVmMessages.addIrEncoding(irEncoding);
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
            throw new TestFrameworkException("message without tag");
        }
    }

    private void parseTagLine(String tag, String message) {
        switch (tag) {
            case STDOUT_TAG -> {
                testVmMessages.addStdoutLine(message);
            }
            case TEST_LIST_TAG -> {
                testVmMessages.addExecutedTest(message);
            }
            case PRINT_TIMES_TAG -> {
                testVmMessages.addMethodTime(message);
            }
            case VM_INFO -> {
                VmInfoParser vmInfoParser = new VmInfoParser();
                vmInfo = vmInfoParser.parse(message);
            }
            case IR_ENCODING -> {
                IrEncodingParser irEncodingParser = new IrEncodingParser();
                irEncoding = irEncodingParser.parse(message);
            }
        }
    }
}
