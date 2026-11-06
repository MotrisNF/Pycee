# Exercise 07: words

| | |
|---|---|
| File to hand in | `words.pce` |
| Uses | `dict` (`get`, `items`, `keys`, `values`, `in`), `for (k, v in d)`, stable `sort` with `key`, strings (`is_alpha`, `lower`, `to_list`) |
| In the manual | `man(dict)`, `man(dict.get)`, `man(dict.keys)`, `man(list.sort)`, `man(string.lower)`, `man(string.to_list)`, `man(for)` |

Counting things is what a dictionary does best. And sorting by two criteria at once needs another detail of pycee: tuples cannot be compared.

## Statement

```
./pycee words.pce [N] < text.txt
```

Read a text from the standard input and write statistics about its words. A **word** is a run of letters (`is_alpha()`), turned to lower case; anything else (spaces, signs, digits) separates words.

Write, in this order:

1. `lines: L, words: W, distinct: D`.
2. `most frequent:` and, below it, the `N` most repeated words (5 when `N` is not given), one per line: the count right-aligned in 4 characters, a space and the word. From the most to the least repeated; **with the same count, in alphabetical order**.
3. `only once: K`: how many distinct words appear exactly once.
4. `anagrams:` and the groups of two or more distinct words with the same letters, each group in brackets with its words in alphabetical order, and the groups sorted by their first word. With none, `anagrams: none`.

When there are no words at all, only the first line is written.

### Rules

- With more than one argument, `usage: words.pce [N]` (code **2**); when `N` is not an integer, `error: 'X' is not an integer` (code **1**).
- Letters with accents are letters (`is_alpha()` says yes), so `café` is one word.

## Examples

With this `text.txt` (line 2 is empty):

```
Listen to the silent night, and to the stone notes of the bells.

The cat and the dog; the dog and the bird.
Enlist now: tones, onset, act!
```

```
$ ./pycee words.pce < text.txt
lines: 4, words: 28, distinct: 18
most frequent:
   7 the
   3 and
   2 dog
   2 to
   1 act
only once: 14
anagrams: [act, cat] [enlist, listen, silent] [notes, onset, stone, tones]
```

The last of the top 5 is `act`: 14 words appear once, and `act` is the first of them alphabetically.

```
$ ./pycee words.pce 2 < text.txt
lines: 4, words: 28, distinct: 18
most frequent:
   7 the
   3 and
only once: 14
anagrams: [act, cat] [enlist, listen, silent] [notes, onset, stone, tones]
$ ./pycee words.pce < /dev/null
lines: 0, words: 0, distinct: 0
```

## Hints (read them only if you get stuck)

<details>
<summary>Splitting the words</summary>

- `split()` is not enough, because it keeps the signs attached (`night,`). Walk the line character by character: while it is a letter, add it to the current word (`cur += c.lower();`); when it is not, the word ends.
- A trick not to forget the last word: walk `line + " "`.

</details>

<details>
<summary>Counting</summary>

- `count[w] = count.get(w, 0) + 1;`: `get` gives a default when the key is not there.
- `for (w, n in count)` walks key and value at once. With a single variable, `for (x in count)` gives each pair as a dict of one item: careful.

</details>

<details>
<summary>Sorting by two things</summary>

- In pycee **tuples cannot be compared** (`tuple(1, "a") < tuple(2, "b")` is a `TypeError`), so you cannot sort by `tuple(-count, word)` as in Python.
- But `sort` is **stable**: sort first by the secondary criterion and then by the main one, and the ties of the second keep the order of the first.
- `count.items()` gives a list of `tuple(word, count)`, and since the methods that change a list give it back, they chain: `count.items().sort(key = (p) => p[0]).sort(key = (p) => p[1], reverse = true)`.

</details>

<details>
<summary>Anagrams</summary>

- Two words are anagrams when they have the same letters, sorted: `"".join(w.to_list().sort())`.
- Group them in a dict whose key is that and whose value is the list of words.

</details>

## Going further (optional)

- Write the result as JSON too (`man(json.dump)`).
- Ignore a list of stop words ("the", "a", "and"...) kept in a `set`.
