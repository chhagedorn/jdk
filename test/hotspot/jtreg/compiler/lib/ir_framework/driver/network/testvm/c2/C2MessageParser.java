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

package compiler.lib.ir_framework.driver.network.testvm.c2;

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.IR;
import compiler.lib.ir_framework.Test;
import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.driver.network.testvm.TestVmMessageParser;
import compiler.lib.ir_framework.test.network.MessageTag;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Dedicated parser for {@link MethodDump}s received directly from the C2 compiler in the Test VM compiling the
 * {@link Test @Test} annotated methods with {@link IR @IR} rules.
 */
public class C2MessageParser implements TestVmMessageParser<MethodDump> {
    private static final Pattern COMPILE_PHASE_PATTERN = Pattern.compile("^COMPILE_PHASE: (.*)$");
    private static final PhaseDump INVALID_DUMP = PhaseDump.createInvalid();

    private final MethodDump methodDump;
    private PhaseDump phaseDump;

    public C2MessageParser(String methodName) {
        this.methodDump = new MethodDump(methodName);
        this.phaseDump = INVALID_DUMP;
    }

    @Override
    public void parseLine(String line) {
        Matcher m = COMPILE_PHASE_PATTERN.matcher(line);
        if (m.matches()) {
            parseCompilePhase(m);
            return;
        }
        if (line.equals(MessageTag.END_MARKER)) {
            parseEndTag();
            return;
        }
        TestFramework.check(!phaseDump.isInvalid(), "missing COMPILE_PHASE header");
        phaseDump.add(line);
    }

    private void parseCompilePhase(Matcher m) {
        CompilePhase compilePhase = CompilePhase.forName(m.group(1));
        TestFramework.check(phaseDump.isInvalid(), "can only have one active phase dump");
        phaseDump = new PhaseDump(compilePhase);
        methodDump.add(compilePhase, phaseDump);
    }

    private void parseEndTag() {
        TestFramework.check(!phaseDump.isInvalid(), "must have an active phase dump");
        phaseDump = INVALID_DUMP;
    }

    @Override
    public MethodDump output() {
        TestFramework.check(phaseDump.isInvalid(), "querying while still having active phase dump");
        return methodDump;
    }
}
