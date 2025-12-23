package compiler.lib.ir_framework.driver.network;

import java.util.ArrayList;
import java.util.List;

public class MethodTimes implements TestVmMessage {
    private final List<String> methodTimes;

    public MethodTimes() {
        this.methodTimes = new ArrayList<>();
    }

    public void add(String time) {
        methodTimes.add(time);
    }

    @Override
    public void print() {
        if (methodTimes.isEmpty()) {
            return;
        }
        System.out.println("Test Execution Times");
        System.out.println("--------------------");
        for (String methodTime : methodTimes) {
            System.out.println("- " + methodTime);
        }
        System.out.println();
    }
}
