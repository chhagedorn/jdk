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


$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test1.java > output
check_crash Test1.java

$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test2.java > output
check_crash Test2.java

$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test3.java > output
check_crash Test3.java

# jdk-21+26-2230-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test4.java > output
check_crash Test4.java

# jdk-21+24-2010-tier8 (currently attached as Test/Reduced.java to JDK-8308392)
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test5.java > output
check_crash Test5.java
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Reduced::* Reduced5.java > output
check_crash Reduced5.java

$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test6.java > output
check_crash Test6.java

$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:-TieredCompilation -XX:CompileCommand=compileonly,Test::* Test7.java > output
check_crash Test7.java

# mach5-one-jdk-21+29-2397-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:-TieredCompilation -XX:CompileCommand=compileonly,Test::* Test8.java > output
check_crash Test8.java

# mach5-one-jdk-22+5-323-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:-TieredCompilation -XX:CompileCommand=compileonly,Test::* Test9.java > output
check_crash Test9.java

# mach5-one-jdk-21+30-2418-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test10.java > output
check_crash Test10.java

# mach5-one-jdk-21+30-2416-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test11.java > output
check_crash Test11.java

# mach5-one-jdk-22+5-322-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:-TieredCompilation -XX:CompileCommand=compileonly,Test::* Test12.java > output
check_crash Test12.java

# mach5-one-jdk-21+30-2419-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test13.java > output
check_crash Test13.java

# mach5-one-jdk-22+5-341-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test14.java > output
check_crash Test14.java

# mach5-one-jdk-22+5-340-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test15.java > output
check_crash Test15.java

# mach5-one-jdk-22+5-340-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test16.java > output
check_crash Test16.java

# mach5-one-jdk-21+31-2434-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test17.java > output
check_crash Test17.java

# mach5-one-jdk-22+6-368-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test18.java > output
check_crash Test18.java

# mach5-one-jdk-22+5-343-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test19.java > output
check_crash Test19.java

# mach5-one-jdk-21+31-2438-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test20.java > output
check_crash Test20.java

# mach5-one-jdk-21+31-2439-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test21.java > output
check_crash Test21.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test22.java > output
check_crash Test22.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test23.java > output
check_crash Test23.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test24.java > output
check_crash Test24.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test25.java > output
check_crash Test25.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test26.java > output
check_crash Test26.java

# mach5-one-jdk-22+6-360-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test27.java > output
check_crash Test27.java

# mach5-one-jdk-22+7-396-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test28.java > output
check_crash Test28.java

# mach5-one-jdk-22+6-377-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test29.java > output
check_crash Test29.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test30.java > output
check_crash Test30.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test31.java > output
check_crash Test31.java

# mach5-one-jdk-21+32-2452-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test32.java > output
check_crash Test32.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test33.java > output
check_crash Test33.java

# mach5-one-jdk-22+7-443-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test34.java > output
check_crash Test34.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test35.java > output
check_crash Test35.java

# mach5-one-jdk-22+9-607
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test36.java > output
check_crash Test36.java

# mach5-one-jdk-22+9-607
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test37.java > output
check_crash Test37.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test38.java > output
check_crash Test38.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test39.java > output
check_crash Test39.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test40.java > output
check_crash Test40.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test41.java > output
check_crash Test41.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test42.java > output
check_crash Test42.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test43.java > output
check_crash Test43.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test44.java > output
check_crash Test44.java

# mach5-one-jdk-22+11-754-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test45.java > output
check_crash Test45.java

# mach5-one-jdk-22+11-772-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test46.java > output
check_crash Test46.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test47.java > output
check_crash Test47.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test48.java > output
check_crash Test48.java

# custom user run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test49.java > output
check_crash Test49.java

# mach5-one-jdk-22+11-772-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test50.java > output
check_crash Test50.java

# mach5-one-jdk-22+11-772-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test51.java > output
check_crash Test51.java

# mach5-one-jdk-22+11-772-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test52.java > output
check_crash Test52.java

# mach5-one-jdk-22+11-772-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test53.java > output
check_crash Test53.java

# mach5-one-jdk-22+11-786-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test54.java > output
check_crash Test54.java

# mach5-one-jdk-22+12-814-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test55.java > output
check_crash Test55.java

# mach5-one-jdk-22+12-810-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test56.java > output
check_crash Test56.java

# mach5-one-jdk-22+12-810-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileOnly=Test::* -XX:CompileOnly=Test::* -XX:-TieredCompilation -XX:CompileCommand=RepeatCompilation,Test::mainTest,100 -XX:+StressIGVN Test57.java > output
check_crash Test57.java

# mach5-one-jdk-22+12-833-tier6
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test58.java > output
check_crash Test58.java

# mach5-one-jdk-22+12-831-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* -XX:-TieredCompilation Test59.java > output
check_crash Test59.java

# mach5-one-jdk-22+12-833-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test60.java > output
check_crash Test60.java

# mach5-one-jdk-22+12-856-tier8
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test61.java > output
check_crash Test61.java

# custom run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test62.java > output
check_crash Test62.java

# custom run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test63.java > output
check_crash Test63.java

# custom run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test64.java > output
check_crash Test64.java

# custom run
$java -XX:+AbortVMOnCompilationFailure -Xcomp -XX:CompileCommand=compileonly,Test::* Test64.java > output
check_crash Test65.java
