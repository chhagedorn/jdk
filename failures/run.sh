#!/bin/bash

# Run: bash run.sh path/to/java

java=$1
RED='\033[1;31m'
GREEN='\033[1;32m'
NC='\033[0m'
test_nr=1

function check_crash() {
    mv output $1_output
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


$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test1.java > output
check_crash Test1.java

$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test2.java > output
check_crash Test2.java

$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test3.java > output
check_crash Test3.java

# jdk-21+26-2230-tier6
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test4.java > output
check_crash Test4.java

# jdk-21+24-2010-tier8 (currently attached as Test/Reduced.java to JDK-8308392)
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test5.java > output
check_crash Test5.java
$java -Xcomp -XX:CompileCommand=compileonly,Reduced::* Reduced5.java > output
check_crash Reduced5.java

$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test6.java > output
check_crash Test6.java

$java -Xcomp -XX:-TieredCompilation -XX:CompileCommand=compileonly,Test::* Test7.java > output
check_crash Test7.java

# mach5-one-jdk-21+29-2397-tier8
$java -Xcomp -XX:-TieredCompilation -XX:CompileCommand=compileonly,Test::* Test8.java > output
check_crash Test8.java

# mach5-one-jdk-22+5-323-tier8
$java -Xcomp -XX:-TieredCompilation -XX:CompileCommand=compileonly,Test::* Test9.java > output
check_crash Test9.java

# mach5-one-jdk-21+30-2418-tier6
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test10.java > output
check_crash Test10.java

# mach5-one-jdk-21+30-2416-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test11.java > output
check_crash Test11.java

# mach5-one-jdk-22+5-322-tier8
$java -Xcomp -XX:-TieredCompilation -XX:CompileCommand=compileonly,Test::* Test12.java > output
check_crash Test12.java

# mach5-one-jdk-21+30-2419-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test13.java > output
check_crash Test13.java

# mach5-one-jdk-22+5-341-tier6
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test14.java > output
check_crash Test14.java

# mach5-one-jdk-22+5-340-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test15.java > output
check_crash Test15.java

# mach5-one-jdk-22+5-340-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test16.java > output
check_crash Test16.java

# mach5-one-jdk-21+31-2434-tier6
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test17.java > output
check_crash Test17.java

# mach5-one-jdk-22+6-368-tier6
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test18.java > output
check_crash Test18.java

# mach5-one-jdk-22+5-343-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test19.java > output
check_crash Test19.java

# mach5-one-jdk-21+31-2438-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test20.java > output
check_crash Test20.java

# mach5-one-jdk-21+31-2439-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test21.java > output
check_crash Test21.java

# custom user run
$java -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test22.java > output
check_crash Test22.java

# custom user run
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test23.java > output
check_crash Test23.java

# custom user run
$java -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test24.java > output
check_crash Test24.java

# custom user run
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test25.java > output
check_crash Test25.java

# custom user run
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test26.java > output
check_crash Test26.java

# mach5-one-jdk-22+6-360-tier6
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test27.java > output
check_crash Test27.java

# mach5-one-jdk-22+7-396-tier6
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test28.java > output
check_crash Test28.java

# mach5-one-jdk-22+6-377-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test29.java > output
check_crash Test29.java

# custom user run
$java -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test30.java > output
check_crash Test30.java

# custom user run
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test31.java > output
check_crash Test31.java

# mach5-one-jdk-21+32-2452-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test32.java > output
check_crash Test32.java

# custom user run
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test33.java > output
check_crash Test33.java

#  mach5-one-jdk-22+7-443-tier8
$java -Xcomp -XX:CompileCommand=compileonly,Test::* Test34.java > output
check_crash Test34.java
