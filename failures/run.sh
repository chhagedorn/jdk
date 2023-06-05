#!/bin/bash

# Run: bash run.sh path/to/java

java=$1
RED='\033[1;31m'
GREEN='\033[1;32m'
NC='\033[0m'
test_nr=1

function check_crash() {
    echo -n "Test$test_nr.java... "
    test_nr=$((test_nr+1))
    hs_err_file=$(find . -maxdepth 1 -name "hs_err_pid*")
    if [[ $hs_err_file ]]; then
        echo -e "${RED}CRASH!${NC}"
        rm -f hs_err*
        rm -f replay_pid*
    else
        echo -e "${GREEN}SUCCESS!${NC}"
    fi
}


$java -Xcomp -XX:CompileOnly=Test Test1.java > output$test_nr
check_crash

$java -Xcomp -XX:CompileOnly=Test Test2.java > output$test_nr
check_crash

$java -Xcomp -XX:CompileOnly=Test Test3.java > output$test_nr
check_crash

# jdk-21+26-2230-tier6
$java -Xcomp -XX:CompileOnly=Test Test4.java > output$test_nr
check_crash

# jdk-21+24-2010-tier8 (currently attached as Test/Reduced.java to JDK-8308392)
$java -Xcomp -XX:CompileOnly=Test Test5.java > output$test_nr
check_crash
$java -Xcomp -XX:CompileOnly=Reduced Reduced5.java > output$test_nr
check_crash

