package compiler.lib.ir_framework.driver.network.testvm.java;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.stream.Stream;

public class IrRuleIds implements Iterable<Integer> {
    private static final IrRuleIds EMPTY = new IrRuleIds(new ArrayList<>());
    private final List<Integer> ruleIds;

    public IrRuleIds(List<Integer> ruleIds) {
        this.ruleIds = ruleIds;
    }

    public static IrRuleIds createEmpty() {
        return EMPTY;
    }

    public boolean isEmpty() {
        return equals(EMPTY);
    }

    public int count() {
        return ruleIds.size();
    }

    @Override
    public Iterator<Integer> iterator() {
        return ruleIds.iterator();
    }

    public Stream<Integer> stream() {
        return ruleIds.stream();
    }
}
