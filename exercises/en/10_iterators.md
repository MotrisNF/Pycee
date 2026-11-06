# Exercise 10: types of your own

| | |
|---|---|
| File to hand in | `iterators.pce` |
| Uses | the special methods `len`, `get`, `set`, `iter` / `has_next` / `next`, `to_str`, `compare`; `...rest`; several values at once (`a, b = b, a + b`) |
| In the manual | `man(obj)`, `man(for)`, `man(types)`, `man(operators)`, `man(IndexError)` |

An `obj` can behave like a type of the language: be indexed with `x[i]`, walked with `for`, shown with `print` and compared with `<`. What pycee does **not** have is overloading of `+`, `-` or `*`: methods are used for that.

## Statement

Write three `obj`. The main program is already written (below): copy it at the end of your file without changing it.

**`Range(start, stop, step = 1)`**, like `range`, but taking decimals too.

- `len()` is how many values it gives: `ceil((stop - start) / step)`, or 0 when that is negative.
- `r[i]` is `start + i * step`; a negative `i` counts from the end. Out of range, `IndexError` with the message `index I out of a Range of N`.
- `for (x in r)` gives its values in order.
- It is shown as `Range(0, 1, 0.25)`.
- A `step` of 0 is a `ValueError` with the message `the step cannot be 0`.

**`Fib(limit)`**: a `for` over it gives the Fibonacci numbers (0, 1, 1, 2, 3...) that do not go above `limit`. It can be walked more than once, and each time it starts over.

**`Vector(...xs)`**, a vector of any number of components.

- `len()`, `v[i]` and `v[i] = x` on its components.
- `norm()` is the square root of the sum of the squares.
- `add(w)` gives a new `Vector`, component by component. With a different size, `ValueError` with the message `different sizes: A and B`.
- Two vectors compare by their norm (`v > w`, and `sort` orders them from the smallest to the largest norm).
- It is shown as `<3, 4>`.

The main program:

```
# ---- given: do not change anything below ----
prog main() {
    var r = Range(0, 1, 0.25);
    print(r, r.len(), r[0], r[-1], [x for (x in r)]);
    print([x for (x in Range(10, 0, -3))], Range(5, 5).len());
    try {
        print(r[4]);
    } except IndexError (e) {
        print("IndexError:", e.message());
    }
    try {
        Range(0, 1, 0);
    } except ValueError (e) {
        print("ValueError:", e.message());
    }

    var f = Fib(100);
    print([x for (x in f)]);
    print(sum(x for (x in f) if (x % 2 is 0)));

    var v = Vector(3, 4);
    var w = Vector(1, 1);
    print(v, v.len(), v.norm(), v.add(w), v > w);
    v[0] = 0;
    print(v, [Vector(5), Vector(-1), Vector(2, 2)].sort());
    try {
        v.add(Vector(1));
    } except ValueError (e) {
        print("ValueError:", e.message());
    }
    return 0;
}
```

## Example

```
$ ./pycee iterators.pce
Range(0, 1, 0.25) 4 0.0 0.75 [0.0, 0.25, 0.5, 0.75]
[10, 7, 4, 1] 0
IndexError: index 4 out of a Range of 4
ValueError: the step cannot be 0
[0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89]
44
<3, 4> 2 5.0 <4, 5> true
<0, 4> [<-1>, <2, 2>, <5>]
ValueError: different sizes: 2 and 1
```

`r[0]` is `0.0` and not `0` because `0 + 0 * 0.25` already is a float. The second time `f` is walked (in the `sum`), it starts from 0 again.

## Hints (few)

<details>
<summary>How the language uses your methods</summary>

- `for (x in obj)` calls `obj.iter()`, and on what it gives back, `has_next()` (which must give a bool) and `next()` on every round. `iter()` can give back another object (a "cursor" with its position) or the object itself, reset.
- `obj[i]` calls `get(i)` and `obj[i] = v` calls `set(i, v)`.
- `compare(other)` gives back a number: negative when `self` goes first, 0 when they are equal, positive when it goes after. With it, `<`, `>`... and `sort` work.
- Remember that fields are made in `create`: an `iter()` that resets a field can change it, but the field must exist from `create` on.

</details>

## Going further (optional)

- Give `Range` a method `reversed()` that gives back another `Range` with the same values in reverse order.
- A `Matrix(rows, columns)` whose `for` gives each row as a `Vector`.
