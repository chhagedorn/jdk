package compiler.lib.ir_framework.driver.network;

import compiler.lib.ir_framework.shared.TestFrameworkException;

import java.io.BufferedReader;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.concurrent.Callable;

public class HotSpotMessageReader implements Callable<MethodDump> {
    private final Socket socket;
    private final BufferedReader reader; // identity already consumed
    private final HotSpotMessageParser messageParser;

    public HotSpotMessageReader(Socket socket, BufferedReader in) {
        this.socket = socket;
        this.reader = in;
        String methodName = readMethodName();
        this.messageParser = new HotSpotMessageParser(methodName);
    }

    private String readMethodName() {
        try {
            socket.setSoTimeout(10000);
            String methodName = reader.readLine();
            socket.setSoTimeout(0);
            return methodName;
        } catch (SocketTimeoutException e) {
            throw new TestFrameworkException("Did not receive method name after 10s", e);
        } catch (Exception e) {
            throw new TestFrameworkException("Error reading method name", e);
        }
    }

    @Override
    public MethodDump call() throws Exception {
        try (socket; reader) {
            String line;
            while ((line = reader.readLine()) != null) {
                messageParser.parse(line.trim());
            }
            return messageParser.methodDump();
        } catch (Exception e) {
            throw new TestFrameworkException("Error while reading Test VM socket messages", e);
        }
    }
}
