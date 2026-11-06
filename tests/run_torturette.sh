#!/bin/sh
# Fault injection with torturette: every malloc/calloc/realloc of pycee is
# made to fail, one at a time, and leaks, double frees and crashes on those
# paths are reported. Complements valgrind, which only sees the path taken.
#   sh tests/run_torturette.sh            all cases
#   sh tests/run_torturette.sh -v         same, with the full reports
#
# torturette preloads its own malloc, which does not mix with the
# sanitizers, so pycee is built apart with -g and no -fsanitize.
# The tool lives in the project (tests/torturette/: the torturette binary
# and the injector.so it preloads, found next to it; Linux x86-64, glibc),
# so nothing outside this directory is needed.

cd "$(dirname "$0")/.." || exit 2

tool=tests/torturette/torturette
bin=build/torture/pycee
verbose=0
[ "$1" = "-v" ] && verbose=1

if [ ! -x "$tool" ]; then
    echo "torturette not found at $tool"
    exit 2
fi
mkdir -p build/torture
gcc -Wall -Wextra -Werror -g -O0 -Iinclude $(find src -name '*.c') -lm \
    -o "$bin" || exit 2
repl_input=build/torture/repl.in
printf 'func f() { return 1; }\nx = 2;\nfunc g() { } func f() { }\nvar a = 1;\nvar b = a + f();\nvar c = zz;\nimport not_here;\nprog main() { }\n' > "$repl_input"

fail=0
# One case per line: name|arguments|stdin file ("-" = none; no arguments
# = REPL). A name starting with "fd64 " runs with only 64 file descriptors
# (to reach the paths taken when the system has none left).
while IFS='|' read -r name args input; do
    [ -z "$name" ] && continue
    set -- $args
    limit=$(ulimit -n)
    case "$name" in fd64\ *) limit=64 ;; esac
    if [ "$input" = "-" ]; then
        report=$(ulimit -n "$limit"; "$tool" "$bin" "$@" 2>&1 < /dev/null)
    else
        report=$(ulimit -n "$limit"; "$tool" "$bin" "$@" 2>&1 < "$input")
    fi
    report=$(printf '%s\n' "$report" | sed 's/\x1b\[[0-9;]*m//g')
    result=$(printf '%s\n' "$report" | grep "OVERALL RESULT")
    case "$result" in
        *"[OK]"*) echo "OK:   $name" ;;
        *)
            fail=$((fail + 1))
            echo "KO:   $name"
            if [ "$verbose" -eq 1 ]; then
                printf '%s\n' "$report"
            else
                printf '%s\n' "$report" | grep -E "│ #.*KO|Happens in"
            fi
            ;;
    esac
done <<EOF
file|examples/example7.pce|-
ast|--ast examples/example5.pce|-
tokens|--tokens examples/example3.pce|-
syntax error|tests/resolver/err_syntax_reaches_run.pce|-
resolver ok|tests/resolver/ok_globals.pce|-
resolver scopes|tests/resolver/ok_scopes.pce|-
resolver growth|tests/resolver/ok_many_names.pce|-
resolver expressions|tests/resolver/ok_expressions.pce|-
resolver arity error|tests/resolver/err_arity_create.pce|-
resolver scope error|tests/resolver/err_vemp_vs_var.pce|-
resolver error|tests/resolver/err_two_progs.pce|-
missing file|tests/does_not_exist.pce|-
imports|tests/imports/ok_import.pce|-
circular import|tests/imports/err_circular.pce|-
module syntax error|tests/imports/err_module_syntax.pce|-
missing module|tests/imports/err_no_module.pce|-
repl||$repl_input
eval strings||tests/eval/strings.pce
eval conversions||tests/eval/conversions.pce
eval globals||tests/eval/globals.pce
eval repl||tests/eval/repl.pce
eval methods||tests/eval/methods.pce
eval control flow||tests/eval/control_flow.pce
eval exit errors||tests/eval/exit_errors.pce
run fizzbuzz|tests/run/fizzbuzz.pce|-
run loops|tests/run/loops.pce|-
run locals|tests/run/locals.pce|-
run switch|tests/run/switch.pce|-
run error|tests/run/err_cond_not_bool.pce|-
run tuples|tests/run/tuples.pce|-
run unpack error|tests/run/err_unpack_count.pce|-
run functions|tests/run/functions.pce|-
run closures|tests/run/closures.pce|-
run traceback|tests/run/err_traceback.pce|-
run recursion|tests/run/err_recursion.pce|-
run default error|tests/run/err_in_default.pce|-
run prog args|tests/run/prog_args.pce|-
run exit code|tests/run/prog_return_code.pce|-
run exceptions|tests/run/exceptions.pce|-
run finally paths|tests/run/finally_paths.pce|-
run chained error|tests/run/err_chained.pce|-
run error in finally|tests/run/err_in_finally.pce|-
run error values|tests/run/error_values.pce|-
run input|tests/run/read_numbers.pce|tests/run/read_numbers.in
run input eof|tests/run/read_numbers.pce|-
eval exit through try||tests/eval/exit_through_try.pce
eval errors repl||tests/eval/errors_repl.pce
run gc error context|tests/run/gc_error_context.pce|-
run strings|tests/run/strings_unicode.pce|-
run string escapes|tests/run/string_escapes.pce|-
run invalid utf8 input|tests/run/input_invalid_utf8.pce|tests/run/input_invalid_utf8.in
eval multiline string||tests/eval/multiline_string.pce
run fstrings|tests/run/fstrings.pce|-
eval fstring repl||tests/eval/fstring_repl.pce
run lists|tests/run/lists.pce|-
run list methods|tests/run/list_methods.pce|-
run string split|tests/run/string_split.pce|-
run sort error|tests/run/err_sort_mixed.pce|-
run callback error|tests/run/err_callback_traceback.pce|-
run dicts sets|tests/run/dicts_sets.pce|-
run unhashable key|tests/run/err_key_unhashable.pce|-
run comprehensions|tests/run/comprehensions.pce|-
run nested comprehensions|tests/run/comprehensions_nested.pce|-
run aggregates chaining|tests/run/aggregates_chaining.pce|-
run id error|tests/run/err_id_number.pce|-
run objects|tests/run/objects.pce|-
run obj fields in create|tests/run/obj_fields_in_create.pce|-
run obj to_str|tests/run/obj_to_str.pce|-
run obj compare|tests/run/obj_compare.pce|-
run obj iter get set|tests/run/obj_iter_get_set.pce|-
run obj errors|tests/run/obj_errors.pce|-
run native modules|tests/run/native_modules.pce|-
native names|--check tests/resolver/native_names_ok.pce|-
native name taken|tests/imports/err_native_name_taken.pce|-
eval native repl||tests/eval/native_repl.pce
run sys|tests/run/sys_light.pce|-
run sys exit|tests/run/sys_exit_from_import.pce|-
eval sys repl||tests/eval/sys_repl.pce
run math|tests/run/math_module.pce|-
run io files|tests/run/io_files.pce|-
run io errors|tests/run/io_errors.pce|-
run io stdin|tests/run/io_stdin.pce|tests/run/io_stdin.in
fd64 run io many files|tests/run/io_many_files.pce 100|-
run random|tests/run/random_module.pce|-
run time|tests/run/time_module.pce|-
run os|tests/run/os_module.pce|-
run signal|tests/run/signal_module.pce|-
run signal uncaught|tests/run/signal_uncaught.pce|-
eval signal repl||tests/eval/signal_repl.pce
run json|tests/run/json_module.pce|-
run milestone word count|tests/run/word_count.pce tests/run/word_count.txt build/word_count.json|-
EOF
[ "$fail" -eq 0 ]
