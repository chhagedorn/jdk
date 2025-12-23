package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.shared.TestFrameworkException;

import java.io.BufferedReader;
import java.net.Socket;
import java.util.concurrent.Callable;

// TODO: Common class with HotSpotVmMessageReader and pass in strategy or just parser?
public class TestVmMessageReader implements Callable<TestVmMessages> {
    private final Socket socket;
    private final BufferedReader reader; // identity already consumed
    private final TestVmMessageParser messageParser;

    public TestVmMessageReader(Socket socket, BufferedReader reader) {
        this.socket = socket;
        this.reader = reader;
        this.messageParser = new TestVmMessageParser();
    }

    @Override
    public TestVmMessages call() {
        try (socket; reader) {
            String line;
            while ((line = reader.readLine()) != null) {
                messageParser.parse(line.trim());
            }
            return messageParser.testVmMessages();
        } catch (Exception e) {
            throw new TestFrameworkException("Error while reading Test VM socket messages", e);
        }
    }
}

