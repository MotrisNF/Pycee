# Exercise 06: grades

| | |
|---|---|
| File to hand in | `grades.pce` |
| Uses | `input()`, `try`/`except`, strings (`split`, `strip`), `num()`, lists, tuples, functions, f-strings with a format, `sort(key, reverse)`, comprehensions |
| Not used | `import` (no module) |
| In the manual | `man(input)`, `man(EOFError)`, `man(string.split)`, `man(num)`, `man(tuple)`, `man(fstrings)`, `man(list.sort)`, `man(comprehensions)` |

## Statement

Write a program that reads a list of students with their grade from the standard input, one per line, and shows a summary at the end.

Each valid line has **exactly two words**: the name and the grade.

```
ana 7.5
```

A grade is a number (integer or decimal) **from 0 to 10, both included**.

The program reads lines with `input()` until the input ends. At that point `input()` raises an `EOFError`.

### Rules

1. **Empty lines.** An empty line, or one with only spaces, is skipped silently.
2. **Invalid lines.** An invalid line is skipped with the warning `line N ignored: "text"`. A line is invalid when:
   - it does not have two words;
   - the grade is not a number;
   - the grade is out of 0..10.

   `N` is the number of the line (the first one is 1, and empty lines count too) and `text` is the line exactly as it was read.
3. **No valid line.** Write `no grades` and end with exit code **1**.
4. **Some valid line.** Write, in this order:
   - `students: X`: how many valid students there are.
   - `mean: M`: the mean of their grades, **with two decimals**.
   - `best: name (grade)`: the student with the highest grade. On a tie, the one that came **first**.
   - `passed: a, b, c`: the names with a grade ≥ 5, separated by `", "`, in the order they came.
   - The **ranking**: every student from the highest to the lowest grade (on a tie, in the order of the input), as `position. name grade`. The name is left-aligned in 8 characters and the grade right-aligned in 5.

   The exit code is **0**.

The grade is shown as the language shows it: `7` when it was an integer and `7.5` when it had decimals.

## Examples

With the input in a file: `./pycee grades.pce < input.txt`.

**Example 1.** This is the input (line 5 is empty):

```
ana 7.5
luis 5
pepe
marta 9.5

juan 3.25
sara eleven
leo 11
bea 9.5
```

This is the output:

```
line 3 ignored: "pepe"
line 7 ignored: "sara eleven"
line 8 ignored: "leo 11"
students: 5
mean: 6.95
best: marta (9.5)
passed: ana, luis, marta, bea
1. marta     9.5
2. bea       9.5
3. ana       7.5
4. luis        5
5. juan     3.25
```

The exit code is 0. Marta and Bea tie, but Marta goes first because she came earlier.

**Example 2.** This is the input (line 1 is empty):

```

hello
```

This is the output:

```
line 2 ignored: "hello"
no grades
```

The exit code is 1.

**Example 3.** This is the input:

```
ana 7
luis 4.5
```

This is the output:

```
students: 2
mean: 5.75
best: ana (7)
passed: ana
1. ana         7
2. luis      4.5
```

The exit code is 0.

## How to check it

```
printf 'ana 7.5\nluis 5\npepe\nmarta 9.5\n\njuan 3.25\nsara eleven\nleo 11\nbea 9.5\n' > input.txt
./pycee grades.pce < input.txt; echo "exit=$?"
```

The output must match the examples character by character.

## Hints (read them only if you get stuck)

<details>
<summary>Shape of the program</summary>

- A function `parse(line)` that gives back `tuple(name, grade)` when the line is valid, or `none` when it is not.
- In `prog main()`, a `while (true)` that reads with `input()` inside a `try`, and an `except EOFError { break; }` to get out.
- The exit code is what `prog main()` gives back with `return`.

</details>

<details>
<summary>Rules of the language you will bump into</summary>

- **`var` is not allowed inside a loop.** Declare it before the loop or use `vemp`, which lives only in its block: `vemp line = input();`.
- **Conditions need a bool.** `if (s)` with a tuple is an error: write `if (s is none)`.
- **There is no `==`.** Compare with `is` and `is not`.
- **`num("eleven")` raises a `ValueError`.** Catch it with `except ValueError { ... }`.
- **To unpack in a comprehension, write `for (name, grade in students)`,** without extra parentheses around `name, grade`.

</details>

<details>
<summary>Format and order</summary>

- `f"{x:.2f}"` gives two decimals. `f"{name:<8}"` left-aligns in 8 characters and `f"{grade:>5}"` right-aligns in 5.
- `l.copy().sort(key = (s) => s[1], reverse = true)` gives a copy sorted from highest to lowest without touching `l`. `sort` is **stable**: equal grades keep the order of the input.
- `l.enumerate()` gives `tuple(index, item)`, starting at 0.
- `", ".join(list)` joins strings.
- To write quotes inside a string: `"\""`.

</details>

## Going further (optional)

- Show the lowest grade and the median too.
- When a name is repeated, keep its last grade and warn with `repeated name: X`.
