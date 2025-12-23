package compiler.lib.ir_framework.test;

import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.shared.TestRunException;

import java.io.IOException;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.Socket;

public class TestVmSocket {
    private static final boolean REPRODUCE = Boolean.getBoolean("Reproduce");
    private static final String SERVER_PORT_PROPERTY = "ir.framework.server.port";
    private static final int SERVER_PORT = Integer.getInteger(SERVER_PORT_PROPERTY, -1);

    private static Socket socket = null;
    private static PrintWriter writer = null;

    public static void send(String message) {
        sendWithTag(Tag.STDOUT_TAG, message);
    }

    public static void sendMultiLine(String tag, String message) {
        if (REPRODUCE) {
            // Debugging Test VM: Skip writing due to -DReproduce;
            return;
        }

        TestFramework.check(socket != null, "must be connected");
        writer.println(tag + System.lineSeparator() + message);
    }

    public static void sendWithTag(String tag, String message) {
        if (REPRODUCE) {
            // Debugging Test VM: Skip writing due to -DReproduce;
            return;
        }

        TestFramework.check(socket != null, "must be connected");
        writer.println(tag + " " + message);
    }

    public static void connect() {
        if (REPRODUCE) {
            // Debugging Test VM: Skip writing due to -DReproduce;
            return;
        }

        TestFramework.check(SERVER_PORT != -1, "Server port was not set correctly for flag and/or test VM "
                + "or method not called from flag or test VM");

        try {
            // Keep the client socket open until the test VM terminates (calls closeClientSocket before exiting main()).
            socket = new Socket(InetAddress.getLoopbackAddress(), SERVER_PORT);
            writer = new PrintWriter(socket.getOutputStream(), true);
            writer.println(TestVM.IDENTITY);
        } catch (Exception e) {
            // When the test VM is directly run, we should ignore all messages that would normally be sent to the
            // driver VM.
            String failMsg = System.lineSeparator() + System.lineSeparator() + """
                             ###########################################################
                              Did you directly run the test VM (TestVM class)
                              to reproduce a bug?
                              => Append the flag -DReproduce=true and try again!
                             ###########################################################
                             """;
            throw new TestRunException(failMsg, e);
        }

    }

    /**
     * Closes (and flushes) the printer to the socket and the socket itself. Is called as last thing before exiting
     * the main() method of the flag and the test VM.
     */
    public static void close() {
        if (socket != null) {
            writer.close();
            try {
                socket.close();
            } catch (IOException e) {
                throw new RuntimeException("Could not close TestVM socket", e);
            }
        }
    }
}
