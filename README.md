# pycee

pycee is an interpreted programming language written from scratch in C. It reads like Python, is written with the structure of C (braces, `;`, parentheses around conditions) and reports errors like Python, with their type, the line and a traceback. Source files use the `.pce` extension.

## Why it exists

pycee was born from bringing together the comfortable parts of the languages I know with my pressing need for perfect memory management, without the programmer having to worry about it. And also because I found it really interesting and curious to create a programming language through another one.

Its ideas come from there:

1. **Bring together the comfortable parts of each language.** From Python: every value has methods (`s.split(",")`, `l.sort()`), comprehensions, f-strings, clear errors and a standard library you import. From C: the structure with braces and `;`, and `++`/`--`. When there are two ways of doing something, the one that makes the programmer's life easier wins.
2. **Memory that is 100% safe, without the programmer thinking about it.** There is no `malloc` or `free` in sight: values are managed by a garbage collector and every access is checked, so no pycee program can corrupt memory. The interpreter itself has no leaks or invalid accesses on any path either, not even when an allocation fails. Every change is checked with AddressSanitizer, valgrind and a tester that makes each `malloc` fail, one at a time.

The language also tries to catch as much as possible before running: undeclared names, repeated variables or the wrong number of arguments.

## A quick look

```
obj Point {
    func create(x, y) {
        self.x = x;
        self.y = y;
    }

    func move(dx, dy) {
        self.x += dx;
        self.y += dy;
        return self;
    }

    func to_str() {
        return f"({self.x}, {self.y})";
    }
}

func even_squares(l) {
    return [x * x for (x in l) if (x % 2 is 0)];
}

prog main() {
    var nums = [5, 2, 8, 1];
    print(nums.copy().sort(), nums);            # [1, 2, 5, 8] [5, 2, 8, 1]
    print(even_squares(nums), sum(nums));       # [4, 64] 16
    print(any(n > 7 for (n in nums)));          # true

    var p = Point(1, 2);
    print(p.move(3, 0), f"{3.14159:.2f}");      # (4, 2) 3.14

    var double = (x) => x * 2;
    print(nums.map(double));                    # [10, 4, 16, 2]

    try {
        print(10 / 0);
    } except ZeroDivisionError (e) {
        print("Caught:", e.message());          # Caught: division by zero
    }
    return 0;
}
```

An uncaught error shows where it happened and how it got there:

```
func mean(l) {
    return sum(l) / l.len();
}

prog main() {
    print(mean([]));
}
```
```
Traceback (most recent call last):
  File "mean.pce", line 6, in main
        print(mean([]));
              ^^^^^^^^
  File "mean.pce", line 2, in mean
        return sum(l) / l.len();
               ^^^^^^^^^^^^^^^^
ZeroDivisionError: division by zero
```

Without a file it opens the REPL:

```
$ ./pycee
>>> 2 + 3 * 4
14
>>> "naïve".len()
5
```

## What it has so far

- **Types:** exact 64-bit integers (an overflow is an `OverflowError`) and floats, Unicode strings (`"naïve".len()` is 5), lists, tuples, sets, dicts and `range`.
- **Variables:** `var`, `vemp` (lives only in its block) and `const`.
- **Control flow:** `if`/`elif`/`else`, `while`, `do`/`while`, `for` with `else`, `switch` and `break outer`.
- **Functions:** they are values, with lambdas, closures, default values, named arguments and `...rest`.
- **Objects:** `obj` with inheritance, `super`, `private`, `static` and special methods (`to_str`, `compare`, `iter`...).
- **Errors:** `try`/`except`/`finally`, `raise` and `assert`, with your own errors extending the language's.
- **Modules:** `import m`, `import m as x` and `from m import a as b`.
- **Garbage collector:** mark & sweep, its own.

On the way, the standard library: `sys`, `math`, `io` (files), `random`, `time`, `os` and `json`. Then performance and quality, and at the end a built-in manual (`man`) and a tutorial with exercises.

## Build and test

You need `gcc` and `make` on Linux.

```
make              # ./pycee: development build, with sanitizers
make release      # ./pycee-release: optimised (-O2)
make test         # the whole test suite
make stress       # the tests with the collector running on every allocation
sh tests/run_torturette.sh   # fault injection (the tool is in tests/torturette)

./pycee program.pce [arguments]
./pycee           # REPL
```

## Documentation

- [docs/TYPES.md](docs/TYPES.md): every type, with its operators and methods.
- [docs/EBNF.md](docs/EBNF.md): the grammar and the rules checked before running.
- [docs/ROADMAP.md](docs/ROADMAP.md): the roadmap by phases (in Spanish).
- [docs/SESSION.md](docs/SESSION.md): the state of the project and the decisions taken (in Spanish).
- [examples/](examples/): example programs.
