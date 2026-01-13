package compiler.lib.ir_framework.driver.network.testvm.hotspot;

import compiler.lib.ir_framework.CompilePhase;

import java.util.ArrayList;
import java.util.List;

public class MethodDumpHistory {
    private static final MethodDumpHistory EMPTY = new MethodDumpHistory();
    private final List<MethodDump> dumps;

    public MethodDumpHistory() {
        this.dumps = new ArrayList<>();
    }

    public static MethodDumpHistory createEmpty() {
        return EMPTY;
    }

    public boolean isEmpty() {
        return equals(EMPTY);
    }

    public void add(MethodDump methodDump) {
        dumps.add(methodDump);
    }

    public PhaseDump methodDump(CompilePhase compilePhase) {
        MethodDump methodDump = dumps.getLast();
        return methodDump.phaseDump(compilePhase);
    }
}
