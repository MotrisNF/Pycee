# pycee exercises

Twelve exercises to learn pycee by using it, from the first `prog` to a program split into modules that keeps its state in a file. They are for people who already program: they do not explain what a loop is, but they do explain how pycee differs from what you know, its strong points and also its weak ones.

| # | Exercise | What it brings in |
|---|---|---|
| 01 | [greet](01_greet.md) | `prog`, `argv`, `for`, f-strings, exit codes |
| 02 | [fizzbuzz](02_fizzbuzz.md) | `var` and `vemp`, conditions that need a bool, `int()` and `try` |
| 03 | [primes](03_primes.md) | fixed-size lists, `while`, comprehensions, output with a width |
| 04 | [caesar](04_caesar.md) | strings, `const`, reading until `EOFError`, the C-style `%` |
| 05 | [matrices](05_matrices.md) | nested comprehensions, `map` / `filter`, `sum` / `all` over a comprehension, no `zip` |
| 06 | [grades](06_grades.md) | parsing input, tuples, `sort(key, reverse)`, formatting |
| 07 | [words](07_words.md) | `dict`, stable sorting by two criteria (tuples do not compare) |
| 08 | [functions](08_functions.md) | lambdas, closures, `...rest`, functions that make functions |
| 09 | [bank](09_bank.md) | `obj`, inheritance, `private`, `static`, your own errors |
| 10 | [iterators](10_iterators.md) | special methods: `len`, `get`/`set`, `iter`, `compare`, `to_str` |
| 11 | [calculator](11_calculator.md) | a parser and an error hierarchy: everything together |
| 12 | [tasks](12_tasks.md) | modules, files, `json`, `os`, `sys`, stderr |

## How to work

- Do them in order: each one uses what the previous ones brought in. The first ones take you by the hand; from 09 on, the hints get fewer and the statement only says what must happen.
- Every exercise has examples with their exact output. Your program is right when it gives that output **character by character**, and the same exit code (`echo "exit=$?"`). There is no automatic checker: run the examples yourself.
- The reference is the manual: start `./pycee` and type `man(name)` (`man(list.sort)`, `man(fstrings)`, `man()` for the index). Every exercise lists the entries it needs.
- Read the hints only when you get stuck; they are folded on purpose.
- The solutions are not in the repository.

There is a Spanish version of the same exercises in [../es/](../es/README.md).
