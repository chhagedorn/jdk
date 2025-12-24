/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
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
