#!/bin/sh
# Real signals from outside (kill), which the test suites cannot send:
# Ctrl+C (SIGINT) in a busy loop, an empty loop, sleep() and input() is a
# KeyboardInterrupt (exit code 130, or caught); a handler of SIGTERM runs
# without cutting sleep() short; at the REPL's prompt Ctrl+C drops the
# line being typed, and while something runs it goes back to the prompt.
# Timing-based: each wait is generous.

cd "$(dirname "$0")/.." || exit 2

tmp=build/signals
mkdir -p "$tmp"
fail=0

expect() {
    if [ "$2" = "$3" ]; then
        echo "OK:   $1"
    else
        echo "FAIL: $1 (expected $3, got $2)"
        fail=$((fail + 1))
    fi
}

printf 'prog main() {\n    var i = 0;\n    while (true) {\n        i++;\n    }\n}\n' > "$tmp/loop.pce"
printf 'prog main() {\n    while (true) { }\n}\n' > "$tmp/empty.pce"
printf 'import time;\nprog main() {\n    time.sleep(30);\n}\n' > "$tmp/sleep.pce"
for name in loop empty sleep; do
    ./pycee "$tmp/$name.pce" > "$tmp/out" 2>&1 &
    pid=$!
    sleep 1
    kill -INT $pid
    wait $pid
    code=$?
    grep -q "^KeyboardInterrupt" "$tmp/out" || code="no KeyboardInterrupt"
    expect "Ctrl+C in $name" "$code" 130
done

printf 'prog main() {\n    try {\n        input();\n    } except KeyboardInterrupt {\n        print("caught");\n        return (3);\n    }\n}\n' > "$tmp/input.pce"
rm -f "$tmp/fifo"
mkfifo "$tmp/fifo"
(sleep 4 > "$tmp/fifo" &)
./pycee "$tmp/input.pce" < "$tmp/fifo" > "$tmp/out" 2>&1 &
pid=$!
sleep 1
kill -INT $pid
wait $pid
expect "Ctrl+C in input(), caught" "$?:$(cat "$tmp/out")" "3:caught"

printf 'import signal;\nimport time;\nvar n = 0;\nfunc on_term(s) {\n    n++;\n}\nprog main() {\n    signal.handle(signal.SIGTERM, on_term);\n    time.sleep(2);\n    print(n);\n}\n' > "$tmp/term.pce"
./pycee "$tmp/term.pce" > "$tmp/out" 2>&1 &
pid=$!
sleep 0.5
kill -TERM $pid
sleep 0.3
kill -TERM $pid
wait $pid
expect "SIGTERM handler, sleep goes on" "$?:$(cat "$tmp/out")" "0:2"

rm -f "$tmp/fifo"
mkfifo "$tmp/fifo"
(
    exec 3> "$tmp/fifo"
    printf 'func f() {\n' >&3
    sleep 1
    printf 'print("after")\nwhile (true) { }\n' >&3
    sleep 1.5
    printf 'print("back")\n' >&3
    sleep 0.5
) &
./pycee < "$tmp/fifo" > "$tmp/out" 2>&1 &
pid=$!
sleep 0.5
kill -INT $pid
sleep 1.2
kill -INT $pid
wait $pid
code=$?
expect "Ctrl+C in the REPL" "$code:$(grep -c KeyboardInterrupt "$tmp/out"):$(grep -c -e '^after$' -e '^back$' "$tmp/out")" "0:2:2"
rm -f "$tmp/fifo"

echo "signals: $((6 - fail)) passed, $fail failed"
[ "$fail" -eq 0 ]
