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
