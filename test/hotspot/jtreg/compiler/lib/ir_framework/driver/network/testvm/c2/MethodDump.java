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
import compiler.lib.ir_framework.TestFramework;

import java.util.*;

/**
 * This class holds all {@link CompilePhaseDump}s of a single {@link IR @IR}-annoted method.
 */
public class MethodDump {
    private final String methodName;
    private final Map<CompilePhase, CompilePhaseDump> phaseDumps;

    public MethodDump(String methodName) {
        this.methodName = methodName;
        this.phaseDumps = new LinkedHashMap<>();
    }

    public String methodName() {
        return methodName;
    }

    void add(CompilePhaseDump compilePhaseDump) {
        CompilePhase compilePhase = compilePhaseDump.compilePhase();
        if (compilePhase.overrideRepeatedPhase() || !phaseDumps.containsKey(compilePhase)) {
            phaseDumps.put(compilePhase, compilePhaseDump);
        }
    }

    public CompilePhaseDump phaseDump(CompilePhase compilePhase) {
        TestFramework.check(compilePhase != CompilePhase.DEFAULT, "cannot query for DEFAULT");
        return phaseDumps.computeIfAbsent(compilePhase, _ -> new CompilePhaseDump(compilePhase));
    }
}
