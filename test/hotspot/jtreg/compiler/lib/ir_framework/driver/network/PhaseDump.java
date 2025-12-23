package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.CompilePhase;

import java.util.ArrayList;
import java.util.List;

public class PhaseDump {
    private final CompilePhase compilePhase;
    private final List<String> dump;

    PhaseDump(CompilePhase compilePhase) {
        this.compilePhase = compilePhase;
        this.dump = new ArrayList<>();
    }

    void add(String line) {
        dump.add(line);
    }
}
