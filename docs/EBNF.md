```ebnf
(* ---------- Program (top level) ---------- *)

(* Imports go first, before any other declaration. *)
program = { import_decl } { top_decl } EOF ;
top_decl = func_decl | prog_decl | obj_decl | var_decl | const_decl ;

(* import utils;  import utils as u;  from utils import parse, dump as d;
   No "from x import *". Parsed into NODE_IMPORT; loading modules is the resolver's job. *)
import_decl = "import" IDENT [ "as" IDENT ] ";"
            | "from" IDENT "import" import_name { "," import_name } ";" ;
import_name = IDENT [ "as" IDENT ] ;

func_decl = "func" IDENT "(" [ params ] ")" block ;
prog_decl = "prog" IDENT "(" [ params ] ")" block ;
(* Fields are declared by self.x = ... in create, or with var (needed for
   private, static or an initial value). Single inheritance; both forms
   mean the same; P is an obj or an error type. *)
obj_decl = "obj" IDENT [ "extends" IDENT | "(" IDENT ")" ] "{" { member } "}" ;
member = [ "private" ] [ "static" ] ( var_decl | func_decl ) ;
(* A default may use the parameters before it; once one has a default,
   all the later ones need one; ...rest (a tuple) goes last. *)
params = param { "," param } ;
param = [ "..." ] IDENT [ "=" expression ] ;

(* ---------- Statements ---------- *)

statement = var_decl
          | func_decl          (* inside a function: a constant of its block *)
          | try_stmt
          | raise_stmt
          | assert_stmt
          | vemp_decl
          | const_decl
          | do_stmt
          | if_stmt
          | while_stmt
          | for_stmt
          | switch_stmt
          | return_stmt
          | break_stmt
          | continue_stmt
          | simple_stmt ;

block = "{" { statement } "}" ;
(* Size or initial value, never both: var l[3] = [...]; is not valid. *)
var_decl = "var" IDENT [ "[" expression "]" | "=" expression ] ";"
         | ( "var" | "vemp" | "const" ) IDENT "," IDENT { "," IDENT }
           "=" expression { "," expression } ";" ;
           (* one value per name, or one tuple unpacked when it runs *)
vemp_decl = "vemp" IDENT [ "[" expression "]" | "=" expression ] ";" ;

if_stmt = "if" "(" expression ")" block { "elif" "(" expression ")" block } [ "else" block ] ;
while_stmt = "while" "(" expression ")" block ;
(* The for's else runs only if the loop ends without a break. *)
for_stmt = "for" "(" IDENT "in" expression ")" block [ "else" block ] ;
(* outer: the loop around the current one. Not a keyword: only here. *)
continue_stmt = "continue" [ "outer" ] ";" ;
break_stmt = "break" [ "outer" ] ";" ;
do_stmt = "do" block "while" "(" expression ")" ";" ;
(* Errors. An except without types catches any error (Error is the base
   of all; SystemExit is not one, so exit() is never caught). The name is
   a variable of its block. return, break and continue cannot leave a
   finally block. *)
try_stmt = "try" block { except_clause } [ "finally" block ] ;   (* at least one *)
except_clause = "except" [ IDENT { "," IDENT } ] [ "(" IDENT ")" ] block ;
raise_stmt = "raise" expression ";" ;                  (* an error value *)
assert_stmt = "assert" "(" expression [ "," expression ] ")" ";" ;
(* Constants: value required, never reassigned. Inside a function a const
   lives in its block (like vemp); at the top level it is a global. *)
const_decl = "const" IDENT "=" expression ";" ;
return_stmt = "return" [ expression ] ";" ;

switch_stmt = "switch" "(" expression ")" "{" { case_clause } [ default_clause ] "}" ;
case_clause = "case" case_value { "," case_value } block ;
case_value = [ "-" ] NUMBER | STRING ;
default_clause = "default" block ;

(* Assignment or bare expression (print(x); l.append(1); i++;).
   Both start with an expression, so they share a rule: the parser reads
   the expression and, if an assign_op follows, it is an assignment.
   Whether the left side is assignable (IDENT, x[i], x.field) is checked
   by the resolver, not by the grammar. *)
simple_stmt = expression [ assign_op expression ] ";"
            | expression { "," expression } "=" expression { "," expression } ";" ;
            (* a, b = b, a: all values first; as many values as targets *)
assign_op = "=" | "+=" | "-=" | "*=" | "/=" | "//=" | "%=" ;

(* ---------- Expressions (lowest to highest precedence) ---------- *)

expression = or_expr ;
or_expr = and_expr { ( "or" | "xor" ) and_expr } ;
and_expr = not_expr { "and" not_expr } ;
not_expr = "not" not_expr | comparison ;
comparison = sum [ ( "is" [ "not" ] | [ "not" ] "in" | "<" | "<=" | ">" | ">=" ) sum ] ;
sum = product { ( "+" | "-" ) product } ;
product = unary { ( "*" | "/" | "//" | "%" ) unary } ;
(* ++ and -- work exactly as in C: prefix (++i) yields the new value,
   postfix (i++) yields the old one; both are expressions. Postfix binds
   tighter than prefix, so ++i++ is ++(i++), which is invalid because
   i++ is not assignable (checked by the resolver). Operands are evaluated
   left to right, so i = i++; is defined (unlike C): it leaves i unchanged. *)
unary = ( "-" | "++" | "--" ) unary | postfix ;

postfix = primary { call | index | field | "++" | "--" } ;
(* Positional arguments (and ...t, a tuple spread into them) go before
   the named ones. Anything that gives a function can be called. *)
call = "(" [ call_arg { "," call_arg } ] ")" ;
call_arg = expression | "..." expression | IDENT "=" expression ;
(* c[i], or a slice c[start:end] / c[start:end:step], each part optional
   (s[:5], l[::-1]). A slice cannot be assigned to (checked by the resolver). *)
index = "[" ( expression | slice ) "]" ;
slice = [ expression ] ":" [ expression ] [ ":" [ expression ] ] ;
field = "." IDENT ;
arguments = expression { "," expression } ;

primary = NUMBER
        | STRING
        | "true" | "false" | "none" | "self"
        | IDENT
        | list_literal
        | func_expr
        | arrow_func
        | "(" expression ")" ;
list_literal = "[" [ arguments ] "]" ;
(* Anonymous functions (closures, like every inner function). An arrow is
   told from "(" expression ")" by "()", "(...", "(x," or "(x =", or a
   "=>" right after "(x)". *)
func_expr = "func" "(" [ params ] ")" block ;
arrow_func = "(" [ params ] ")" "=>" ( expression | block ) ;

(* ---------- Tokens (produced by the lexer) ---------- *)

IDENT = letter { letter | digit } ;            (* must not be a keyword *)
NUMBER = digit { digit } [ "." digit { digit } ] ;
STRING = '"' { dq_char | escape } '"'
       | "'" { sq_char | escape } "'" ;
escape = "\" ( "n" | "t" | '"' | "'" | "\" ) ;
dq_char = any_char - ( '"' | "\" | newline ) ;
sq_char = any_char - ( "'" | "\" | newline ) ;
letter = "a".."z" | "A".."Z" | "_" ;
digit = "0".."9" ;
any_char = ? any character ? ;
newline = ? line break ? ;

(* Comments: # to end of line, and /* */ block comments. The lexer discards them. *)
```

## Rules the grammar does not express (checked by the resolver)

- The left side of an assignment, and the operand of `++`/`--` (prefix or postfix), must be `IDENT`, `x[i]` or `x.field`.
- `var` is forbidden inside loops (at any nesting level).
- `vemp` only inside functions; a `for` variable is an implicit `vemp`.
- No name may repeat another visible one (var, vemp, parameter or global).
- `break` and `continue` only inside a loop (inside a `case` they belong to the enclosing loop); `return` only inside a function; `create` only `return;` without a value.
- `self` only inside the methods of an `obj`.
- At most one `prog`, with 0 or 2 parameters (`argc`, `argv`). A file without `prog` is valid and runs nothing. The `prog` cannot be called.
- An `obj` cannot repeat field or method names. Without `create`, it is built with no arguments: `Point()`.
- A `var` is visible from its declaration to the end of its function; using it before is a `NameError`. A global's initial value may only use the globals declared above it (functions may use any).
- Functions, objects, natives and modules are not values: they can only be called (`f(...)`, `Point(...)`) or, for modules, used as `m.name`. Only those names and methods can be called.
- The number of arguments of `f(...)`, `Point(...)`, natives and `self.m(...)` is checked before running (`TypeError`); other methods, at runtime.
- `self.name` must be a declared field or method of the `obj` (`AttributeError`); a field cannot be called and a method is not a value.
- No value may repeat in the `case`s of one `switch`.

## Modules (imports)

- `import utils` loads `utils.pce` from the directory of the importing file. Packages (directories) are postponed.
- A module is loaded once, however many files import it.
- Circular imports are an error.
- Modules are not values (like functions): `utils.parse` is resolved before running; `var m = utils;` is an error.
- Imported names follow the usual rule: no name may repeat another visible one.
- A module offers only what it declares itself (its `func`, `obj` and `var`): not its `prog`, nor what it imports.
- A module's global variables are read-only outside it (`m.x = 1;`, or `x = 1;` after `from m import x`, are errors); they are always read with their current value, and the module's own functions may change them.
- In the REPL, `import m;` loads `m.pce` from the current directory.
- Errors: `ImportError` (no such module, circular import, `from m import x` with no `x`), `AttributeError` (`m.x` with no `x`).
- Only the `prog` of the main file runs; the `prog` of an imported module is ignored (like `if __name__ == "__main__"`).

**Modules of the language** (`sys`, `math`, `io`, `random`, `time`, `os`, `signal`, `json`; phase 10):
- Imported like any other: `import math;`, `import math as m;`, `from math import sqrt, pi as PI;`. They are built in: no file is read.
- A user's `m.pce` next to the importing file, named like one of them, is an `ImportError` when imported: `'math' is a module of the language: rename math.pce`.
- Their functions are values like the global natives (`var f = math.sqrt;`, `l.map(math.sqrt)`), shown `<built-in math.sqrt>`; their arity is checked before running. Their constants (`math.pi`, `sys.argv`) are read-only (`math.pi = 3;` is a `SyntaxError`).
- Naming rule: no member is called like a reserved word or a global native, so any of them can be imported alone (checked by `tests/check_members.sh`; `./pycee --members` lists them). The one exception is `sys.exit`: in the REPL, where `exit()` already exists, `from sys import exit` is an `ImportError`.

**Named arguments of natives:** only where documented: `print(..., sep, end, file)`, `io.eprint(..., sep, end)`, `json.dump(v, indent = n)` (keyword-only: they go after every positional argument and can only be given by name) and `os.list_dir(p)` (positional or named). Any other native takes no named arguments (`TypeError`).

## Planned syntax (decided 2026-11-05, not implemented yet)

Decided in the design review; see SESSION.md for the reasons and TYPES.md for the semantics. Each piece is added to the grammar above when it is implemented.

```ebnf

(* Manual in the REPL (planned, not written yet): the argument is not evaluated. *)
man_stmt = "man" "(" ( keyword | IDENT [ "." IDENT ] ) ")" ;


(* Arguments: "k: v" pairs only inside dict(...) (phase 9). *)
argument = expression ":" expression ;

(* Expressions, lowest to highest precedence. *)
expression = conditional ;
conditional = or_expr [ "?" expression ":" conditional ] ;
(* or_expr, and_expr, not_expr as above, then: *)
comparison = bit_or [ ( "is" [ "not" ] | [ "not" ] "in" | "<" | "<=" | ">" | ">=" ) bit_or ] ;
bit_or = bit_xor { "|" bit_xor } ;
bit_xor = bit_and { "^" bit_and } ;
bit_and = shift { "&" shift } ;
shift = sum { ( "<<" | ">>" ) sum } ;
unary = ( "-" | "~" | "++" | "--" ) unary | postfix ;

(* Comprehensions: in list literals and alone inside tuple(...), set(...),
   dict(...), any(...), all(...), sum(...), min(...), max(...). As in Python,
   any number of for and if clauses after the first for, left to right:
   each one runs inside the one before it. *)
list_literal = "[" [ expression ( comprehension | { "," expression } ) ] "]" ;
comprehension = comp_for { comp_for | "if" "(" expression ")" } ;
comp_for = "for" "(" IDENT { "," IDENT } "in" expression ")" ;


top_decl = [ "private" ] ( func_decl | prog_decl | obj_decl | var_decl | const_decl ) ;
```

**Literals (lexer)**
- Numbers: `1_000_000` (underscores between digits), `0xFF`, `0b1010`, `0o17` (ints); `1e9`, `2.5e-3` (floats); a `.` or an exponent makes a float. An int that does not fit in 64 bits is a `SyntaxError`.
- Strings: escapes `\r \0 \xHH \u{XXXX}` as well; `"""..."""` multi-line; `r"..."` raw; `f"... {expr} ..."` with `{{` `}}` for braces and Python format specifiers after `:` (`{x:.2f}`, `{n:>5}`).

**Modules:** everything a module declares is exported except what is `private`. `export` stays reserved with no planned use.

**with:** `with (expr as name) block` is shorthand for `try`/`finally` calling `name.close()`.

**Reserved words** (rejected as names from now on, even before they do anything): `extends` `super` `enum` `private` `static` `match` `async` `await` `export` `with` `man`. (`const` and `do` work since phase 6; `try` `except` `finally` `raise` `assert` since phase 7b.) The character `@` is reserved (possible decorators).

**New tokens:** `?` `:` `=>` `...` `&` `|` `^` `~` `<<` `>>`.

## Native functions (not syntax, called like any other function)

- Global: `print(..., sep, end)`, `input()`, `str(x)`, `num(s)`, `int(x)`, `float(x)`, `range(...)`, `tuple(...)`, `set(...)`, `dict(...)`. The rest in standard modules (`import math`, `import sys`, `import io`).
- Every value has methods (`s.split(",")`, `l.append(x)`, `l.join(",")`, `2.pow(3)`...); see [TYPES.md](TYPES.md), the reference for every type (operators, methods, how it is shown).

## Token list (lexer)

95 token types (`TOKEN_LIST` in `include/front/tokens.h` is the source of truth). The tables below list the original ones; added on 2026-11-05: `TOK_INT` / `TOK_FLOAT` (replacing `TOK_NUMBER`), the 17 reserved words (`TOK_TRY`... `TOK_WITH`, described as "reserved word 'try'" in messages), `TOK_MAN` (`man`, reserved), `TOK_AT` (`@`, reserved), `TOK_QUESTION` `?`, `TOK_COLON` `:`, `TOK_ARROW` `=>`, `TOK_ELLIPSIS` `...`, the bit operators `& | ^ ~ << >>` and their assignments `&= |= ^= <<= >>=`.

**Literals and special (5)**

| Token | Lexeme |
|---|---|
| `TOK_IDENT` | identifier that is not a keyword |
| `TOK_INT` | `42`, `1_000`, `0xFF`, `0b1010`, `0o17` (no sign: `-1` is `-` then `1`) |
| `TOK_FLOAT` | `3.14`, `1e9`, `2.5e-3` (`1.` and `.5` are not numbers) |
| `TOK_STRING` | `STRING` (escapes are validated by the lexer, decoded later) |
| `TOK_EOF` | end of buffer, `offset` = buffer length, `len` = 0 |

**Keywords (29)**

| Group | Tokens |
|---|---|
| Declarations | `func` `prog` `obj` `var` `vemp` |
| Modules | `import` `from` `as` |
| Control flow | `if` `elif` `else` `while` `for` `in` `switch` `case` `default` `break` `continue` `return` |
| Logic and comparison | `and` `or` `xor` `not` `is` |
| Literal values | `true` `false` `none` `self` |

Named `TOK_` + uppercase keyword (`TOK_FUNC`, `TOK_ELIF`...).
`is not` is two tokens; the parser joins them.
`print`, `range`, `pow` and `create` are not keywords: they are `TOK_IDENT`.

**Punctuation (9)**

| Token | Lexeme | Token | Lexeme |
|---|---|---|---|
| `TOK_LPAREN` | `(` | `TOK_RPAREN` | `)` |
| `TOK_LBRACE` | `{` | `TOK_RBRACE` | `}` |
| `TOK_LBRACKET` | `[` | `TOK_RBRACKET` | `]` |
| `TOK_COMMA` | `,` | `TOK_SEMICOLON` | `;` |
| `TOK_DOT` | `.` | | |

**Operators (19)**, maximal munch (longest match wins)

| Arithmetic | | Comparison | | Assignment | |
|---|---|---|---|---|---|
| `TOK_PLUS` | `+` | `TOK_LT` | `<` | `TOK_ASSIGN` | `=` |
| `TOK_MINUS` | `-` | `TOK_LE` | `<=` | `TOK_PLUS_EQ` | `+=` |
| `TOK_STAR` | `*` | `TOK_GT` | `>` | `TOK_MINUS_EQ` | `-=` |
| `TOK_SLASH` | `/` | `TOK_GE` | `>=` | `TOK_STAR_EQ` | `*=` |
| `TOK_SLASH_SLASH` | `//` | | | `TOK_SLASH_EQ` | `/=` |
| `TOK_PERCENT` | `%` | | | `TOK_SLASH_SLASH_EQ` | `//=` |
| | | | | `TOK_PERCENT_EQ` | `%=` |
| | | | | `TOK_PLUS_PLUS` | `++` |
| | | | | `TOK_MINUS_MINUS` | `--` |

There is no `==`, `!=` or `!`: a lone `!` is a `SyntaxError`.

Maximal munch, as in C: `a--b` lexes as `a` `--` `b` (a `SyntaxError` in the parser), `a---b` as `a` `--` `-` `b` (that is, `(a--) - b`), and `--x` is a prefix decrement, not a double negation. Double negation needs a space or parentheses: `- -x`, `-(-x)`.

**Lexer rules not shown in the tables**

- Comments are checked before operators: at `/`, if the next char is `*` it is a block comment (`a/*b` starts a comment, as in C); `#` starts a line comment. `//` is never a comment.
- Block comments do not nest: the first `*/` closes them. An unclosed `/*` is a `SyntaxError` pointing at the `/*`.
- Whitespace (space, `\t`, `\r`, `\n`) only separates tokens.
- A letter right after a number is an error (`3x`: invalid number literal), as in Python. `_` between digits is allowed (`12_000`, planned; today still an error), but not at the end or doubled. `1.x` is `1` `.` `x`.
- Leading zeros are an error (`007`, `00.5`: leading zeros are not permitted); `0` and `0.5` are valid.
- A `STRING` token keeps its quotes; escapes are only validated by the lexer and decoded when the parser builds the value. Non-ASCII bytes (UTF-8) are allowed inside strings only.
- Errors (all `SyntaxError`): invalid character, invalid number literal, leading zeros, unterminated string literal (also a newline inside a string), invalid escape sequence, unterminated block comment.
