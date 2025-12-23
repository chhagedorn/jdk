package compiler.lib.ir_framework.driver.network;

import java.util.LinkedHashSet;
import java.util.Set;

public class MethodDump {
    private final String methodName;
    private final Set<PhaseDump> phaseDumps;

    public MethodDump(String methodName) {
        this.methodName = methodName;
        this.phaseDumps = new LinkedHashSet<>();
    }

    void add(PhaseDump phaseDump) {
        phaseDumps.add(phaseDump);
    }
}
