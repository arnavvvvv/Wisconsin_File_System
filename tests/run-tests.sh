#! /usr/bin/env bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NONE='\033[0m'

# Determine project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ $(basename "$SCRIPT_DIR") == "tests" ]]; then
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
    TESTS_DIR="$SCRIPT_DIR"
else
    PROJECT_ROOT="$SCRIPT_DIR"
    TESTS_DIR="$SCRIPT_DIR/tests"
fi

# run_test testdir testnumber
run_test () {
    local testdir=$1
    local testnum=$2
    local verbose=$3

    # pre: execute this after before the test is done, to set up
    local prefile="$PROJECT_ROOT/$testdir/$testnum.pre"
    if [[ -f $prefile ]]; then
    (cd "$PROJECT_ROOT" && bash "$prefile") >> /dev/null 2>&1
	if (( $verbose == 1 )); then
	    echo -n "pre-test:  "
	    cat "$prefile"
	fi
    fi
    local testfile="$PROJECT_ROOT/$testdir/$testnum.run"
    if (( $verbose == 1 )); then
	echo -n "test:      "
	cat $testfile
    fi
    
    # Run test from project root so relative paths work
    (cd "$PROJECT_ROOT" && timeout 15 /bin/bash "$testfile" > "$TESTS_DIR/tests-out/$testnum.out" 2> "$TESTS_DIR/tests-out/$testnum.err")
    echo $? > "$TESTS_DIR/tests-out/$testnum.rc"

    # post: execute this after the test is done, to clean up
    local postfile="$PROJECT_ROOT/$testdir/$testnum.post"
    if [[ -f $postfile ]]; then
	(cd "$PROJECT_ROOT" && eval $(cat "$postfile"))
	if (( $verbose == 1 )); then
	    echo -n "post-test: "
	    cat "$postfile"
	fi
    fi
    return 
}

print_error_message () {
    local testnum=$1
    local contrunning=$2
    local filetype=$3
    builtin echo -e "test $testnum: ${RED}$testnum.$filetype incorrect${NONE}"
    echo "  what results should be found in file: tests/tests/$testnum.$filetype"
    echo "  what results produced by your program: tests/tests-out/$testnum.$filetype"
    echo "  compare the two using diff, cmp, or related tools to debug, e.g.:"
    echo "  prompt> diff tests/tests/$testnum.$filetype tests/tests-out/$testnum.$filetype"
    echo "  See tests/$testnum.run for what is being run"
    if (( $contrunning == 0 )); then
	exit 1
    fi
}

# check_test testnumber contrunning out/err
check_test () {
    local testnum=$1
    local contrunning=$2
    local filetype=$3

    # option to use cmp instead?
    returnval=$(diff "$PROJECT_ROOT/$expecteddir/$testnum.$filetype" "$TESTS_DIR/tests-out/$testnum.$filetype")
    if (( $? == 0 )); then
	echo 0
    else
	echo 1
    fi
}

# run_and_check testdir testnumber contrunning verbose printerror
#   testnumber: the test to run and check
#   printerrer: if 1, print an error if test does not exist
run_and_check () {
    local testdir=$1
    local testnum=$2
    local contrunning=$3
    local verbose=$4
    local failmode=$5

    if [[ ! -f "$PROJECT_ROOT/$testdir/$testnum.run" ]]; then
	if (( $failmode == 1 )); then
	    echo "test $testnum does not exist" >&2; exit 1
	fi
	exit 0
    fi
    echo -n -e "running test $testnum: "
    cat "$PROJECT_ROOT/$testdir/$testnum.desc"
    run_test $testdir $testnum $verbose
    rccheck=$(check_test $testnum $contrunning rc)
    outcheck=$(check_test $testnum $contrunning out)
    errcheck=$(check_test $testnum $contrunning err)
    othercheck=0
    if [[ -f "$PROJECT_ROOT/$expecteddir/$testnum.other" ]]; then
	othercheck=$(check_test $testnum $contrunning other)
    fi
    # echo "results: outcheck:$outcheck errcheck:$errcheck"
    if (( $rccheck == 0 )) && (( $outcheck == 0 )) && (( $errcheck == 0 )) && (( $othercheck == 0 )); then
	echo -e "test $testnum: ${GREEN}passed${NONE}"
	if (( $verbose == 1 )); then
	    echo ""
	fi
    else
	if (( $rccheck == 1 )); then
	    print_error_message $testnum $contrunning rc
	fi
	if (( $outcheck == 1 )); then
	    print_error_message $testnum $contrunning out
	fi
	if (( $errcheck == 1 )); then
	    print_error_message $testnum $contrunning err
	fi
	if (( $othercheck == 1 )); then
	    print_error_message $testnum $contrunning other
	fi
    fi
}

# usage: call when args not parsed, or when help needed
usage () {
    echo "usage: run-tests.sh [-h] [-v] [-t test] [-c] [-s] [-d testdir]"
    echo "  -h                help message"
    echo "  -v                verbose"
    echo "  -t n              run only test n"
    echo "  -c                continue even after failure"
    echo "  -s                skip pre-test initialization"
    echo "  -d testdir        run tests from testdir"
    return 0
}

#
# main program
#
verbose=0
testdir="tests"
expecteddir="tests/tests"
contrunning=0
skippre=0
specific=""

args=`getopt hvsct:d: $*`
if [[ $? != 0 ]]; then
    usage; exit 1
fi

set -- $args
for i; do
    case "$i" in
    -h)
	usage
	exit 0
        shift;;
    -v)
        verbose=1
        shift;;
    -c)
        contrunning=1
        shift;;
    -s)
        skippre=1
        shift;;
    -t)
        specific=$2
	shift
	number='^[0-9]+$'
	if ! [[ $specific =~ $number ]]; then
	    usage
	    echo "-t must be followed by a number" >&2; exit 1
	fi
        shift;;
    -d)
        testdir=$2
	shift
        shift;;
    --)
        shift; break;;
    esac
done

# need a test directory; must be named "tests-out"
# Create it inside tests/ directory
if [[ ! -d "$TESTS_DIR/tests-out" ]]; then
    mkdir "$TESTS_DIR/tests-out"
fi

# do a one-time setup step
if (( $skippre == 0 )); then
    if [[ -f "$PROJECT_ROOT/tests/pre" ]]; then
	echo -e "doing one-time pre-test (use -s to suppress)"
	(cd "$PROJECT_ROOT" && source tests/pre)
	if (( $? != 0 )); then
	    echo "pre-test: failed"
	    exit 1
	fi
	echo ""
    fi
fi

# run just one test
if [[ $specific != "" ]]; then
    run_and_check $testdir $specific $contrunning $verbose 1
    exit 0
fi

# run all tests
(( testnum = 1 ))
while true; do
    run_and_check $testdir $testnum $contrunning $verbose 0
    (( testnum = $testnum + 1 ))
done

exit 0
