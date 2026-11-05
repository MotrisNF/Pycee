#!/bin/sh
# Runs every test suite: each tests/<suite>/*.pce goes through ./pycee with
# the suite's mode, and stdout, stderr and the exit code are compared with
# the matching .out file.
#   sh tests/run_tests.sh                     all suites
#   sh tests/run_tests.sh parser              one suite
#   sh tests/run_tests.sh --update [suite]    regenerate .out files (check them by eye!)
#
# A .out whose first line is "#contains" only requires each following
# line to appear somewhere in the output (for output that depends on the
# machine, such as where the stack guard stops); --update leaves it alone.
#
# Adding a suite = one line in suite_mode ("-" = no flag, normal mode;
# "<" = the file is typed into the REPL, one line at a time). In the
# other modes a <name>.in next to the test is its stdin (/dev/null if none).

cd "$(dirname "$0")/.." || exit 2

suite_mode() {
    case "$1" in
        lexer)  echo "--tokens" ;;
        parser) echo "--ast" ;;
        resolver) echo "--check" ;;
        imports) echo "--check" ;;
        eval) echo "<" ;;
        run) echo "-" ;;
        *)      echo "" ;;
    esac
}

update=0
if [ "$1" = "--update" ]; then
    update=1
    shift
fi
suites=${1:-"lexer parser resolver imports eval run"}

total_fail=0
for suite in $suites; do
    mode=$(suite_mode "$suite")
    if [ -z "$mode" ]; then
        echo "unknown suite: $suite"
        exit 2
    fi
    pass=0
    fail=0
    for src in tests/"$suite"/*.pce; do
        expected_file="${src%.pce}.out"
        if [ "$mode" = "<" ]; then
            actual=$(./pycee < "$src" 2>&1; echo "exit=$?")
        else
            if [ "$mode" = "-" ]; then
                set -- "$src"
            else
                set -- "$mode" "$src"
            fi
            input_file="${src%.pce}.in"
            [ -f "$input_file" ] || input_file=/dev/null
            actual=$(./pycee "$@" < "$input_file" 2>&1; echo "exit=$?")
        fi
        if [ -f "$expected_file" ] && [ "$(head -n 1 "$expected_file")" = "#contains" ]; then
            [ "$update" -eq 1 ] && continue
            missing=$(tail -n +2 "$expected_file" | while IFS= read -r want; do
                printf '%s\n' "$actual" | grep -qF -- "$want" || echo "$want"
            done)
            if [ -z "$missing" ]; then
                pass=$((pass + 1))
            else
                fail=$((fail + 1))
                echo "FAIL: $src (missing: $missing)"
            fi
            continue
        fi
        if [ "$update" -eq 1 ]; then
            printf '%s\n' "$actual" > "$expected_file"
            continue
        fi
        if [ -f "$expected_file" ] && [ "$actual" = "$(cat "$expected_file")" ]; then
            pass=$((pass + 1))
        else
            fail=$((fail + 1))
            echo "FAIL: $src"
            printf '%s\n' "$actual" | diff "$expected_file" - | head -20
        fi
    done
    if [ "$update" -eq 1 ]; then
        echo "$suite: updated"
    else
        echo "$suite: $pass passed, $fail failed"
    fi
    total_fail=$((total_fail + fail))
done
[ "$total_fail" -eq 0 ]
