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

import compiler.lib.ir_framework.CompilePhase;
import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.test.Tag;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

class HotSpotMessageParser {
    private static final Pattern COMPILE_PHASE_PATTERN = Pattern.compile("^COMPILE_PHASE (.*)$");

    private static final PhaseDump EMPTY_PHASE = new PhaseDump(CompilePhase.PRINT_IDEAL);

    private final MethodDump methodDump;
    private PhaseDump phaseDump;

    public HotSpotMessageParser(String methodName) {
        this.methodDump = new MethodDump(methodName);
    }

    public void parse(String line) {
        Matcher m = COMPILE_PHASE_PATTERN.matcher(line);
        if (m.matches()) {
            CompilePhase compilePhase = CompilePhase.forName(m.group());
            TestFramework.check(phaseDump.equals(EMPTY_PHASE), "can only have one active phase dump");
            phaseDump = new PhaseDump(compilePhase);
            return;
        }
        if (line.equals(Tag.END_TAG)) {
            TestFramework.check(!phaseDump.equals(EMPTY_PHASE), "must have an active phase dump");
            methodDump.add(phaseDump);
            phaseDump = EMPTY_PHASE;
            return;
        }
        phaseDump.add(line);
    }

    MethodDump methodDump() {
        TestFramework.check(!phaseDump.equals(EMPTY_PHASE), "still parsing phase dump");
        return methodDump;
    }
}
