package compiler.lib.ir_framework.driver.network;

import java.util.ArrayList;
import java.util.List;

public class ExecutedTests implements TestVmMessage {
    private final List<String> tests;

    public ExecutedTests() {
        this.tests = new ArrayList<>();
    }

    public void add(String test) {
        tests.add(test);
    }

    @Override
    public void print() {
        if (tests.isEmpty()) {
            return;
        }

        System.out.println();
        System.out.println("Executed Subset of Tests");
        System.out.println("------------------------");
        for (String test : tests) {
            System.out.println("- " + test);
        }
        System.out.println();
    }
}
