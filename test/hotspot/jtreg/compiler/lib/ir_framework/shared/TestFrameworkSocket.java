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
import compiler.lib.ir_framework.test.TestVM;

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

    private static final String HOTSPOT_IDENTITY = "#HotSpot#";

    // Static fields used for test VM only.
    private static final String SERVER_PORT_PROPERTY = "ir.framework.server.port";
    private static final int SERVER_PORT = Integer.getInteger(SERVER_PORT_PROPERTY, -1);

    private static Socket clientSocket = null;
    private static PrintWriter clientWriter = null;

    private final int serverSocketPort;
    private final ServerSocket serverSocket;
    private boolean running;
    private final ExecutorService executor;
    private final List<Future<MethodDump>> methodDumps;
    private Future<TestVmMessages> testVmFuture;

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
        System.out.println(serverSocket.isBound());
        System.out.println(serverSocket.isClosed());
        Socket client = serverSocket.accept();
        System.out.println(serverSocket.isBound());
        System.out.println(serverSocket.isClosed());
        BufferedReader reader = new BufferedReader(new InputStreamReader(client.getInputStream()));
        String identity = readIdentity(client, reader).trim();
        System.out.println(identity);
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
        if (identity.equals(TestVM.IDENTITY)) {
            testVmFuture = executor.submit(new TestVmMessageReader(client, reader));
        } else if (identity.equals(HOTSPOT_IDENTITY)) {
            Future<MethodDump> future = executor.submit(new HotSpotMessageReader(client, reader));
            methodDumps.add(future);
        } else {
            throw new TestFrameworkException("Wrong identity: " + identity);
        }
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

    public TestVmData testVmData(boolean allowNotCompilable) {
        TestVmMessages testVmMessages = testVmMessages();
        MethodDumps methodDumps = methodDumps();
        return new TestVmData(testVmMessages, methodDumps, allowNotCompilable);
    }

    private MethodDumps methodDumps() {
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

    /**
     * Get the socket output of the flag VM.
     */
    private TestVmMessages testVmMessages() {
        try {
            return testVmFuture.get();
        } catch (ExecutionException e) {
            throw new TestFrameworkException("No test VM messages were received", e);
        } catch (Exception e) {
            throw new TestFrameworkException("Error while fetching Test VM Future", e);
        }
    }
}

