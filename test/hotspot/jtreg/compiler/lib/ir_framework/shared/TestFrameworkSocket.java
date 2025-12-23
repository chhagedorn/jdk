/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package compiler.lib.ir_framework.shared;

import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.driver.network.*;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.*;
import java.util.*;
import java.util.concurrent.*;

/**
 * Dedicated driver VM socket to receive data from test VM Java and HotSpot code.
 */
public class TestFrameworkSocket implements AutoCloseable {
    public static final String STDOUT_PREFIX = "[STDOUT]";
    public static final String TESTLIST_TAG = "[TESTLIST]";
    public static final String DEFAULT_REGEX_TAG = "[DEFAULT_REGEX]";
    public static final String PRINT_TIMES_TAG = "[PRINT_TIMES]";
    public static final String NOT_COMPILABLE_TAG = "[NOT_COMPILABLE]";

    private static final String TEST_VM_IDENTITY = "#TestVM#";
    private static final String HOTSPOT_IDENTITY = "#HotSpot#";

    // Static fields used for test VM only.
    private static final String SERVER_PORT_PROPERTY = "ir.framework.server.port";
    private static final int SERVER_PORT = Integer.getInteger(SERVER_PORT_PROPERTY, -1);

    private static final boolean REPRODUCE = Boolean.getBoolean("Reproduce");
    private static Socket clientSocket = null;
    private static PrintWriter clientWriter = null;

    private final int serverSocketPort;
    private final ServerSocket serverSocket;
    private boolean running;
    private final ExecutorService executor;
    private final List<Future<MethodDump>> methodDumps;
    private Future<TestVmOutput> testVmFuture;

    public TestFrameworkSocket() {
        try {
            serverSocket = new ServerSocket();
            serverSocket.bind(new InetSocketAddress(InetAddress.getLoopbackAddress(), 0));
        } catch (IOException e) {
            throw new TestFrameworkException("Failed to create TestFramework server socket", e);
        }
        serverSocketPort = serverSocket.getLocalPort();
        executor = Executors.newCachedThreadPool();
        methodDumps = Collections.synchronizedList(new ArrayList<>());
        if (TestFramework.VERBOSE) {
            System.out.println("TestFramework server socket uses port " + serverSocketPort);
        }
        start();
    }

    public int serverSocketPort() {
        return serverSocketPort;
    }
    public String getPortPropertyFlag() {
        return "-D" + SERVER_PORT_PROPERTY + "=" + serverSocketPort;
    }

    private void start() {
        running = true;
        executor.submit(this::acceptLoop);
    }

    private void acceptLoop() {
        while (running) {
            try {
                handleClientConnection();
            } catch (TestFrameworkException e) {
                running = false;
                throw e;
            } catch (Exception e) {
                running = false;
                throw new TestFrameworkException("Server socket error", e);
            }
        }
    }

    private void handleClientConnection() throws IOException {
        Socket client = serverSocket.accept();
        BufferedReader reader = new BufferedReader(new InputStreamReader(client.getInputStream()));
        String identity = readIdentity(client, reader);
        submitTask(identity, client, reader);
    }

    private String readIdentity(Socket client, BufferedReader reader) throws IOException {
        String identity;
        try {
            client.setSoTimeout(10000);
            identity = reader.readLine();
            client.setSoTimeout(0);
        } catch (SocketTimeoutException e) {
            throw new TestFrameworkException("Did not receive initial identity message after 10s", e);
        }
        return identity;
    }

    private void submitTask(String identity, Socket client, BufferedReader reader) {
        if (identity.equals(TEST_VM_IDENTITY)) {
            testVmFuture = executor.submit(new TestVMTask(client, reader));
        } else if (identity.equals(HOTSPOT_IDENTITY)) {
            Future<MethodDump> future = executor.submit(new HotSpotMessageReader(client, reader));
            methodDumps.add(future);
        } else {
            throw new TestFrameworkException("Wrong identity: " + identity);
        }
    }

    public MethodDumps methodDumps() {
        MethodDumps methodDumps = new MethodDumps();
        for (Future<MethodDump> future : this.methodDumps) {
            try {
                MethodDump methodDump = future.get();
                methodDumps.add(methodDump);
            } catch (Exception e) {
                throw new TestFrameworkException("Error while fetching HotSpot Future", e);
            }
        }
        return methodDumps;
    }

    @Override
    public void close() {
        try {
            running = false;
            serverSocket.close();
        } catch (IOException e) {
            throw new TestFrameworkException("Could not close socket", e);
        }
    }

    /**
     * Only called by test VM to write to server socket.
     */
    public static void write(String msg, String tag) {
        write(msg, tag, false);
    }

    /**
     * Only called by test VM to write to server socket.
     * <p>
     * The test VM is spawned by the main jtreg VM. The stdout of the test VM is hidden
     * unless the Verbose or ReportStdout flag is used. TestFrameworkSocket is used by the parent jtreg
     * VM and the test VM to communicate. By sending the prints through the TestFrameworkSocket with the
     * parameter stdout set to true, the parent VM will print the received messages to its stdout, making it
     * visible to the user.
     */
    public static void write(String msg, String tag, boolean stdout) {
        if (REPRODUCE) {
            System.out.println("Debugging Test VM: Skip writing due to -DReproduce");
            return;
        }
        TestFramework.check(SERVER_PORT != -1, "Server port was not set correctly for flag and/or test VM "
                                               + "or method not called from flag or test VM");
        try {
            // Keep the client socket open until the test VM terminates (calls closeClientSocket before exiting main()).
            if (clientSocket == null) {
                clientSocket = new Socket(InetAddress.getLoopbackAddress(), SERVER_PORT);
                clientWriter = new PrintWriter(clientSocket.getOutputStream(), true);
            }
            if (stdout) {
                msg = STDOUT_PREFIX + tag + " " + msg;
            }

            clientWriter.write(TEST_VM_IDENTITY);
            clientWriter.println(msg);
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
        if (TestFramework.VERBOSE) {
            System.out.println("Written " + tag + " to socket:");
            System.out.println(msg);
        }
    }

    /**
     * Closes (and flushes) the printer to the socket and the socket itself. Is called as last thing before exiting
     * the main() method of the flag and the test VM.
     */
    public static void closeClientSocket() {
        if (clientSocket != null) {
            try {
                clientWriter.close();
                clientSocket.close();
            } catch (IOException e) {
                throw new RuntimeException("Could not close TestVM socket", e);
            }
        }
    }

    /**
     * Get the socket output of the flag VM.
     */
    public TestVmOutput testVmOutput() {
        try {
            return testVmFuture.get();
        } catch (ExecutionException e) {
            throw new TestFrameworkException("Not data was received on socket", e);
        } catch (Exception e) {
            throw new TestFrameworkException("Could not read from socket task", e);
        }
    }
}

