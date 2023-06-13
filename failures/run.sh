#!/bin/bash

# Run: bash run.sh path/to/java

java=$1
RED='\033[1;31m'
GREEN='\033[1;32m'
NC='\033[0m'
test_nr=1

function check_crash() {
    echo -n "-> $1: "
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


$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test1.java > output$test_nr
check_crash Test1.java

$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test2.java > output$test_nr
check_crash Test2.java

$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test3.java > output$test_nr
check_crash Test3.java

# jdk-21+26-2230-tier6
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test4.java > output$test_nr
check_crash Test4.java

# jdk-21+24-2010-tier8 (currently attached as Test/Reduced.java to JDK-8308392)
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test5.java > output$test_nr
check_crash Test5.java
$java -Xcomp -XX:CompileCommand=compileonly,Reduced::* Reduced5.java > output$test_nr
check_crash Reduced5.java

$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test6.java > output$test_nr
check_crash Test6.java

