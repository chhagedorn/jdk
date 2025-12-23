package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.test.Tag;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

class HotSpotMessageParser {
    private static final Pattern COMPILE_PHASE_PATTERN = Pattern.compile("^COMPILE_PHASE (.*)$");

    private static final PhaseDump EMPTY_PHASE = new PhaseDump(CompilePhase.PRINT_IDEAL);

    private final MethodDump methodDump;
    private PhaseDump phaseDump;

    public HotSpotMessageParser(String methodName) {
        this.methodDump = new MethodDump(methodName);
    }

    public void parse(String line) {
        Matcher m = COMPILE_PHASE_PATTERN.matcher(line);
        if (m.matches()) {
            CompilePhase compilePhase = CompilePhase.forName(m.group());
            TestFramework.check(phaseDump.equals(EMPTY_PHASE), "can only have one active phase dump");
            phaseDump = new PhaseDump(compilePhase);
            return;
        }
        if (line.equals(Tag.END_TAG)) {
            TestFramework.check(!phaseDump.equals(EMPTY_PHASE), "must have an active phase dump");
            methodDump.add(phaseDump);
            phaseDump = EMPTY_PHASE;
            return;
        }
        phaseDump.add(line);
    }

    MethodDump methodDump() {
        TestFramework.check(!phaseDump.equals(EMPTY_PHASE), "still parsing phase dump");
        return methodDump;
    }
}
