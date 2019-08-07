#!/bin/sh

# Run all the test executables specified as arguments and output a summary.

TEST_PROGS=$@

# Some color codes.
RESET="\e[0m"
BG_GREEN="\e[102m"
BG_RED="\e[101m"
FG_BLACK="\e[30m"

PASS=0
FAIL=0

for TEST in $TEST_PROGS; do
    # Run the test.
    if ./$TEST; then
        STR="${BG_GREEN}${FG_BLACK}PASS$RESET"
        PASS=$(( PASS + 1 ))
    else
        STR="${BG_RED}${FG_BLACK}FAIL$RESET"
        FAIL=$(( FAIL + 1 ))
    fi
    echo -e "$STR $TEST"
done

echo "Tests summary:"
if (( $FAIL == 0 )); then
    echo "    All $PASS tests passed."
else
    echo "    $FAIL tests failed."
fi
