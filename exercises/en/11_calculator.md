# Exercise 11: calculator

| | |
|---|---|
| File to hand in | `calculator.pce` |
| Uses | everything so far: a lexer, a recursive descent parser in an `obj`, a hierarchy of your own errors with fields, `super.create`, `except` with several types |
| In the manual | `man(errors)`, `man(super)`, `man(try)`, `man(number.pow)`, `man(number.sqrt)`, `man(OverflowError)`, `man(ZeroDivisionError)` |

A small interpreter written in your interpreter. There are no code hints here: only the specification.

## Statement

Read expressions from the standard input, one per line, and write the result of each one.

### The grammar

```
line       = NAME "=" expr | expr
expr       = term { ("+" | "-") term }
term       = unary { ("*" | "/" | "%") unary }
unary      = "-" unary | power
power      = primary [ "^" unary ]
primary    = NUMBER | NAME | NAME "(" expr ")" | "(" expr ")"
```

- `NUMBER`: digits, with an optional decimal point (`3`, `2.5`). It is converted with `num()`.
- `NAME`: a letter followed by letters, digits or `_`.
- `^` is the power and associates to the right (`2 ^ 3 ^ 2` is `2 ^ 9`). As in maths, `-2 ^ 2` is `-4`.
- Functions: `sqrt(x)` and `abs(x)`.
- The operators behave like pycee's: `/` always gives a float, `%` follows C and integers are exact, of 64 bits.

### What is written

- An expression writes its value as `print` writes it: `7`, `3.5`.
- An assignment `x = expr` keeps the value and writes `x = value`. Variables last until the end of the input.
- `pi` is already defined (`3.141592653589793`).
- Empty lines and lines starting with `#` are skipped (but they count for the line number).
- At the end: `L lines, E errors`. The exit code is **1** when there was any error and **0** otherwise.

### Errors

An error writes a message and the next line runs.

| Situation | Message |
|---|---|
| syntax error | `line N, column C: message` (`C` is the column of the token, from 1) |
| a character that is not in the grammar | `unexpected character '$'` |
| something is missing at the end | `missing a number`, or `expected ')' but found the end` |
| something is left over | `unexpected '5' at the end` |
| a number like `1.2.3` | `malformed number '1.2.3'` (column of the number) |
| a variable or function that does not exist | `line N: undefined variable: z`, `line N: unknown function: foo` |
| errors of the computation | `line N: Type: message`, with the type and the message of the pycee error (`ZeroDivisionError`, `ValueError`, `OverflowError`) |

### Design requirements

- A hierarchy of your own: `obj CalcError extends Error`; `CalcSyntaxError extends CalcError`, with a field `col` (its `create(msg, col)` calls `super.create(msg)`); and `CalcNameError extends CalcError`.
- The parser is an `obj` (with the list of tokens and the current position as fields) with one method per rule of the grammar.

## Example

Input:

```
1 + 2 * 3
(1 + 2) * 3
2 ^ 3 ^ 2
-2 ^ 2
2 ^ -1
y = 2 ^ 63
7 / 2
7 % 3
# a comment
x = 2 ^ 62
x + 1
r = sqrt(16) + abs(-3)
2 * pi * r
z * 2
3 +
(4 + 5
1 / 0
2 $ 3
sqrt(-1)
foo(2)
1.2.3
4 5
```

Output (exit code 1):

```
7
9
512
-4
0.5
line 6: OverflowError: integer overflow
3.5
1
x = 4611686018427387904
4611686018427387905
r = 7.0
43.982297150257104
line 14: undefined variable: z
line 15, column 4: missing a number
line 16, column 7: expected ')' but found the end
line 17: ZeroDivisionError: division by zero
line 18, column 3: unexpected character '$'
line 19: ValueError: the square root of a negative number is not a real number
line 20: unknown function: foo
line 21, column 1: malformed number '1.2.3'
line 22, column 3: unexpected '5' at the end
22 lines, 10 errors
```

`2 ^ 63` does not fit in a 64-bit integer: pycee raises an `OverflowError` instead of losing precision.

## Going further (optional)

- Functions defined by the user: `f(x) = x * x + 1` and then `f(3)`.
- Comparisons and the ternary: `x > 3 ? 1 : 0`.
