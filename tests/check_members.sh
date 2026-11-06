#!/bin/sh
# Checks the naming rule of the modules of the language: no member may be
# called like a reserved word or a global native, so that every one can
# be imported alone. Each member (from ./pycee --members) is imported
# with "from m import name" in a file (--check) and on a REPL line; the
# one allowed exception is sys.exit, which only clashes in the REPL.

cd "$(dirname "$0")/.." || exit 2

tmp=build/members
mkdir -p "$tmp"
fail=0
count=0
members=$(./pycee --members) || exit 2
while read -r mod name; do
    count=$((count + 1))
    printf 'from %s import %s;\nprog main() { }\n' "$mod" "$name" > "$tmp/m.pce"
    if ! out=$(./pycee --check "$tmp/m.pce" 2>&1); then
        echo "FAIL: $mod.$name cannot be imported alone in a file:"
        echo "$out" | head -1
        fail=$((fail + 1))
    fi
    out=$(printf 'from %s import %s\n' "$mod" "$name" | ./pycee 2>&1)
    if [ "$mod.$name" = "sys.exit" ]; then
        [ -n "$out" ] && continue
        echo "FAIL: sys.exit can be imported alone in the REPL"
        fail=$((fail + 1))
    elif [ -n "$out" ]; then
        echo "FAIL: $mod.$name cannot be imported alone in the REPL:"
        echo "$out" | head -1
        fail=$((fail + 1))
    fi
done <<END
$members
END
echo "members: $((count - fail)) passed, $fail failed"
[ "$fail" -eq 0 ]
