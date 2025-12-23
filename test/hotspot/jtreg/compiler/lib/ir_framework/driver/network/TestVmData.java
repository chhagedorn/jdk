package compiler.lib.ir_framework.driver.network;

public class TestVmData {
    private final TestVmMessages testVmMessages;
    private final MethodDumps methodDumps;
    private final boolean allowNotCompilable;

    public TestVmData(TestVmMessages testVmMessages, MethodDumps methodDumps, boolean allowNotCompilable) {
        this.testVmMessages = testVmMessages;
        this.methodDumps = methodDumps;
        this.allowNotCompilable = allowNotCompilable;
    }

    public IrEncoding irEncoding() {
        return testVmMessages.irEncoding();
    }

    public boolean isNotCompilableAllowed() {
        return allowNotCompilable;
    }

    public void printTestVmMessages() {
        testVmMessages.print();
    }

    public VmInfo vmInfo() {
        return testVmMessages.vmInfo();
    }
}
