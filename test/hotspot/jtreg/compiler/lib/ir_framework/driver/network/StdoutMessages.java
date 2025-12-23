package compiler.lib.ir_framework.driver.network;

import java.util.ArrayList;
import java.util.List;

public class StdoutMessages implements TestVmMessage {
    private final List<String> messages;

    public StdoutMessages() {
        this.messages = new ArrayList<>();
    }

    public void add(String time) {
        messages.add(time);
    }

    @Override
    public void print() {
        if (messages.isEmpty()) {
            return;
        }
        System.out.println();
        System.out.println("Test VM Messages");
        System.out.println("----------------");
        for (String methodTime : messages) {
            System.out.println("- " + methodTime);
        }
    }
}
