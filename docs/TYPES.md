# Types of pycee

Design reference for every built-in type: what it is, how it is written and shown, which operators it accepts and which methods it has. Like [EBNF.md](EBNF.md), this is the reference the implementation follows.

**Status:** structural rules decided by the author on 2026-11-05 (see the last section). The method lists are a proposal for the author to review; they can still grow later without breaking anything, since each type has its own method table.

## Every value

| Method | Result |
|---|---|
| `x.type()` | Name of its type: `"number"`, `"string"`, `"bool"`, `"none"`, `"list"`, `"tuple"`, `"set"`, or the `obj` name (`"Point"`) |
| `x.is_type(name)` | `true` if `x.type() is name`; for objects, also for any ancestor (inheritance) |
| `x.id()` | Which value it is: the same for every name holding it (`var b = a;` → `a.id() is b.id()`), another for a copy. A number from 1 up, in order of creation, never reused. Numbers, bools and `none` have none (`TypeError`). An `obj` may have its own field or method `id`, which hides this one |

Global functions (the rest of the standard library lives in modules: `import math`, `import sys`, `import io`; see "Modules of the language" below): `print(..., sep = " ", end = "\n", file = f)` (`sep` and `end` keyword-only strings; `file` an open file for writing, such as `io.stderr`), `input()` (`EOFError` at the end of input; on a terminal, the next call waits again after Ctrl+D), `str(x)`, `num(x)`, `int(x)`, `float(x)`, `range(...)` (as in Python: a value of type `range` that makes its numbers as it is walked; integers only, step `0` is `ValueError`; shown as `range(0, 5)`), `tuple(...)`, `set(...)`, `dict(...)`, `any(x)`, `all(x)`, `sum(x)`, `min(x)`, `max(x)` (on anything a `for` walks, as the method of that name on its items; or with a comprehension alone inside, `any(x > 3 for (x in l))`, `sum(x * x for (x in l))`: no list is made for `any`/`all`, which stop as soon as they know the answer). These names are reserved like the other global functions. Errors: `Error(msg)` and one constructor per type (`TypeError`, `ValueError`, `ZeroDivisionError`, `OverflowError`, `IndexError`, `KeyError`, `AttributeError`, `AssignmentError`, `RuntimeError`, `RecursionError`, `NotImplementedError`, `EOFError`, `AssertionError`, `IOError` and its two children `FileNotFoundError` and `PermissionError`); every one is an `Error`. `KeyboardInterrupt` (Ctrl+C) is **not** an `Error`: only an `except KeyboardInterrupt` catches it, never a bare `except` nor `except Error`, so a program can always be stopped; uncaught, it shows its traceback and the program ends with code 130 (in the REPL, back to the prompt). An error with no message is shown with its type alone (`ValueError`, `KeyboardInterrupt`). An error value has `e.type()` ("ValueError"), `e.message()`, `e.line()`, `e.file()`, `e.traceback()`; `print(e)` shows `ValueError: msg`; `e.is_type("Error")` is true. Only in the REPL: `exit()` / `exit(n)` (`n` an integer from 0 to 255); in files, `sys.exit(n)`.

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
| `pow(exp)` | `2.pow(3)` is `8` (int when both are ints and `exp >= 0`); a result too large is an `OverflowError` (int or float) |
| `abs()` | |
| `round()`, `floor()`, `ceil()`, `trunc()` | ints; `round` goes half away from zero (`2.5` → `3`, `-2.5` → `-3`), `trunc` toward zero (`-2.7` → `-2`) |
| `round(digits)` | a float with that many decimals, half away from zero (`3.14159.round(2)` is `3.14`; `0.125.round(2)` is `0.13`; `2.675.round(2)` is `2.67`, since that float is really 2.67499...); negative digits round to tens, hundreds... (`1234.5.round(-2)` is `1200.0`) |
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

## file

An open file, from `io.open(path, mode)`, or one of the standard streams `io.stdin`, `io.stdout`, `io.stderr`. Shown `<file "a.txt">` (`<file "<stdin>">`); `.type()` is `"file"`. Text is UTF-8: what is read is cleaned like `input()` (an invalid byte becomes U+FFFD). `is` compares by identity; a file cannot go in a set or be a dict key.

| Method | Result |
|---|---|
| `read()` | Everything left, as one string (`""` at the end) |
| `read_line()` | The next line **with its `"\n"`** (the last one may have none), or `""` at the end |
| `read_lines()` | A list of the lines left, each with its `"\n"` |
| `write(s)` | Writes the string `s` (anything else is a `TypeError`: `f.write(str(5))`); gives back the file, so writes chain: `f.write("a").write("b")` |
| `has_next()` | `true` if a read would give something |
| `flush()` | What was written goes out now; gives back the file |
| `close()` | Closes it (`none`); closing it again does nothing. What could not be written (a full disk) is an `IOError` here |
| `is_closed()` | |

`for (line in f)` walks the lines left, each with its `"\n"`. Reading a file opened for writing (or writing one opened for reading) is an `IOError`, and so is using a closed file. A file nobody can reach any more is closed by the collector, and every file still open is closed when the program ends, so what was written reaches the disk even without `close()`; when the system runs out of file descriptors, a collection runs first to close the unreachable ones. The standard streams are never closed: `io.stdout.close()` is an `IOError`.

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

## Modules of the language

Imported like the user's own (`import math;`, `from math import sqrt as root;`); see [EBNF.md](EBNF.md), "Modules", for the rules. Their functions are values, shown `<built-in math.sqrt>`; their constants are read-only. Members are added to this section as each one is written (phase 10); until then a member exists (its name and arity are known) but calling it is a `NotImplementedError`.

| Module | Members |
|---|---|
| `sys` | `argv`, `platform`, `version`; `exit`, `recursion_limit`, `gc_collect`, `memory`, `gc_stats`, `gc_enable`, `gc_disable`, `gc_set_min`, `gc_set_max` |
| `math` | `pi`, `e`, `tau`, `inf`, `nan`; `sqrt`, `abs`, `round`, `floor`, `ceil`, `trunc`, `pow`, `exp`, `log`, `log2`, `log10`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `degrees`, `radians`, `hypot`, `gcd`, `lcm`, `factorial`, `is_nan`, `is_inf`, `is_finite` |
| `io` | `stdin`, `stdout`, `stderr`; `read_file`, `read_lines`, `write_file`, `append_file`, `open`, `eprint` |
| `random` | `rand_int`, `rand_float`, `choice`, `shuffle`, `sample`, `seed` |
| `time` | `now`, `clock`, `sleep` |
| `os` | `exists`, `is_file`, `is_dir`, `list_dir`, `remove`, `rename`, `make_dir`, `remove_dir`, `cwd`, `env`, `join`, `base_name`, `dir_name` |
| `signal` | `SIGINT`, `SIGTERM`, `SIGHUP`, `SIGQUIT`, `SIGUSR1`, `SIGUSR2`, `SIGALRM`, `SIGPIPE`, `SIGCHLD`; `handle`, `ignore`, `reset`, `send` |
| `json` | `parse`, `dump` |

### sys

| Member | |
|---|---|
| `argv` | The program's arguments as a tuple of strings, `argv[0]` its path: the same as `prog main(argc, argv)` gets (`tuple()` in the REPL) |
| `platform` | The system, in lower case: `"linux"` |
| `version` | The version of pycee: `"0.10.0"` (the phase of the roadmap as the minor version, until 1.0) |
| `exit()`, `exit(code)` | Ends the program with that status (an integer from 0 to 255; `0` without one). It cannot be caught: it goes through every `except`, running the `finally` blocks on the way. In the REPL it ends the session (there `from sys import exit` is an error: `exit()` already exists) |
| `recursion_limit()`, `recursion_limit(n)` | How many calls can be in progress at once (1000 at the start), or set it (`n` an integer ≥ 1; `ValueError` otherwise). Passing it is a `RecursionError`; the real stack guard still protects against a limit too high |
| `gc_collect()` | Runs a collection now (even when disabled) and gives how many objects it freed |
| `memory()` | Bytes the live objects take |
| `gc_stats()` | `dict("collections": n, "objects": n, "bytes": n, "next_gc": n)`: collections run so far, live objects, their bytes, and where the next automatic collection is |
| `gc_disable()`, `gc_enable()` | Stop / resume automatic collections (`gc_set_max` included): with the collector off, memory is the program's business; running out of it ends the program cleanly |
| `gc_set_min(bytes)` | Below this many bytes in use there is no automatic collection (1 MiB at the start) |
| `gc_set_max(bytes)` | On reaching this many bytes there is always a collection, whatever `gc_set_min` says (`0`, the start: no maximum). A maximum below what the program keeps alive collects on every allocation: correct, but slow |

Sizes are integers ≥ 0 (`ValueError` otherwise, `TypeError` if not an integer). Automatic collections otherwise run when the memory in use passes twice what survived the last one.

### math

Every function takes numbers (int or float; `TypeError` otherwise, a bool included). Constants: `pi`, `e`, `tau`, `inf`, `nan` (floats).

| Member | |
|---|---|
| `sqrt(x)`, `abs(x)`, `round(x)`, `round(x, digits)`, `floor(x)`, `ceil(x)`, `trunc(x)`, `pow(x, y)` | The methods of number of the same name: `math.sqrt(x)` is `x.sqrt()` (see number) |
| `exp`, `log2`, `log10`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `degrees`, `radians` | One number, a float |
| `log(x)`, `log(x, base)` | Natural logarithm, or in that base. `x > 0`, `base > 0` and not `1` (`ValueError` otherwise) |
| `atan2(y, x)`, `hypot(x, y)` | Two numbers, a float |
| `gcd(a, b, ...)`, `lcm(a, b, ...)` | Two or more ints (`TypeError` with a float); the result is `>= 0`; `gcd(0, 0)` is `0`, `lcm` with a `0` is `0` |
| `factorial(n)` | An int `>= 0` (`ValueError` if negative); `21!` and up do not fit: `OverflowError` |
| `is_nan(x)`, `is_inf(x)`, `is_finite(x)` | Bools; an int is never nan or inf |

As in Python: a result out of the domain (`asin(2)`, `sin(inf)`, a nan from numbers that are not nan) is a `ValueError` ("math domain error"); one too large (`exp(1000)`: inf from finite numbers) an `OverflowError`; an int that does not fit in 64 bits, an `OverflowError`. A nan given stays nan (`sin(nan)` is `nan`).

### io

Paths are strings (no `\0` inside), relative to the current directory. A failure of the system is an `IOError` with the path in the message: `FileNotFoundError` when it (or a directory on the way) does not exist, `PermissionError` when it is not allowed, `IOError` otherwise (`is a directory: "tests"`). All three are global, like the other errors.

| Member | |
|---|---|
| `read_file(path)` | The whole file as a string |
| `read_lines(path)` | A list of its lines, **each with its `"\n"`** |
| `write_file(path, text)` | Creates or empties the file and writes `text` (a string); `none` |
| `append_file(path, text)` | Adds `text` at the end (creating the file if needed); `none` |
| `open(path)`, `open(path, mode)` | A `file`: mode `"r"` (read, the default), `"w"` (create or empty, then write) or `"a"` (add at the end); any other mode is a `ValueError` |
| `eprint(..., sep, end)` | `print` to the error stream |
| `stdin`, `stdout`, `stderr` | The standard streams, as files |

`print(..., file = f)` writes to `f`. Text sent to the error stream (`eprint`, `print(file = io.stderr)`, `io.stderr.write`) comes out after everything printed before it, and `input()` shows what was printed before waiting.

### random

A generator of its own (xoshiro256**, seeded through splitmix64), not the C library's: after `seed(n)` the sequence is the same on any machine; without a seed it starts from the system's randomness, different on every run. Not for cryptography.

| Member | |
|---|---|
| `seed(n)` | From now on, the sequence that the integer `n` gives |
| `rand_int(a, b)` | An int from `a` to `b`, **both included** (`a <= b`, `ValueError` otherwise; ints only) |
| `rand_float()` | A float in `[0, 1)` |
| `choice(seq)` | One item at random of a list, tuple, string (a character) or range; `IndexError` if it is empty. A set has no positions: `TypeError` |
| `shuffle(l)` | Puts the items of the list in a random order, in place, and gives it back (a tuple cannot change: `TypeError`) |
| `sample(seq, k)` | A new list of `k` items of `seq` (as in `choice`) taken at random, none twice, in a random order; `k` from 0 to the number of items (`ValueError` otherwise) |

### time

| Member | |
|---|---|
| `now()` | Seconds since 1970-01-01 00:00 UTC, as a float (the wall clock: it can jump) |
| `clock()` | Seconds from some fixed moment, as a float; it never goes back, so `clock() - t0` is a duration |
| `sleep(s)` | Waits `s` seconds (a number `>= 0`, fractions too; `ValueError` if negative) |

### os

Paths are strings relative to the current directory. What the system refuses is an `IOError` with the path, as in `io`: `FileNotFoundError` when it does not exist, `PermissionError` when it is not allowed, `IOError` otherwise (`file exists`, `directory not empty`, `not a directory`...). A link is followed.

| Member | |
|---|---|
| `exists(p)`, `is_file(p)`, `is_dir(p)` | Bools (never an error for a path that is not there) |
| `list_dir()`, `list_dir(p)` | The names in the directory (`"."` by default; `p` can be given by name), sorted, without `"."` and `".."` |
| `remove(p)` | Deletes a file (a directory is an `IOError`: `remove_dir`) |
| `rename(old, new)` | Renames or moves; `new` is replaced if it exists |
| `make_dir(p)` | Makes one directory: its parent must exist, and it must not (`IOError: file exists`) |
| `make_dir(p, "r")` | Also every directory missing on the way; one that already exists is fine. Any other second argument is a `ValueError` |
| `remove_dir(p)` | Deletes an empty directory (`IOError: directory not empty` otherwise) |
| `cwd()` | The current directory, as an absolute path |
| `env(name)` | The environment variable, or `none` if it is not set |
| `join(a, b, ...)` | The parts with one `"/"` between them; a part starting with `"/"` starts again from there (`join("a", "/x")` is `"/x"`) |
| `base_name(p)` | What follows the last `"/"`: `"a/b.txt"` → `"b.txt"`, `"a/"` → `""` |
| `dir_name(p)` | What comes before the last `"/"`: `"a/b.txt"` → `"a"`, `"b.txt"` → `""`, `"/a"` → `"/"` |

`join`, `base_name` and `dir_name` only work on the text (as Python's `os.path`): the paths need not exist.

### signal

A handler is a function that takes the signal's number. It never runs inside the signal itself: the signal is noted and the handler runs at the next safe point (before each statement, each call, each round of a loop); a wait that the signal interrupts (`input()`, `time.sleep()`, reading or writing a file) attends it at once and then goes on waiting. An error raised by a handler comes out where the program was.

| Member | |
|---|---|
| `SIGINT`, `SIGTERM`, `SIGHUP`, `SIGQUIT`, `SIGUSR1`, `SIGUSR2`, `SIGALRM`, `SIGPIPE`, `SIGCHLD` | Their numbers (Linux's: `SIGINT` is `2`) |
| `handle(sig, f)` | From now on `f(sig)` runs when `sig` comes. With a handler for `SIGINT`, Ctrl+C calls it instead of being a `KeyboardInterrupt` |
| `ignore(sig)` | `sig` does nothing from now on |
| `reset(sig)` | Back to the start: `SIGINT` is a `KeyboardInterrupt`, any other does what the system does (most end the program) |
| `send(sig)` | Sends `sig` to the program itself; it is attended before `send` returns |

Only those signals: any other number is a `ValueError` (`SIGKILL` and `SIGSTOP` cannot be handled by anyone; `SIGSEGV`, `SIGFPE`, `SIGBUS` and `SIGILL` are not allowed, since going on after one would not be memory safe).

Without the module, Ctrl+C is a `KeyboardInterrupt`: in a file it stops the program (code 130) unless an `except KeyboardInterrupt` catches it; at the REPL's prompt it drops what was being typed, and while something runs it stops it and goes back to the prompt.

### json

| Member | |
|---|---|
| `parse(text)` | The value a JSON text holds: object → `dict`, array → `list`, string → string, `true`/`false` → bools, `null` → `none`; a number without `.` or exponent → int, otherwise float. A repeated key keeps its last value. `\uXXXX` escapes (surrogate pairs too; half a pair alone becomes U+FFFD) |
| `dump(value)`, `dump(value, indent = n)` | The JSON text of `value`: in one line as Python writes it (`{"a": 1, "b": [1, 2]}`), or each item on its own line, `n` spaces deeper per level (`indent` from 0 to 64, only by name: `dump(v, 2)` is a `TypeError`). Tuples become arrays; text stays UTF-8 (`"año"`, not `"a\u00f1o"`); control characters are escaped |

Errors: a text that is not JSON is a `ValueError` with where it went wrong (`json.parse(): expected ',' or ']' but found '2' at line 1, column 4`); an int that does not fit in 64 bits or a float out of range (`1e400`), an `OverflowError` (JSON has no inf). `dump` of a set, an object, a function, a range or a file, or a dict with keys that are not strings, is a `TypeError`; `nan` and `inf`, and a value that contains itself, a `ValueError`. Nesting deeper than the recursion limit (`sys.recursion_limit()`, 1000 at the start), when parsing or dumping, is a `RecursionError`, as in Python; the real stack guard still protects a limit set very high.
