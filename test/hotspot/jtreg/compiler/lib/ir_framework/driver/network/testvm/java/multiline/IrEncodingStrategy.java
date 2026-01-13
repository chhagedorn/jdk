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

package compiler.lib.ir_framework.driver.network.testvm.java.multiline;

import compiler.lib.ir_framework.driver.network.testvm.java.IrEncoding;
import compiler.lib.ir_framework.driver.network.testvm.java.IrRuleIds;
import compiler.lib.ir_framework.shared.TestFrameworkException;
import compiler.lib.ir_framework.test.IrEncodingPrinter;

import java.util.ArrayList;
import java.util.List;

/**
 * Dedicated strategy to parse the multi-line IR Encoding message into a new {@link IrEncoding} object.
 */
public class IrEncodingStrategy implements MultiLineParsingStrategy<IrEncoding> {
    private final IrEncoding irEncoding;

    public IrEncodingStrategy() {
        this.irEncoding = new IrEncoding();
    }

    @Override
    public void parseLine(String line) {
        if (line.equals(IrEncodingPrinter.NO_ENCODING)) {
            return;
        }

        String[] splitLine = line.split(",");
        if (splitLine.length < 2) {
            throw new TestFrameworkException("Invalid IR match rule encoding. No comma found: " + splitLine[0]);
        }
        String testName = splitLine[0];
        IrRuleIds irRulesIds = parseIrRulesIds(splitLine);
        irEncoding.add(testName, irRulesIds);
    }

    /**
     * Parse rule indexes from IR encoding line of the format: <method,idx1,idx2,...>
     */
    private IrRuleIds parseIrRulesIds(String[] splitLine) {
        List<Integer> irRuleIds = new ArrayList<>();
        for (int i = 1; i < splitLine.length; i++) {
            try {
                irRuleIds.add(Integer.parseInt(splitLine[i]));
            } catch (NumberFormatException e) {
                throw new TestFrameworkException("Invalid IR match rule encoding. No number found: " + splitLine[i]);
            }
        }
        return new IrRuleIds(irRuleIds);
    }

    @Override
    public IrEncoding output() {
        return irEncoding;
    }
}
