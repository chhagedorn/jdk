package compiler.lib.ir_framework.driver.network;

public interface TestVmParser<E> {
    void parse(String line);
    void finish();
    E output();
}
