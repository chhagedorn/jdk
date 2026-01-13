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
import compiler.lib.ir_framework.TestFramework;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

public class CompilePhaseDump {
    private static final CompilePhaseDump INVALID = new CompilePhaseDump(CompilePhase.DEFAULT);
    private final CompilePhase compilePhase;
    private final List<String> dump;

    CompilePhaseDump(CompilePhase compilePhase) {
        this.compilePhase = compilePhase;
        this.dump = new ArrayList<>();
    }

    public static CompilePhaseDump createInvalid() {
        return INVALID;
    }

    public boolean isInvalid() {
        return equals(INVALID);
    }

    public boolean isEmpty() {
        return dump.isEmpty();
    }

    public CompilePhase compilePhase() {
        TestFramework.check(!isInvalid(), "cannot query INVALID");
        return compilePhase;
    }

    void add(String line) {
        dump.add(line);
    }

    /**
     * Strips away tabs and white spaces and add 2 leading whitespaces for each non-empty line.
     * This makes it easier to read when dumping the compilation output.
     */
    public String dumpForOptoAssembly() {
        TestFramework.check(!isInvalid(), "cannot query INVALID");
        return dump.stream()
                .map(line -> {
                    String trimmed = line.trim();
                    return trimmed.isEmpty() ? "" : " ".repeat(2) + trimmed;
                })
                .collect(Collectors.joining(System.lineSeparator()));
    }

    public String dump() {
        TestFramework.check(!isInvalid(), "cannot query INVALID");
        return String.join(System.lineSeparator(), dump);
    }
}
