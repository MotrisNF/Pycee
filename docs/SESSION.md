# Estado del proyecto (para retomar)

Última actualización: 2026-11-05. Este fichero basta para continuar desde cualquier máquina: qué es pycee, cómo se trabaja, dónde está cada cosa, qué está hecho y qué toca ahora.

## Cómo retomar en 5 minutos
1. Compilar y comprobar que todo está verde:
   ```
   make && make test        # 357 tests (lexer, parser, resolver, imports, eval, run), con ASan + UBSan
   make stress              # los mismos tests con el GC recolectando en cada reserva
   make release             # ./pycee-release (-O2), para valgrind y medir
   sh tests/run_torturette.sh   # inyección de fallos de malloc: 71 casos OK
   ```
   torturette (proyecto del autor, MIT) va dentro del repositorio: `tests/torturette/` tiene el ejecutable y el `injector.so` que precarga (lo busca junto a sí mismo), compilados para Linux x86-64 con glibc. El proyecto no depende de nada externo salvo `gcc`, `make`, la libc y valgrind. En otra arquitectura habría que recompilar torturette desde su fuente (repositorio propio del autor, con libft como submódulo).
2. Leer "Dónde estamos" (abajo) y la fase actual en [ROADMAP.md](ROADMAP.md).
3. La referencia del lenguaje: [TYPES.md](TYPES.md) (cada tipo, operadores y métodos) y [EBNF.md](EBNF.md) (gramática y reglas del resolver). Se mantienen al día con cada cambio.

## Forma de trabajar
- **El autor decide** todo lo que es diseño del lenguaje o comportamiento visible (sintaxis, nombres, mensajes, qué es error). **El código lo escribe quien implementa**, que decide solo los detalles internos y los explica.
- Antes de programar una fase o algo con comportamiento visible: **hacer al autor todas las preguntas abiertas de una vez, al principio**, con una recomendación en cada una. Lo que se decida sin él se apunta en este fichero como "decidido en la implementación" para que lo revise.
- No se hacen commits: el proyecto vive en local (hay un `git init` con 3 commits antiguos; el resto está sin confirmar).
- Estilo del código: `return (x);`, declaraciones al principio de la función, 4 espacios, comentarios de bloque en inglés, X-macros para tablas, errores que suben con `t_error` (nunca `exit`, salvo `fatal_oom`). Mensajes de error del lenguaje en inglés. Documentación en español (salvo TYPES.md y EBNF.md, en inglés).
- **Tras cada paso:** `make test`, `make stress`, valgrind sobre los tests nuevos (`valgrind -q --leak-check=full --error-exitcode=9 ./pycee-release f.pce`) y torturette. Un test nuevo de un comportamiento importante se añade también a `tests/run_torturette.sh`.

## Objetivos del lenguaje (del autor)
- **Gestión de memoria 100 % segura**, en el intérprete (ninguna fuga, doble free ni acceso inválido en ningún camino, incluidos los fallos de reserva) y en el lenguaje (ningún programa pycee puede corromper memoria). Un hueco de seguridad de memoria es siempre un bloqueo.
- **Juntar la parte cómoda de todos los lenguajes**: ante varias opciones, la que más facilite la vida a quien programa (y decir de qué lenguaje viene).
- Al terminar: un **manual en el REPL** (`man`) y un **tutorial con ejercicios al estilo 42** (ver ROADMAP).

## Dónde estamos
- **Fases 0 a 9 terminadas** (diseño, infraestructura, lexer, parser, resolver, expresiones, sentencias, funciones y closures, errores capturables, GC, tipos y objetos).
- Hechos además, tras revisar con el autor lo de la fase 9: comprensiones con varios `for` e `if`; los métodos que modifican una lista/set/dict la devuelven (`l.append(3).sort()`); `extend`; `any()`/`all()` sin función; globales `any/all/sum/min/max`; `x.id()`; `from m import a as b`; `input()` en un terminal vuelve a esperar tras Ctrl+D.
- **Siguiente: fase 10 (librería estándar), paso 1.** La API está decidida entera (sección "Fase 10" abajo). Pasos:
  1. Módulos nativos: el resolver conoce `sys`, `math`, `io`, `random`, `time`, `os`, `json` (miembros: funciones con aridad y constantes); `import`, `from ... import ... as`; `ImportError` si hay un `m.pce` del usuario con ese nombre; un test que compruebe que ningún miembro se llama como una palabra reservada o una nativa global; argumentos con nombre en nativas (lo necesita `print(sep, end, file)`).
  2. `sys`. 3. `math`. 4. `io` (tipo `file` que se cierra al liberarlo el GC; `IOError` y sus hijos; `print(..., file)`, `eprint`, `stdin/stdout/stderr`). 5. `time` y `random`. 6. `os`. 7. `json`. 8. Hito: un programa que lee un fichero, lo procesa y escribe el resultado.
- Después: fase 11 (rendimiento: las llamadas son ~3,7× más lentas que en Python; internar nombres, umbrales del GC) y fase 12 (calidad, decoradores `@`, dudas pendientes).

## Ficheros
| Ruta | Contenido |
|---|---|
| [README.md](../README.md) | Qué es pycee, por qué existe, ejemplos |
| [docs/ROADMAP.md](ROADMAP.md) | Hoja de ruta en 13 fases (0–12) con hitos, y lo que va después |
| [docs/TYPES.md](TYPES.md) | Referencia de cada tipo: sintaxis, cómo se muestra, operadores y métodos |
| [docs/EBNF.md](EBNF.md) | Gramática completa y reglas del resolver |
| [examples/](../examples/) | `example1..7.pce`, cada uno con un comentario de lo que muestra |
| `src/`, `include/` | El intérprete (ver "Arquitectura") |
| `tests/` | `run_tests.sh`, `run_torturette.sh` y las suites |
| `tests/torturette/` | torturette (inyección de fallos de `malloc`) + `injector.so` + su licencia |
| `tools/gen_unicode.py` | Genera `src/types/unicode_data.c` desde Python (Unicode 16.0.0) |
| `test.pce` | Fichero de pruebas del autor: no tocarlo |

## Arquitectura
- Intérprete **tree-walking** en C (libc permitida), tipado dinámico. Etapas: lexer → parser (AST en una arena) → resolver (comprueba reglas y anota cada nombre con su slot) → intérprete.
- Capas (una capa solo usa las de arriba; el Makefile tiene una variable por capa, añadir un `.c` = añadir su nombre):
  ```
  src/core/      arena errors reader source stack buffer utf8
  src/front/     tokens lexer parser ast_print
  src/resolver/  symtab scope declarations objects expressions statements modules resolver
  src/runtime/   value heap interp eval exec call ops format fmtspec methods
  src/types/     number range tuple list string seq table dict object error unicode_data
  src/natives/   natives (las globales de NATIVE_LIST; fase 10: módulos)
  ```
  Cabeceras en `include/<capa>/`; `#include "core/arena.h"` con `-Iinclude`. `resolver_internal.h` (prefijo `rs_`) e `interp_internal.h` (prefijo `rt_`) son internas.
- **Posiciones**: cada fuente cargada (fichero, módulo, línea del REPL `<stdin:N>`) tiene un rango de offsets global; un `size_t` identifica fichero y posición (`source.c`).
- **Errores**: `t_error` (tipo, offset, len, mensaje, traceback de 16 marcos internos y 16 externos, contexto encadenado) sube hasta `main`. Errores de análisis sin traceback; de ejecución, al estilo Python. La falta de memoria es fatal (`fatal_oom` libera la sesión entera antes de salir). Guarda de pila real (`core/stack.c`): anidación o recursión profundas dan `SyntaxError: too deeply nested` / `RecursionError: stack exhausted`, nunca *segfault*. Códigos de salida: 0 bien, 1 error del lenguaje, 2 uso incorrecto o fichero ilegible.
- **Resolver**: símbolos en `t_symtab` (globales con índice hash); sin shadowing; `t_ref` (`REF_GLOBAL/LOCAL/CAPTURE/NATIVE/FUNC/OBJ/MODULE`, slot, módulo); `FUNC.nslots` = tamaño del marco; closures con `t_capture`; módulos `.pce` cargados una vez, import circular = error. Objetos en `objects.c`: posiciones de campos (los del padre primero), estáticos como globales ocultas, `self` = parámetro oculto en el slot 0, `super` resuelto al analizar.
- **Valores**: `t_value` de 16 bytes (`VAL_NONE/BOOL/INT/FLOAT/OBJ`). Objetos del heap con cabecera `t_obj` (tipo, marca, `id`, `next`): `OBJ_STRING RANGE TUPLE FUNCTION NATIVE BOUND CELL ERROR LIST DICT SET INSTANCE CLASS METHOD`. Strings siempre UTF-8 válido (lo de fuera se sanea con U+FFFD), con `nchars` en caché.
- **GC** mark & sweep no móvil (`runtime/heap.c`), con pila gris. Raíces: la pila de valores del intérprete, las globales de cada módulo e `in->ret`. **Regla de oro: un valor que deba sobrevivir a otra reserva va en la pila de valores, nunca solo en una variable de C.** `make stress` (`PYCEE_GC_STRESS=1`) recolecta en cada reserva y lo comprueba. `x.id()` es un contador en `heap_alloc` (no la dirección).
- **Ejecución**: marcos en la pila de valores (`in->frame`); `t_exec` = ERROR/OK/BREAK/CONTINUE/RETURN con `jump_levels` para `break outer`; variables capturadas en celdas (`t_cell`). Métodos: una tabla `t_method` por tipo más una común (`type`, `is_type`, `id`), búsqueda por nombre (internar en la fase 11). Nativas globales: una sola X-macro `NATIVE_LIST` (`include/resolver/natives.h`) para resolver y runtime. `rt_call` deja que C llame a funciones del programa; `rt_collect` recorre cualquier iterable con el mismo `for` del lenguaje. Mostrar valores: `rt_show` con banco de buffers (`rt_buf_take/give`) para que `to_str` pueda ejecutarse en medio.
- **Comprensiones**: `NODE_COMPREHENSION` = cadena de `FOR`/`IF` cuyo cuerpo final es `NODE_COMP_ADD`; el resultado crece en un slot oculto del marco. Dentro de `tuple/set/dict/any/all/sum/min/max(...)` el resolver convierte la llamada en la comprensión (`COMP_*`); `any`/`all` se paran al saber la respuesta.
- **Tests**: `tests/run_tests.sh [--update] [suite]`. Cada `.pce` con su `.out` (stdout + stderr + `exit=N`); un `.in` al lado es su stdin; un `.out` que empieza por `#contains` solo exige esas líneas. Suites: `lexer` (`--tokens`), `parser` (`--ast`), `resolver` e `imports` (`--check`: analiza sin ejecutar), `eval` (el fichero se teclea en el REPL), `run` (ejecuta el `prog`).

## Resumen del lenguaje (el detalle manda en TYPES.md y EBNF.md)
- Nivel superior: `import`, `func`, `prog`, `obj`, `var`, `const`. Como mucho un `prog`: `prog main()` o `prog main(argc, argv)` (`argv` es una tupla, `argv[0]` la ruta); su `return` es el código de salida (0..255).
- Sintaxis: `;` en toda sentencia, llaves siempre, paréntesis en las condiciones. Comentarios `#` y `/* */` (`//` es división entera). `if/elif/else`, `while`, `do { } while (c);`, `for (x in it) { } else { }`, `for (k, v in it)`, `switch` (sin fallthrough), `break`/`continue` y `break outer;`.
- Variables: `var` (vive en toda la función; prohibida en bucles), `vemp` (solo su bloque), `const`. Ningún nombre puede tapar a otro visible, ni a una nativa global. `func` y `obj` son nombres constantes.
- Números: int de 64 bits exacto (`OverflowError`) y float; `/` da float, `//` y `%` truncan como C. Bits `& | ^ ~ << >>`. Lógica `and or not xor`; comparación `is`, `is not`, `< <= > >=` (sin `==`, no encadenables); las condiciones exigen un bool. `++`/`--` como C con orden definido. Ternario `c ? a : b`.
- Strings Unicode inmutables (`"año".len()` es 3), cortes, `in` (substring), f-strings con el mini-lenguaje de formato de Python. Listas `[...]`, tuplas `tuple(...)`, sets `set(...)`, dicts `dict("a": 1)`, `range` como Python. Comprensiones como Python.
- Funciones: valores, lambdas `(x) => x * 2`, closures que capturan la variable, valores por defecto, argumentos con nombre, `...rest` y `f(...t)`.
- Objetos: `obj P extends Q` (o `obj P(Q)`); los campos se declaran con `self.x = ...` dentro de `create` (los demás métodos no pueden añadir); `private`, `static`, `super`; métodos especiales `to_str`, `compare`, `len`, `get`/`set`, `iter`/`has_next`/`next`.
- Errores: `try { } except Tipo, Otro (e) { } finally { }`, `raise`, `assert(c, msg)`, base `Error`; los `obj` pueden extender un error.
- Módulos: `import m;`, `import m as x;`, `from m import a, b as c;`; un módulo es `m.pce` junto al fichero que importa; sus globales son de solo lectura desde fuera.

## Decisiones del autor por tema
Las anteriores a la fase 5 se revisaron todas en un repaso general ("revisión de cabos sueltos"); lo que sigue es lo vigente.

**Generales**
- Errores al estilo Python; el primer error no capturado termina el programa (en el REPL, vuelve al prompt). REPL: `>>> `, `... ` para continuar, eco del valor salvo `none`, `;` final opcional; `exit()`/`exit(n)` solo en el REPL (en ficheros, `sys.exit`).
- Todo valor tiene métodos (`s.split(",")`, `2.pow(3)`, `l.len()`), porque es lo más cómodo.
- Sin conversiones implícitas: `"a" + 5` es `TypeError`. Tipos distintos: `is` da `false`; `<` entre tipos distintos es `TypeError`.
- Listas, sets, dicts y objetos se comparten (asignar no copia). `l += x` amplía en el sitio.
- Palabras reservadas sin uso todavía: `enum match async await export with man` y el símbolo `@`. `man(...)` será el manual del REPL.

**Fase 6**: `range` como Python; control de flujo en el REPL; `break outer` actúa sobre el bucle que envuelve al actual (no hay etiquetas declaradas; `outer` no es palabra reservada); desempaquetar con número distinto: `SyntaxError` si se ve al analizar, `AssignmentError` en ejecución.

**Fase 7**: tuplas básicas; `return` de `prog` con las reglas de `exit`; `func` con nombre dentro de otra; un valor por defecto puede usar los parámetros anteriores; se muestran `<func doble>`, `<func lambda>`, `<built-in print>`, `<method p.move>`, y su `type()` es `"function"`.

**Fase 7b**: `except ValueError (e)` (sin `as`; escribir `as` da un error que explica la forma correcta), varios tipos con comas, nombre opcional, sin tipo captura todo, sin `else` en `try`; `raise ValueError("msg")` / `raise e;`; `RecursionError` se captura, `SystemExit` no; `return/break/continue` dentro de `finally` es `SyntaxError`; `e.type() message() line() file() traceback()`, `print(e)` → `ValueError: msg`; errores encadenados como Python; tipo base `Error`.

**Fase 8**: el control del GC desde el programa va en `sys` (fase 10).

**Fase 9**
- Strings: `capitalize` (no `title`), `strip/lstrip/rstrip(chars)`, `split()` sin argumento, `index`, `reverse`, `is_digit/is_alpha/is_alnum/is_space/is_ascii` (lista abierta); `join` en las dos formas; `in` es substring; UTF-8 inválido → U+FFFD.
- Listas/tuplas: `append`, `pop()`/`pop(i)`, `enumerate()`, `map`, `filter`, `reduce`, `sort(key, reverse)` (admite lambdas), `any`, `all`, `each`, `sum`, `min`, `max`. **Los métodos que modifican devuelven la propia lista** (`append insert extend remove clear sort reverse`); copia ordenada con `l.copy().sort()`. `any()`/`all()` también sin función (cada elemento debe ser bool).
- Globales `any/all/sum/min/max(x)`: con cualquier iterable o con una comprensión dentro (`sum(x * x for (x in l))`); nombres reservados como el resto de nativas (`var max` no vale).
- Comprensiones: varios `for` e `if`, como Python ("fundamental").
- `x.id()`: el mismo para todo nombre que apunta al mismo valor; números, bools y `none` → `TypeError`.
- Dicts: `for (x in d)` da cada par como un dict de longitud 1; `for (k, v in d)` desempaqueta; `keys()`/`values()`.
- Objetos: campos declarados en `create` con `self.x = ...` (los demás métodos los leen y cambian, no añaden); `super.create(...)`/`super.m()`; un `obj` puede extender cualquier tipo de error; campos no `private` accesibles desde fuera; `static` también a través de un objeto.

**Fase 10 (API decidida el 2026-11-05)**
- **Regla general:** ningún miembro de un módulo del sistema se llama como una palabra reservada o una nativa global, para que todo se pueda importar suelto (`from math import sqrt, pi`). Un `m.pce` del usuario con el nombre de un módulo del sistema → `ImportError` al importarlo ("'math' is a module of the language: rename math.pce").
- `sys`: `argv`, `exit(code)`, `recursion_limits(n)`, `platform`, `version`; `gc_collect()` (devuelve los objetos liberados), `memory()` (bytes en uso), `gc_stats()` (dict con `collections`, `objects`, `bytes`, `next_gc`), `gc_enable()`, `gc_disable()`, `gc_set_min(bytes)` (por debajo nunca recolecta) y `gc_set_max(bytes)` (al llegar siempre recolecta), independientes y cambiables durante la ejecución.
- `math`: `pi e tau inf nan`; `sqrt abs round floor ceil pow trunc` (también como métodos de número); `sin cos tan asin acos atan atan2 sinh cosh tanh degrees radians exp log(x[, base]) log2 log10 hypot gcd lcm factorial is_nan is_inf is_finite`. Fuera de dominio → `ValueError`.
- `io`: funciones directas `read_file`, `read_lines` (**cada línea conserva su "\n"**), `write_file`, `append_file`; y objeto `file` con `open(path, "r"|"w"|"a")`: `read`, `read_line` (`""` al final), `read_lines`, `write` (devuelve el fichero), `has_next`, `close`, `is_closed`, `for (linea in f)`; se muestra `<file "a.txt">`; usarlo cerrado → `IOError`; se cierra al liberarlo el GC o al salir. `io.stdin/stdout/stderr` (no se cierran); `print(..., sep, end, file = io.stderr)` **y** `io.eprint(...)`. UTF-8; rutas relativas al directorio actual. Errores: `IOError` (hijo de `Error`) → `FileNotFoundError`, `PermissionError`; el mensaje lleva la ruta.
- `random`: `rand_int(a, b)` (extremos incluidos), `rand_float()` [0, 1), `choice`, `shuffle(l)` (en el sitio, devuelve la lista), `sample(seq, k)`, `seed(n)`; sin semilla, distinto en cada ejecución.
- `time`: `now()`, `clock()` (monótono), `sleep(s)`.
- `os`: `exists is_file is_dir list_dir(p = ".")` (ordenado) `remove rename make_dir remove_dir cwd env(name)` (`none` si no existe) `join base_name dir_name`; errores `IOError`.
- `json`: `parse(text)` (`ValueError` con línea y columna) y `dump(value, indent = n)` (tuplas → listas; claves no string, sets, objetos y funciones → `TypeError`; `nan`/`inf` → `ValueError`).
- `input()`: tras Ctrl+D en un terminal, la siguiente llamada vuelve a esperar (como Python); con fichero o tubería sigue en EOF.

**Para la fase 12** (decidido aplazar): decoradores `@` para getters/setters de campos `private` (idea del autor, a concretar con él), si `sort` acepta un comparador de dos elementos, y el mensaje "found end of file" de un error dentro de un campo de f-string (debería decir "found '}'").

## Decidido en la implementación (el autor lo vio el 2026-11-05; se puede cambiar)
- Parámetros: sin defecto no puede ir tras uno con defecto; `...rest` el último; posicionales antes que con nombre. `var a, b = 1, 2;` vale. El traceback solo aparece si el error cruzó una llamada.
- `const` dentro de una función vive en su bloque (se permite en bucles). En el REPL el eco solo es del nivel superior y un `else` va en la misma línea (`} else {`).
- Un error capturado es un valor nuevo; repr `ValueError("msg")`, str `ValueError: msg`; mensaje de `assert` opcional.
- f-strings: sin `{x=}`, sin `!r`, sin campos anidados en el formato; `f"{1.0:.2}"` da `1` (formato `g`; Python da `1.0`).
- Listas: `is` compara elementos (`l.copy() is l` es `true`; para la identidad, `id()`); `filter/any/all` exigen bool; cambiar el tamaño de la lista dentro de `map/filter/sort...` → `RuntimeError`; `map`/`filter` sobre una tupla dan tupla; `reduce` sin inicial empieza por el primero; `min/max/reduce` de algo vacío → `ValueError`; `sum` solo números; `insert` fuera de rango inserta en el extremo; `[[...]]` para una lista que se contiene.
- Sets y dicts: también devuelven el propio valor al modificarse (`add remove clear`); los sets van en orden de inserción; `1` y `1.0` son la misma clave; `d.get(k)` sin defecto da `none`; un `nan` como clave no se encuentra nunca.
- Objetos: un `private` es solo del `obj` que lo declara y solo por `self`; un hijo puede redefinir un método con otra aridad; `type`/`is_type` no se pueden redefinir, `id` sí (es un nombre de campo habitual); sin `create` no acepta argumentos (si extiende un error, un mensaje opcional); `<Point>`, `<obj Point>`, `<method Point.move>`; `to_str` se usa también en listas y en el REPL; `compare` funciona con el objeto a la derecha; `to_str` que no da string o `compare` que no da número → `TypeError`.
- En el REPL, `l.append(2)` muestra la lista (ya no devuelve `none`).
- `from sys import exit` en el REPL choca con la global `exit` del REPL: se propone que sea error (las dos hacen lo mismo).

## Lecciones (para no repetir errores)
- **torturette encuentra lo que valgrind no ve**: los locales de la libc (`setlocale`/`newlocale`) dejan memoria sin liberar para siempre y valgrind lo oculta con `__libc_freeres`. Por eso las tablas Unicode son propias (`unicode_data.c`, comprobadas idénticas a Python en los 1.112.064 códigos).
- Nunca pasar como parámetro de salida un puntero a la pila de valores: puede crecer (realloc) mientras se usa. Usar una variable local y copiar después.
- La limpieza de la sesión se registra después de inicializar todo lo que no reserva (un fallo de `malloc` temprano liberaba basura).
- Un `for` comprueba también al terminar si la lista cambió de tamaño.
- El binario release (-O2) puede dar avisos que el debug no da (`maybe-uninitialized`): compilar los dos.

## Medidas
- `tests/big.pce` (3 MB): lexer 0,09 s, parser 0,22 s (44 MB), resolver 0,09 s con la tabla hash (1,31 s con búsqueda lineal).
- Cálculo: tree-walking ≈ 2–5× más lento que un bytecode tipo CPython; las llamadas a funciones, ~3,7× más lentas que en Python (objetivo de la fase 11).
