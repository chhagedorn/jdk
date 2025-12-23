package compiler.lib.ir_framework.driver.network;

import java.util.HashSet;
import java.util.Set;

public class MethodDumps {
    private final Set<MethodDump> methodDumps;

    public MethodDumps() {
        this.methodDumps = new HashSet<>();
    }

    public void add(MethodDump methodDump) {
        methodDumps.add(methodDump);
    }
}
