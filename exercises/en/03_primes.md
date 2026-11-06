# Exercise 03: primes

| | |
|---|---|
| File to hand in | `primes.pce` |
| Uses | fixed-size lists (`var l[n]`), `while`, `range` with a step, comprehensions with `if`, `enumerate()`, f-strings with a width, `join` |
| In the manual | `man(var)`, `man(while)`, `man(range)`, `man(comprehensions)`, `man(enumerate)`, `man(fstrings)`, `man(join)` |

The sieve of Eratosthenes, as an excuse to work with lists and with the shape of the output.

## Statement

```
./pycee primes.pce N
```

Write every prime up to `N` (included), then a summary.

### Rules

1. **The primes** go in rows of 10, each number right-aligned in 5 characters. The last row may have fewer.
2. **The summary**, in this order:
   - `total: K`: how many primes there are.
   - `largest: P`: the largest of them.
   - `twins: G -> (p, q) (p, q) ...`: the pairs of consecutive primes 2 apart, in order. With none, just `twins: 0`.
3. **No primes** (when `N` is less than 2, negative included): write `no primes up to N`, exit code **0**.
4. **Arguments.** With a number of arguments other than 1, `usage: primes.pce N` (code **2**). When the argument is not an integer, `error: 'X' is not an integer` (code **1**).

## Examples

```
$ ./pycee primes.pce 50
    2    3    5    7   11   13   17   19   23   29
   31   37   41   43   47
total: 15
largest: 47
twins: 6 -> (3, 5) (5, 7) (11, 13) (17, 19) (29, 31) (41, 43)
```

```
$ ./pycee primes.pce 2
    2
total: 1
largest: 2
twins: 0
$ ./pycee primes.pce 1
no primes up to 1
$ ./pycee primes.pce seven; echo "exit=$?"
error: 'seven' is not an integer
exit=1
```

Up to 1000 there are 168 primes; the last one is 997 and there are 35 pairs of twins.

## Hints (read them only if you get stuck)

<details>
<summary>The sieve</summary>

- `var is_prime[n + 1];` makes a list of `n + 1` items, all `none`. Fill it with `is_prime[i] = i >= 2;`.
- For each `i` from 2 while `i * i <= n`: if `is_prime[i]`, mark its multiples from `i * i` as `false`. `range(i * i, n + 1, i)` gives all of them.
- At the end, a comprehension with `if` takes the primes out: `[k for (k in range(n + 1)) if (is_prime[k])]`.

</details>

<details>
<summary>Rows of 10</summary>

- `f"{p:>5}"` right-aligns in 5 characters.
- `for (i, p in primes.enumerate())` gives the position and the value at once. A row ends when `i % 10 is 9`, or at the last prime.
- Build the row in a string (`line += f"{p:>5}";`) and write it when it ends.

</details>

<details>
<summary>The twins</summary>

- Walk the positions `i` from 0 to `len - 2` and compare `primes[i + 1] - primes[i] is 2`.
- You can build the texts `"(3, 5)"` straight away in a comprehension and join them with `" ".join(...)`.

</details>

## Going further (optional)

- Time it with `N` = 1 000 000 (`time ./pycee-release primes.pce 1000000 > /dev/null`). How much memory does it use? Try `import sys` and `sys.memory()` (`man(sys)`).
- Write only the primes that are palindromes (`man(string.reverse)`).
