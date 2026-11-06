# Exercise 09: bank

| | |
|---|---|
| File to hand in | `bank.pce` |
| Uses | `obj`, `create`, `extends`, `private`, `static`, `to_str`, `compare`, your own errors (`obj E extends ValueError`), `switch` on strings, several `except` |
| In the manual | `man(obj)`, `man(private)`, `man(static)`, `man(super)`, `man(errors)`, `man(raise)`, `man(switch)`, `man(is_type)` |

From here on there are fewer hints: the statement says what must happen, and how is up to you.

## Statement

A bank that reads commands from the standard input, one per line, and runs them.

| Command | What it does | Writes |
|---|---|---|
| `open NAME` | Opens a regular account with balance 0 | `opened: <account>` |
| `open NAME savings` | Opens a savings account with balance 0 | `opened: <account>` |
| `deposit NAME AMOUNT` | Adds the amount to the balance | `<account>` |
| `withdraw NAME AMOUNT` | Takes the amount out, when the balance is enough | `<account>` |
| `transfer FROM TO AMOUNT` | Withdraws from one and deposits into the other | `<from> -> <to>` |
| `interest PERCENT` | Adds that percentage of the balance to every **savings** account, rounded to 2 decimals, in the order they were opened | `<account>` for each one |
| `list` | Every account, from the highest to the lowest balance (with the same balance, in the order they were opened) | `  <account>` (two spaces in front) |

`<account>` is written `name (regular): 120.50` or `name (savings): 220.00`: the balance always with two decimals.

Empty lines are skipped. When the input ends, write `accounts: N, total: T, errors: E` (accounts opened, sum of balances with two decimals, commands that failed). The exit code is **1** when there was any error and **0** otherwise.

### Errors

A command that fails changes nothing (not even halfway: a transfer without enough money withdraws from neither), writes `line N: message` and the next one runs.

| Situation | Message |
|---|---|
| not enough money | `insufficient funds: ana has 70.00, cannot withdraw 500.00` |
| the account does not exist | `no such account: 'eva'` |
| opening one that exists | `account 'ana' already exists` |
| an amount that is not a number greater than 0 | `invalid amount: '-5'` |
| unknown command | `unknown command: 'fly'` |
| wrong number of words | `usage: ...` with the form of the command, e.g. `usage: withdraw NAME AMOUNT` |

### Design requirements

- An `obj Account` and an `obj SavingsAccount extends Account`. The balance is a **`private`** field: from outside it is only read with a method `balance()`.
- `Account.opened` is a **`static`** field that counts the accounts opened.
- Every account is shown with its `to_str()`, and `list` sorts with `sort(reverse = true)` using the accounts' `compare()` (no `key`).
- The errors are yours: `obj BankError extends ValueError`, and `InsufficientFunds` and `NoSuchAccount` extending `BankError`. Notice that the message for not enough money starts with `insufficient funds: `: it comes from an `except` of its own for that type.

## Example

Input:

```
open ana
open luis savings
open ana
deposit ana 100
deposit luis 250.50
withdraw ana 30
withdraw ana 500
transfer luis ana 50.5
interest 10
deposit eva 10
deposit ana -5
fly ana

list
```

Output (exit code 1):

```
opened: ana (regular): 0.00
opened: luis (savings): 0.00
line 3: account 'ana' already exists
ana (regular): 100.00
luis (savings): 250.50
ana (regular): 70.00
line 7: insufficient funds: ana has 70.00, cannot withdraw 500.00
luis (savings): 200.00 -> ana (regular): 120.50
luis (savings): 220.00
line 10: no such account: 'eva'
line 11: invalid amount: '-5'
line 12: unknown command: 'fly'
  luis (savings): 220.00
  ana (regular): 120.50
accounts: 2, total: 340.50, errors: 5
```

## Hints (few)

<details>
<summary>Rules of obj you will bump into</summary>

- Fields are made in `create` with `self.x = ...`; the other methods read and change them, but cannot make new ones. A `private` or `static` field is declared with `private var` / `static var` in the body of the `obj`.
- A `private` is seen only by `self`, and only inside the `obj` that declares it: `SavingsAccount` does not see it either. Let it use the methods of `Account`.
- An error of your own without `create` takes a message: `raise NoSuchAccount("...")`. Catch it by its type: `except InsufficientFunds (e) { ... } except BankError (e) { ... }`; the first that fits wins, so the most specific goes first.
- `a.is_type("SavingsAccount")` says whether an account is a savings one.

</details>

## Going further (optional)

- A savings account only allows 3 withdrawals; the fourth is a new error, `WithdrawalLimit`.
- Save the state of the bank in a file at the end and load it at the start (`man(io)`, `man(json)`).
