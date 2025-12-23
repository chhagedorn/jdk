package compiler.lib.ir_framework.driver.network;

public class TestVmMessages {
    private final StdoutMessages stdoutMessages;
    private final MethodTimes methodTimes;
    private final ExecutedTests executedTests;
    private VmInfo vmInfo; // TODO: Better pass into constructor?
    private IrEncoding irEncoding;

    TestVmMessages() {
        this.stdoutMessages = new StdoutMessages();
        this.executedTests = new ExecutedTests();
        this.methodTimes = new MethodTimes();
        this.vmInfo = new VmInfo();
        this.irEncoding = new IrEncoding();
    }

    public VmInfo vmInfo() {
        return vmInfo;
    }

    public IrEncoding irEncoding() {
        return irEncoding;
    }

    void addStdoutLine(String line) {
        stdoutMessages.add(line);
    }

    void addExecutedTest(String test) {
        executedTests.add(test);
    }

    void addMethodTime(String methodTime) {
        methodTimes.add(methodTime);
    }

    void addVmInfo(VmInfo vmInfo) {
        this.vmInfo = vmInfo;
    }

    void addIrEncoding(IrEncoding irEncoding) {
        this.irEncoding = irEncoding;
    }

    public void print() {
        stdoutMessages.print();
        methodTimes.print();
        executedTests.print();
        vmInfo.print();
        irEncoding.print();
    }
}
