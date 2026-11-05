# Types of pycee

Design reference for every built-in type: what it is, how it is written and shown, which operators it accepts and which methods it has. Like [EBNF.md](EBNF.md), this is the reference the implementation follows.

**Status:** structural rules decided by the author on 2026-11-05 (see the last section). The method lists are a proposal for the author to review; they can still grow later without breaking anything, since each type has its own method table.

## Every value

| Method | Result |
|---|---|
| `x.type()` | Name of its type: `"number"`, `"string"`, `"bool"`, `"none"`, `"list"`, `"tuple"`, `"set"`, or the `obj` name (`"Point"`) |
| `x.is_type(name)` | `true` if `x.type() is name`; for objects, also for any ancestor (inheritance) |
| `x.id()` | Which value it is: the same for every name holding it (`var b = a;` → `a.id() is b.id()`), another for a copy. A number from 1 up, in order of creation, never reused. Numbers, bools and `none` have none (`TypeError`). An `obj` may have its own field or method `id`, which hides this one |

Global functions (the rest of the standard library lives in modules: `import math`, `import sys`, `import io`): `print(..., sep = " ", end = "\n")`, `input()` (`EOFError` at the end of input; on a terminal, the next call waits again after Ctrl+D), `str(x)`, `num(x)`, `int(x)`, `float(x)`, `range(...)` (as in Python: a value of type `range` that makes its numbers as it is walked; integers only, step `0` is `ValueError`; shown as `range(0, 5)`), `tuple(...)`, `set(...)`, `dict(...)`, `any(x)`, `all(x)`, `sum(x)`, `min(x)`, `max(x)` (on anything a `for` walks, as the method of that name on its items; or with a comprehension alone inside, `any(x > 3 for (x in l))`, `sum(x * x for (x in l))`: no list is made for `any`/`all`, which stop as soon as they know the answer). These names are reserved like the other global functions. Errors: `Error(msg)` and one constructor per type (`TypeError`, `ValueError`, `ZeroDivisionError`, `OverflowError`, `IndexError`, `KeyError`, `AttributeError`, `AssignmentError`, `RuntimeError`, `RecursionError`, `NotImplementedError`, `EOFError`, `AssertionError`); every one is an `Error`. An error value has `e.type()` ("ValueError"), `e.message()`, `e.line()`, `e.file()`, `e.traceback()`; `print(e)` shows `ValueError: msg`; `e.is_type("Error")` is true. Only in the REPL: `exit()` / `exit(n)` (`n` an integer from 0 to 255); in files, `sys.exit(n)`.

Lists, sets, dicts and objects are **shared**, never copied implicitly: after `var b = a;`, or when passed to a function, both names see the same value (`copy()` makes a shallow copy).

`is` / `is not` compare by value, except `obj` instances, which compare by identity. Two values of different types are never equal (`1 is "1"` is `false`). `<`, `<=`, `>`, `>=` between different types are a `TypeError`. Conditions only accept `true` or `false`. No implicit conversions: `"a" + 1` and `true + 1` are `TypeError`.

`x in c` / `x not in c` (strings, lists, tuples, sets): `true` if some element of `c` `is` x; for strings, if x is a substring. Same precedence as the comparisons, not chainable.

**Indexing and slicing** (strings, lists, tuples):
- `c[i]`: `i` must be an integer. Negative indexes count from the end (`c[-1]` is the last). Out of range on either side is `IndexError`.
- `c[start:end]`, `c[start:end:step]`: a new value of the same type. Each part is optional (`s[:5]`, `l[2:]`, `l[::-1]`); negative values count from the end; out-of-range bounds are clipped, never an error. A `step` of `0` is `ValueError`. A slice cannot be assigned to (`l[1:3] = ...` is a `SyntaxError`).

**Repetition** (strings, lists, tuples): `"ab" * 3` is `"ababab"`, `[0] * 5` is `[0, 0, 0, 0, 0]` (either order: `3 * "ab"`). The number must be an integer ≥ 0 (`ValueError` otherwise). For a list of lists the inner lists are shared, as in Python.

## number

Two kinds, both of type `"number"`: **int** (64-bit, exact) and **float** (IEEE 754 double). `x.is_int()` tells them apart. Literals: `7`, `1_000_000`, `0xFF`, `0b1010`, `0o17` are ints; `7.0`, `2.5e-3`, `1e9` are floats. An int literal that does not fit in 64 bits is a `SyntaxError`. Shown: an int as `7`, a float as `7.0` or `3.5`. `7 is 7.0` is `true` (same number).

| Operators | |
|---|---|
| `+ - *` | int with int gives int (overflow is `OverflowError`); any float gives float |
| `/` | always float: `6 / 3` is `2.0`; by zero is `ZeroDivisionError` |
| `//` | truncated division, as in C (`-7 // 2` is `-3`); floats allowed (`7.5 // 2` is `3.0`) |
| `%` | remainder of the truncated division, as in C (`-7 % 2` is `-1`); floats allowed (`fmod`) |
| `& \| ^ ~ << >>` | ints only (`TypeError` with floats) |
| `< <= > >=`, `is` | int and float compare by value |
| `-x`, `++`, `--` | |

Conversions (functions): `int(x)` (truncates a float; parses a string), `float(x)`, `num(s)` (int or float depending on the text), `str(x)`.

| Method | Result |
|---|---|
| `pow(exp)` | `2.pow(3)` is `8` (int when both are ints and `exp >= 0`) |
| `abs()` | |
| `round()`, `floor()`, `ceil()` | ints; `round` goes half away from zero (`2.5` → `3`, `-2.5` → `-3`) |
| `sqrt()` | float; `ValueError` for negatives |
| `is_int()` | `true` for an int |

## string

Immutable Unicode text, stored as UTF-8. Length, indexes and slices count **characters**, not bytes: `"naïve".len()` is `5` and `"naïve"[2]` is `"ï"` (ASCII-only strings take a fast path). Written `"..."` or `'...'`; escapes `\n \t \r \0 \" \' \\ \xHH \u{XXXX}`; multi-line `"""..."""`; raw `r"..."` (no escapes); f-strings `f"total: {n}"` (any expression, converted with `str()`; `{{` and `}}` for braces), with Python's format specifiers after `:` (`f"{price:.2f}"`, `f"{n:>5}"`, `f"{n:,}"`, `f"{n:x}"`). A string may contain `\0`. Shown raw by `print`, and with double quotes inside containers and in the REPL echo: `["a", "b"]`.

| Operators | |
|---|---|
| `+` | concatenation (string + string only) |
| `*` | repetition |
| `s[i]`, `s[a:b:c]` | `s[i]` is a string of length 1 |
| `< <= > >=` | byte order |
| `is` | same bytes |

| Method | Result |
|---|---|
| `len()` | |
| `upper()`, `lower()` | new string |
| `capitalize()` | first character upper case, the rest lower case (as Python) |
| `strip()`, `lstrip()`, `rstrip()` | without spaces, tabs and newlines at both ends / the left / the right; `strip(chars)` (and the others) remove those characters instead |
| `split()`, `split(sep)` | list of strings; with no `sep`, splits on any run of whitespace and drops the empty pieces (`"  a  b ".split()` is `["a", "b"]`) |
| `join(collection)` | `", ".join(l)`; the same as `l.join(", ")` (both forms exist) |
| `replace(old, new)` | every occurrence |
| `find(sub)` | first index, or `-1` |
| `index(sub)` | first index, `ValueError` if it is not there |
| `starts_with(s)`, `ends_with(s)` | |
| `count(sub)` | |
| `reverse()` | the characters in reverse order |
| `is_digit()`, `is_alpha()`, `is_alnum()`, `is_space()`, `is_ascii()` | whether every character is one (`false` for `""`, except `is_ascii`) |
| `to_list()` | list of its characters |

More methods can be added later (decided: the list stays open).

- `x in s`: substring (`"ell" in "hello"`). Text that is not valid UTF-8 (from `input()` or a file) has each bad byte replaced by U+FFFD "�": reading never fails because of it.

## bool and none

`true`, `false` and `none`. Only the common methods. `and`, `or`, `xor`, `not` take bools only.

## list

Mutable, ordered, any values (mixed types allowed). Written `[1, "a", [2]]`; `var l[n]` makes `n` elements set to `none`. Shown `[1, "a", [2]]`.

| Operators | |
|---|---|
| `+` | concatenation into a new list |
| `*` | repetition |
| `l[i]`, `l[i] = x`, `l[a:b:c]` | |
| `is` | same length and every element `is` the other |

| Method | Result |
|---|---|
| `len()` | |
| `append(x)`, `insert(i, x)`, `extend(x)` | `extend` adds the items a `for` over `x` gives (any iterable, also the list itself) |
| `pop()`, `pop(i)` | removes and returns the last (or the one at `i`, negative counting from the end); `IndexError` if empty or out of range |
| `remove(x)` | first element that `is` x; `ValueError` if none |
| `clear()` | |
| `index(x)`, `count(x)` | `index` gives `ValueError` if absent |
| `sort()`, `reverse()` | in place; `sort` needs comparable elements |
| | **Every method that changes the list (`append`, `insert`, `extend`, `remove`, `clear`, `sort`, `reverse`) gives back the list itself**, so they chain: `l.append(3).sort()`. A sorted copy, leaving `l` as it is: `l.copy().sort()`. `pop` gives the item. The same for sets (`add`, `remove`, `clear`) and dicts (`remove`, `clear`) |
| `join(sep)` | strings only: `["a", "b"].join(",")` is `"a,b"` (also `",".join(l)`) |
| `map(f)`, `filter(f)` | new list: `l.map((x) => x * 2)`, `l.filter((x) => x > 0)` |
| `reduce(f, start)` | one value: `l.reduce((acc, x) => acc + x, 0)` |
| `sort(key = f, reverse = true)` | `key` and `reverse` optional (named arguments) |
| `any(f)`, `all(f)` | bool (`f` must give a bool); without `f`, `any()` / `all()`, each element must be a bool: `[x > 3 for (x in l)].any()`. `each(f)` calls `f` with every element |
| `sum()`, `min()`, `max()` | also on tuples and sets |
| `enumerate()` | list of `tuple(index, element)`: `for (i, x in l.enumerate())` (also on tuples and strings) |
| `copy()` | shallow copy |
| `to_tuple()`, `to_set()` | |

## tuple

An immutable list: same operators and methods, except those that change it (`append`, `insert`, `extend`, `pop`, `remove`, `clear`, `sort`, `reverse`, `t[i] = x`), which do not exist (`AttributeError` / `TypeError`). Built with `tuple(1, 2, 3)` (no literal). Shown `tuple(1, 2, 3)`. `to_list()`, `to_set()`.

## dict

Keys to values, in **insertion order**. Built with its name and `key: value` pairs, like every structure except lists: `dict("a": 1, "b": 2)`, `dict()`, or a comprehension `dict(k: v for (k in keys))`. Keys must be immutable (as set elements). Shown `dict("a": 1, "b": 2)`.

| Operators | |
|---|---|
| `d[k]` | `KeyError` if absent |
| `d[k] = v` | adds or replaces |
| `k in d` | key present |
| `is` | same pairs, any order |

| Method | Result |
|---|---|
| `len()` | |
| `get(k, default)` | `d[k]`, or `default` if absent (no error) |
| `keys()`, `values()`, `items()` | lists (`items` gives tuples `tuple(k, v)`) |

- Walking a dict (decided by the author): `for (x in d)` gives each pair as a new dict of length 1 (`dict("a": 1)`); `for (k, v in d)` unpacks key and value; `for (x in d.keys())` / `d.values()` give each one by one. `k in d` is still "key present".
| `remove(k)` | `KeyError` if absent |
| `clear()`, `copy()` | |

## set

Unordered collection without repeats. Built with `set(1, 2, 3)` (no literal); a repeated value is kept once. Shown `set(1, 2, 3)`.
- **Order is not guaranteed**: iterating, printing or `to_list()` may give the elements in any order (it depends on their hash). Tests must not rely on it.
- **Only immutable values** can be elements: number, string, bool, none, and tuples whose elements are themselves immutable. A list, a set or an `obj` inside a set is a `TypeError` (if it changed afterwards the set would be corrupt).
- `is` between sets: same elements, in any order. No indexing, slicing or repetition. `to_list()`, `to_tuple()`.

| Method | Result |
|---|---|
| `len()` | |
| `add(x)`, `remove(x)` | `remove` gives `ValueError` if absent |
| `has(x)` | |
| `union(s)`, `intersection(s)`, `difference(s)` | new sets |
| `clear()`, `copy()` | |

## function

Functions are values: named ones (`var f = double;`; the name `double` itself is a constant and cannot be reassigned, so `double(...)` has its arity checked before running), anonymous ones (`func (x) { return x * 2; }`, `(x) => x * 2`), natives (`l.map(str)`) and bound methods (`var mover = p.move;` remembers `p`). Closures capture variables, not copies. Calling a value that is not a function is a `TypeError`; the number of arguments of a call through a value is checked when it runs. `is` compares by identity. Shown `<func double>`, `<func lambda>` (anonymous), `<built-in print>`, `<method Point.move>`. `.type()` is `"function"` for all of them.

## obj (user objects)

Fields are declared by assigning them in `create` (`self.x = x;`, anywhere in its body); the other methods change them but cannot add new ones (an `AttributeError` before running). A `var` in the obj is still possible (needed for `private var`, `static var`, or a field with an initial value and no `create`). Methods with `func`, `create` as constructor. They have the common methods (`p.type()` is `"Point"`) and their own. Shown `<Point>`, or what its `to_str()` returns. `is` compares by identity: `Point(0, 0) is Point(0, 0)` is `false`; `p is p` is `true`.

Special methods the language uses when they exist (implemented in phase 9; to_str must give back a string, compare a number, otherwise a TypeError; has_next a bool): `to_str()` (`print`, `str`, f-strings), `compare(other)` (negative, `0` or positive: `<`, `<=`, `>`, `>=` and `sort()`), `len()`, `get(i)` / `set(i, v)` (`obj[i]`, `obj[i] = v`), `iter()` returning a value with `has_next()` and `next()` (`for (x in obj)`). No operator overloading for `+ - *`.

Fields that are not `private` are read and written from outside (`p.x = 3;`); assigning a field that is not declared is an `AttributeError`. `private var` / `private func`: only usable through `self` inside the obj. `static var` / `static func`: belong to the obj, not to each object (`Point.count`, `Point.origin()`), and can also be reached through an object (`p.count` is the same `Point.count`). Inheritance: single, `obj Circle extends Shape` or, the same, `obj Circle(Shape)`; methods are looked up in the child, then its parent; `super.create(...)` and `super.m(...)` reach the parent's (a child without `create` uses its parent's; with one, the parent's runs only if it is called); `is_type` sees the whole chain.

Errors of your own: an obj can extend `Error` or any error type (`obj ParseError extends ValueError { }`); `raise ParseError("bad")` works, and `except ValueError` catches it too. It has the methods of an error (`message()`, `line()`...) and can add fields and methods.

## Structural decisions (author, 2026-11-05)

| Question | Decision |
|---|---|
| Type of a value | Every value carries its type; `x.type()` gives its name and `x.is_type(name)` checks it |
| Negative indexes | Yes, as in Python (`l[-1]`) |
| Slicing | `[start:end]` and `[start:end:step]`, each part optional; new syntax |
| Membership | `in` / `not in` operators; new syntax |
| `is` on `obj` | By identity |
| Set elements | Immutable values only (tuples of immutables included) |
| Set order | Not guaranteed |
| Showing tuples and sets | `tuple(1, 2)`, `set(1, 2)`; strings inside containers with double quotes |
| Repetition with `*` | Strings, lists and tuples |
| Conversions | `to_list()`, `to_tuple()`, `to_set()` methods (proposal) |
| Numbers | int (64-bit, `OverflowError`) and float, both `"number"`; int shown `7`, float `7.0` |
| Sharing | Lists, sets, dicts and objects are shared, never copied implicitly |
| Functions | Values, with lambdas and closures |
| dict | Built `dict("a": 1)`, insertion order |
| Strings | Unicode characters (UTF-8) |
| `"a" + 1` | `TypeError`; `str()` converts |

Still open: whether `l[1:3] = ...` (assigning a slice) is ever allowed (now a `SyntaxError`), and the review of the method lists above.
