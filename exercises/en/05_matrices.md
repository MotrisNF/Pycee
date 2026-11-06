# Exercise 05: matrices

| | |
|---|---|
| File to hand in | `matrices.pce` |
| Uses | lists of lists, nested comprehensions, `map`, `filter`, `sum`/`min`/`max`/`all` with a comprehension, `raise`, the ternary |
| In the manual | `man(list)`, `man(comprehensions)`, `man(list.map)`, `man(list.filter)`, `man(sum)`, `man(all)`, `man(raise)` |

Here the language starts to shine: with comprehensions and the list functions, loops are hardly needed. And another missing piece shows up: there is no `zip`.

## Statement

```
./pycee matrices.pce < matrix.txt
```

Read a matrix of integers from the standard input (a row per line, the numbers separated by spaces; empty lines are skipped) and write:

```
rows: R, columns: C
transpose:
<the transpose, each number right-aligned in 4 characters>
row sums: [...]
column sums: [...]
flat: [every number, row by row]
even: [the even numbers of the flat list, in the same order]
max: X min: Y
diagonals: D1 D2
magic square: yes
```

### Rules

1. **Diagonals and magic square** only make sense for a square matrix. When it is not square, the `diagonals` line is not written and the last line is `magic square: no (not square)`.
2. **`D1`** is the sum of the main diagonal (top left → bottom right) and **`D2`** the sum of the other one.
3. **It is a magic square** when every row, every column and both diagonals add up to the same. Write `magic square: yes` or `magic square: no`.
4. **Errors** (exit code **1**):
   - an item that is not an integer: `error: 'X' on line N is not an integer`;
   - a row with a different number of items than the first: `error: line N has A numbers, expected B`;
   - no rows at all: `empty matrix`.

   `N` counts every line of the input, the empty ones too.

## Examples

Input (line 3 is empty):

```
2 7 6
9 5 1

4 3 8
```

Output:

```
rows: 3, columns: 3
transpose:
   2   9   4
   7   5   3
   6   1   8
row sums: [15, 15, 15]
column sums: [15, 15, 15]
flat: [2, 7, 6, 9, 5, 1, 4, 3, 8]
even: [2, 6, 4, 8]
max: 9 min: 1
diagonals: 15 15
magic square: yes
```

Input `1 2 3` / `4 5 6`:

```
rows: 2, columns: 3
transpose:
   1   4
   2   5
   3   6
row sums: [6, 15]
column sums: [5, 7, 9]
flat: [1, 2, 3, 4, 5, 6]
even: [2, 4, 6]
max: 6 min: 1
magic square: no (not square)
```

Input `1 2` / `3 4` (square, but not magic):

```
rows: 2, columns: 2
transpose:
   1   3
   2   4
row sums: [3, 7]
column sums: [4, 6]
flat: [1, 2, 3, 4]
even: [2, 4]
max: 4 min: 1
diagonals: 5 5
magic square: no
```

Errors:

```
$ printf '1 2\n3 x\n' | ./pycee matrices.pce; echo "exit=$?"
error: 'x' on line 2 is not an integer
exit=1
$ printf '1 2\n3 4 5\n' | ./pycee matrices.pce; echo "exit=$?"
error: line 2 has 3 numbers, expected 2
exit=1
```

## Hints (read them only if you get stuck)

<details>
<summary>Reading and checking</summary>

- `line.split()` without an argument splits on any run of spaces and drops the gaps; an empty line gives `[]`.
- A function `read_matrix()` that raises a `ValueError` with the message (`raise ValueError(f"...")`) and a single `try`/`except ValueError (e)` in `main` that writes `e.message()` keeps the code tidy.

</details>

<details>
<summary>Without zip</summary>

- There is no `zip`: the transpose is done with indexes, in a nested comprehension: `[[m[i][j] for (i in range(rows))] for (j in range(cols))]`.
- The flat list, with two `for` in the same comprehension: `[x for (row in m) for (x in row)]`.

</details>

<details>
<summary>Sums and checks</summary>

- `m.map((r) => r.sum())` adds up every row.
- `sum(...)`, `max(...)`, `min(...)` and `all(...)` take a comprehension inside, without brackets: `sum(m[i][i] for (i in range(rows)))`. `all` stops at the first `false`.
- `all(s is target for (s in sums))` says whether they are all equal.

</details>

## Going further (optional)

- Multiply the matrix by its transpose.
- A "real" magic square also uses every number from 1 to n² exactly once: check it with a `set` (`man(set)`).
