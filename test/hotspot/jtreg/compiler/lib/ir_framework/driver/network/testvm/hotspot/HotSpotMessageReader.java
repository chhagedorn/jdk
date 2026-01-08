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

package compiler.lib.ir_framework.driver.network.testvm.hotspot;

import compiler.lib.ir_framework.driver.network.testvm.TestVmMessageReader;
import compiler.lib.ir_framework.shared.TestFrameworkException;

import java.io.BufferedReader;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.concurrent.Callable;

public class HotSpotMessageReader implements Callable<MethodDump> {
    private final TestVmMessageReader<MethodDump> messageReader;

    public HotSpotMessageReader(Socket socket, BufferedReader reader) {
        String methodName = readMethodName(socket, reader);
        this.messageReader = new TestVmMessageReader<>(socket, reader, new HotSpotMessageParser(methodName));
    }

    private String readMethodName(Socket socket, BufferedReader reader) {
        try {
            socket.setSoTimeout(10000);
            String methodName = reader.readLine();
            socket.setSoTimeout(0);
            System.err.println(methodName);
            return methodName;
        } catch (SocketTimeoutException e) {
            throw new TestFrameworkException("Did not receive method name after 10s", e);
        } catch (Exception e) {
            throw new TestFrameworkException("Error reading method name", e);
        }
    }

    @Override
    public MethodDump call() throws Exception {
        return messageReader.call();
    }
}
