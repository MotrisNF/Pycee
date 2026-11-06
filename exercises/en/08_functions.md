# Exercise 08: functions that make functions

| | |
|---|---|
| File to hand in | `functions.pce` |
| Uses | lambdas `(x) => ...` and with a block `(x) => { ... }`, closures, default values, named arguments, `...rest`, `f(...l)`, `reduce`, a `dict` as a cache |
| In the manual | `man(function)`, `man(func)`, `man(list.reduce)`, `man(list.map)`, `man(tuple)`, `man(dict)` |

In pycee functions are values: they are kept in variables, passed and given back. Closures capture **the variable**, not a copy of its value, so a function can carry its own state.

## Statement

Write these five functions. The main program is already written (below): copy it as it is at the end of your file, without changing it, and the output must be exactly the one of the example.

| Function | What it gives back |
|---|---|
| `counter(start = 0, step = 1)` | A function without arguments that, every time it is called, gives the next value: `start`, `start + step`, `start + 2 * step`... |
| `memo(f)` | A function of one argument that gives the same as `f`, but calls `f` **only once** for each different argument: the second time it gives back the saved result. |
| `compose(...fs)` | A function of one argument that applies the functions **from right to left**: `compose(f, g)(x)` is `f(g(x))`. With no functions, it gives back `x` as it is. |
| `repeat(f, n)` | A function of one argument that applies `f` `n` times in a row: `repeat(f, 3)(x)` is `f(f(f(x)))`. With `n` 0, it gives back `x`. |
| `partial(f, ...first)` | A function that takes the rest of the arguments and calls `f` with the `first` ones in front: `partial(f, 1)(2, 3)` is `f(1, 2, 3)`. |

The main program:

```
# ---- given: do not change below ----
var calls = 0;
var fib = none;

func slow_fib(n) {
    calls++;
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

prog main() {
    var c = counter();
    var d = counter(10, step = -5);
    print(c(), c(), c(), d(), d(), d());

    fib = memo(slow_fib);
    print(fib(80), "calls:", calls);
    print(fib(80), "calls:", calls);

    var inc = (x) => x + 1;
    var double = (x) => x * 2;
    print(compose(inc, double)(5), compose(double, inc)(5), compose()(5));
    print(repeat(double, 10)(1), repeat(inc, 0)(7));

    var add3 = (a, b, c) => a + b + c;
    print(partial(add3, 1)(2, 3), partial(add3, 1, 2, 3)(), partial(add3)(4, 5, 6));

    var fs = [];
    for (i in range(3)) {
        fs.append(() => i * i);
    }
    print(fs.map((f) => f()));
    return 0;
}
```

## Example

```
$ ./pycee functions.pce
0 1 2 10 5 0
23416728348467685 calls: 81
23416728348467685 calls: 81
11 12 5
1024 7
6 6 15
[0, 1, 4]
```

Notice three things in this output:

- `fib(80)` comes out at once and with **81 calls**: without `memo` it would take billions. Integers are exact up to 2⁶³ − 1; above that, `OverflowError` (they are never silently turned into floats).
- The second time, `calls` does not change: the result came from the cache.
- The last line is `[0, 1, 4]`: every round of the `for` makes a new `i`, and each lambda captures its own. (In Python it would be `[4, 4, 4]`.)

## Hints (read them only if you get stuck)

<details>
<summary>State in a closure</summary>

- A variable of the outer function that the lambda changes stays alive after the outer function ends:

  ```
  func counter(start = 0, step = 1) {
      var next = start;
      return () => {
          vemp value = next;
          next += step;
          return value;
      };
  }
  ```

- A lambda with a block `{ ... }` needs `return`; an expression one, `(x) => x + 1`, does not.

</details>

<details>
<summary>memo</summary>

- Keep the results in a `dict` made inside `memo` and captured by the lambda. `n in cache` says whether it is already there.

</details>

<details>
<summary>compose and partial</summary>

- `...fs` collects the arguments in a **tuple**. `fs[::-1]` gives it reversed and `reduce((acc, f) => f(acc), x)` applies them one by one starting from `x`.
- **Tuples do not concatenate with `+`.** To put `first` and `rest` together, turn them into lists: `first.to_list() + rest.to_list()`, and spread the result in the call with `f(...list)`.

</details>

<details>
<summary>Names</summary>

- A lambda may have a parameter named like a variable of the function around it (`(a, b, c)` while `main` has a `c`): inside the lambda, `c` is its parameter. What is not allowed is repeating a name inside the same function.

</details>

## Going further (optional)

- `memo` only takes functions of one argument. Make it take several (`...args`): what would you use as the key of the dict, when a list cannot be one but a tuple can? (`man(dict)`)
- Write `pipe(...fs)`, which applies from left to right, using `compose`.
