package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.shared.TestFrameworkException;
import compiler.lib.ir_framework.shared.TestFrameworkSocket;

import java.io.BufferedReader;
import java.net.Socket;
import java.util.concurrent.Callable;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class TestVMTask implements Callable<TestVmOutput> {
    private final Socket socket;
    private final BufferedReader reader; // identity already consumed

    public TestVMTask(Socket socket, BufferedReader reader) {
        this.socket = socket;
        this.reader = reader;
    }

    @Override
    public TestVmOutput call() {
        try (socket; reader) {
            String line;
            TestVmOutput testVmOutput = new TestVmOutput();
            Pattern p = Pattern.compile("^(\\[[^]]+])\\s*(.*)$");
            while ((line = reader.readLine()) != null) {
                Matcher m = p.matcher(line);
                if (!m.matches()) {
                    throw new TestFrameworkException("missing tag");
                }
                String tag = m.group(1);
                String message = m.group(2);
                switch (tag) {
                    case TestFrameworkSocket.STDOUT_PREFIX -> testVmOutput.addStdoutLine(message);
                    case TestFrameworkSocket.TESTLIST_TAG -> testVmOutput.addTestListEntry(message);
                    case TestFrameworkSocket.PRINT_TIMES_TAG -> testVmOutput.addPrintTimes(message);
                }
            }
            return testVmOutput;
        } catch (Exception e) {
            throw new TestFrameworkException("Error while reading Test VM socket messages", e);
        }
    }
}
