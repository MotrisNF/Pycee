# Exercise 01: greet

| | |
|---|---|
| File to hand in | `greet.pce` |
| Uses | `prog main(argc, argv)`, `for`, f-strings, `return` as the exit code, the ternary `c ? a : b` |
| In the manual | `man(prog)`, `man(sys.argv)`, `man(for)`, `man(fstrings)`, `man(string.capitalize)`, `man(operators)` |

A first contact: how a program starts, how it gets its arguments and how it tells the system whether it went well.

## Statement

Write a program that greets every name it gets as an argument and, at the end, says how many greetings it gave.

```
./pycee greet.pce ana LUIS
```

### Rules

1. **Each argument is a name.** For each one, write `hello, Name!`, with the first letter in upper case and the rest in lower case (`LUIS` → `Luis`).
2. **At the end**, `N greetings`. With only one, `1 greeting`, singular.
3. **Without arguments**, write `usage: greet.pce name...` and end with exit code **2**.
4. When everything goes well, the exit code is **0**.

## Examples

```
$ ./pycee greet.pce ana LUIS; echo "exit=$?"
hello, Ana!
hello, Luis!
2 greetings
exit=0
```

```
$ ./pycee greet.pce eva; echo "exit=$?"
hello, Eva!
1 greeting
exit=0
```

```
$ ./pycee greet.pce; echo "exit=$?"
usage: greet.pce name...
exit=2
```

## How to check it

Run the three examples. The output must match character by character, and so must the `exit=`.

## Hints (read them only if you get stuck)

<details>
<summary>The entry point</summary>

- A program starts at `prog main(argc, argv)`. `argc` is the number of arguments and `argv` a **tuple** of strings.
- As in C, `argv[0]` is the program itself (`greet.pce`): the names start at `argv[1]`. That is why, without arguments, `argc` is 1.
- What `prog` gives back with `return` is the exit code: `return 2;`.

</details>

<details>
<summary>Walking the names</summary>

- A slice gives the rest of the tuple: `argv[1:]`.
- `for (name in argv[1:]) { ... }` walks every name. The variable of the `for` only exists inside the loop.
- `"LUIS".capitalize()` is `"Luis"`.

</details>

<details>
<summary>Singular and plural</summary>

- The ternary chooses between two values: `n is 1 ? "greeting" : "greetings"`.
- Any expression can go inside an f-string, a ternary too: `f"{n} greeting{n is 1 ? "" : "s"}"`.
- There is no `==`: compare with `is`.

</details>

## Going further (optional)

- If a name is repeated, greet it only once (hint: `man(set)`).
- Greet in alphabetical order (`man(list.sort)`).
