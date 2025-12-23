package compiler.lib.ir_framework.driver.network;

import java.util.ArrayList;
import java.util.List;

public class TestVmOutput {
    private final List<String> stdoutLines;
    private final List<String> tests;
    private final List<String> printTimes;


    TestVmOutput() {
        this.stdoutLines = new ArrayList<>();
        this.tests = new ArrayList<>();
        this.printTimes = new ArrayList<>();
    }

    void addStdoutLine(String line) {
        stdoutLines.add(line);
    }

    void addTestListEntry(String test) {
        tests.add(test);
    }

    void addPrintTimes(String printTime) {
        printTimes.add(printTime);
    }
}
