# Exercise 02: fizzbuzz

| | |
|---|---|
| File to hand in | `fizzbuzz.pce` |
| Uses | `var` and `vemp`, `for` over a `range`, `if`/`elif`/`else`, `%`, `++`, `int()`, `try`/`except ValueError`, functions |
| In the manual | `man(var)`, `man(vemp)`, `man(range)`, `man(if)`, `man(int)`, `man(try)`, `man(func)` |

The classic, with optional arguments. It is a good way to bump into the two rules that surprise most when arriving at pycee: `var` cannot be declared inside a loop, and conditions only take `true` or `false`.

## Statement

```
./pycee fizzbuzz.pce N [a b]
```

Write the numbers from 1 to `N`, one per line, changing:

- the multiples of both `a` and `b` into `FizzBuzz`;
- the multiples of `a` only into `Fizz`;
- the multiples of `b` only into `Buzz`.

When they are not given, `a` is 3 and `b` is 5. At the end write `FizzBuzz: K`, where `K` is how many times `FizzBuzz` came out.

### Rules

1. **Number of arguments.** 1 (`N`) or 3 (`N a b`); with any other number, `usage: fizzbuzz.pce N [a b]` and exit code **2**.

   > Careful: 2 arguments (`N a` without `b`) must be rejected too. The examples below do not show it: think it through yourself.

2. **Every argument** must be an integer greater than 0. When one is not, write `error: 'X' is not a positive integer` (`X` as it was written) and the exit code is **1**. They are checked in order, and only the first one that fails is reported.
3. When everything goes well, the exit code is **0**.

## Examples

```
$ ./pycee fizzbuzz.pce 15
1
2
Fizz
4
Buzz
Fizz
7
8
Fizz
Buzz
11
Fizz
13
14
FizzBuzz
FizzBuzz: 1
```

```
$ ./pycee fizzbuzz.pce 10 2 3
1
Fizz
Buzz
Fizz
5
FizzBuzz
7
Fizz
Buzz
Fizz
FizzBuzz: 1
```

```
$ ./pycee fizzbuzz.pce 10 x 5; echo "exit=$?"
error: 'x' is not a positive integer
exit=1
$ ./pycee fizzbuzz.pce 0; echo "exit=$?"
error: '0' is not a positive integer
exit=1
```

## Hints (read them only if you get stuck)

<details>
<summary>var, vemp and loops</summary>

- `var` lives from its line to the end of the function, even when it is declared inside a block. That is why **it is not allowed inside a loop**: it would be declared again on every round.
- Inside the loop use `vemp`, which lives only in its block and is new on every round: `vemp fizz = i % a is 0;`.
- A counter that outlives the loop is declared before it with `var` and increased with `count++;` (or `count += 1;`).

</details>

<details>
<summary>Conditions</summary>

- `if (i % 3)` is an error: the condition must be a bool. Write `if (i % 3 is 0)`.
- `i % a is 0` already is a bool, so you can keep it in a variable and use it later in `if (fizz and buzz)`.

</details>

<details>
<summary>Converting the arguments</summary>

- `int("12")` is `12`, and `int("x")` raises a `ValueError`.
- A small function that gives back the number, or `none` when it is not one, keeps `main` clean:

  ```
  func to_int(s) {
      try {
          return int(s);
      } except ValueError {
          return none;
      }
  }
  ```

- To keep several values with defaults, start with a list `var nums = [10, 3, 5];` and replace the ones that arrive. Then unpack it: `var limit, a, b = nums[0], nums[1], nums[2];`.

</details>

## Going further (optional)

- Do it with `switch` instead of `if`/`elif` (`man(switch)`): on what value would the `switch` be?
- Accept any number of rules: `fizzbuzz.pce 30 3:Fizz 5:Buzz 7:Bazz`.
