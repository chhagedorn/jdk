package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.TestFramework;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

public class IrEncoding implements TestVmMessage {
    private static final boolean PRINT_IR_ENCODING = Boolean.parseBoolean(System.getProperty("PrintIREncoding", "false"))
                                                     || TestFramework.VERBOSE;

    private final Map<String, List<Integer>> methods;

    public IrEncoding() {
        methods = new HashMap<>();
    }

    public void add(String method, List<Integer> irRuleIds) {
        methods.put(method, irRuleIds);
    }

    public Map<String, List<Integer>> methods() {
        return methods;
    }

    @Override
    public void print() {
        if (!PRINT_IR_ENCODING) {
            return;
        }

        System.out.println();
        System.out.println("IR Encoding");
        System.out.println("-----------");
        for (var entry : methods.entrySet()) {
            String method = entry.getKey();
            String ruleIds = entry.getValue().stream().map(String::valueOf).collect(Collectors.joining(", "));
            System.out.println("- " + method + ": " + ruleIds);
        }
    }
}
