/*
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
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

import compiler.lib.ir_framework.TestFramework;
import compiler.lib.ir_framework.driver.irmatching.parser.TestMethod;
import compiler.lib.ir_framework.shared.TestFrameworkException;

import java.util.ArrayList;
import java.util.List;

/**
 * Class to parse the IR encoding emitted by the test VM and creating {@link TestMethod} objects for each entry.
 *
 * @see TestMethod
 */
public class IrEncodingParser implements TestVmParser<IrEncoding> {

    private boolean finished;
    private final IrEncoding irEncoding;

    public IrEncodingParser() {
        this.irEncoding = new IrEncoding();
    }

    @Override
    public void parse(String line) {
        TestFramework.check(!finished, "cannot parse when already queried");
        String[] splitLine = line.split(",");
        if (splitLine.length < 2) {
            throw new TestFrameworkException("Invalid IR match rule encoding. No comma found: " + splitLine[0]);
        }
        String testName = splitLine[0];
        List<Integer> irRulesIds = parseIrRulesIds(splitLine);
        irEncoding.add(testName, irRulesIds);
    }

    /**
     * Parse rule indexes from IR encoding line of the format: <method,idx1,idx2,...>
     */
    private List<Integer> parseIrRulesIds(String[] splitLine) {
        List<Integer> irRuleIds = new ArrayList<>();
        for (int i = 1; i < splitLine.length; i++) {
            try {
                irRuleIds.add(Integer.parseInt(splitLine[i]));
            } catch (NumberFormatException e) {
                throw new TestFrameworkException("Invalid IR match rule encoding. No number found: " + splitLine[i]);
            }
        }
        return irRuleIds;
    }

    @Override
    public void finish() {
        finished = true;
    }

    @Override
    public IrEncoding output() {
        TestFramework.check(finished, "must be finished before querying");
        return irEncoding;
    }
}
