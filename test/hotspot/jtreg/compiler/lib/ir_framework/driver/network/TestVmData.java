package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.driver.network.testvm.hotspot.MethodDumps;
import compiler.lib.ir_framework.driver.network.testvm.java.IrEncoding;
import compiler.lib.ir_framework.driver.network.testvm.java.JavaMessages;
import compiler.lib.ir_framework.driver.network.testvm.java.VmInfo;

public class TestVmData {
    private final JavaMessages javaMessages;
    private final MethodDumps methodDumps;
    private final boolean allowNotCompilable;

    public TestVmData(JavaMessages javaMessages, MethodDumps methodDumps, boolean allowNotCompilable) {
        this.javaMessages = javaMessages;
        this.methodDumps = methodDumps;
        this.allowNotCompilable = allowNotCompilable;
    }

    public IrEncoding irEncoding() {
        return javaMessages.irEncoding();
    }

    public boolean isNotCompilableAllowed() {
        return allowNotCompilable;
    }

    public void printTestVmMessages() {
        javaMessages.print();
    }

    public VmInfo vmInfo() {
        return javaMessages.vmInfo();
    }
}
