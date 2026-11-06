# Exercise 04: Caesar cipher

| | |
|---|---|
| File to hand in | `caesar.pce` |
| Uses | strings (`find`, indexes, `for` over a string, `join`), `const`, `input()` until `EOFError`, `%` with negatives |
| In the manual | `man(string)`, `man(string.find)`, `man(const)`, `man(input)`, `man(EOFError)`, `man(operators)` |

A small exercise with two traps of the language: there is no `ord` or `chr` to go from a letter to a number, and `%` works as in C, not as in Python.

## Statement

```
./pycee caesar.pce K < text.txt
```

Read the text from the standard input, line by line until it ends, and write each line encrypted: every letter moves `K` places in the alphabet, wrapping around at the end (`z` + 1 is `a`).

### Rules

1. **Only the 26 English letters change**, `a`–`z` and `A`–`Z`, and each one keeps its case. Everything else stays as it is: spaces, digits, signs and letters such as `ï` or `é`.
2. **`K` can be any integer**, negative or above 26 too: `-3` undoes a `3`, and `29` is the same as `3`.
3. **Arguments.** With a number of arguments other than 1, `usage: caesar.pce K` (code **2**). When `K` is not an integer, `error: 'X' is not an integer` (code **1**).

## Examples

With this `text.txt`:

```
Hello, World!
fox jumps XYZ
Naïve café: 2026?
```

```
$ ./pycee caesar.pce 3 < text.txt
Khoor, Zruog!
ira mxpsv ABC
Qdïyh fdié: 2026?
$ ./pycee caesar.pce -29 < text.txt
Ebiil, Tloia!
clu grjmp UVW
Kxïsb zxcé: 2026?
```

Encrypting and then decrypting gives the original text back:

```
$ ./pycee caesar.pce 3 < text.txt | ./pycee caesar.pce -3
Hello, World!
fox jumps XYZ
Naïve café: 2026?
```

## Hints (read them only if you get stuck)

<details>
<summary>Letters without ord or chr</summary>

- pycee has no `ord` or `chr`. The position of a letter comes from an alphabet: `const LOWER = "abcdefghijklmnopqrstuvwxyz";` and `LOWER.find(c)` gives its position, or `-1` when it is not there.
- The new letter is `LOWER[(i + k) % 26]`.

</details>

<details>
<summary>pycee's %</summary>

- In pycee, `%` follows C: the result has the sign of the first operand. `-7 % 3` is `-1`, not `2` as in Python.
- With a negative `K`, `(i + k) % 26` can be negative. Here it happens to work, because a negative index counts from the end (`LOWER[-3]` is `"x"`, just the right letter). But that is luck: if you used that number for anything else (comparing it, keeping it, computing with it), it would be wrong. Normalise `K` once at the start, so it stays between 0 and 25: `k = (k % 26 + 26) % 26;`.

</details>

<details>
<summary>Building the string</summary>

- Strings are immutable: `s[i] = "x"` is not possible.
- Walk the line with `for (c in line)` (each `c` is a string of one character, also for `ï`) and join the result: `"".join([shift(c, k) for (c in line)])`.

</details>

<details>
<summary>Reading to the end</summary>

```
while (true) {
    try {
        print(encode(input(), k));
    } except EOFError {
        break;
    }
}
```

</details>

## Going further (optional)

- Break the cipher without knowing `K`: try the 26 shifts and keep the one that gives the most `e` letters (or the most common letter of your language).
- Encrypt the digits too, with their own alphabet of 10.
