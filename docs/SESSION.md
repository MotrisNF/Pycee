# Estado del proyecto (para retomar)

Última actualización: 2026-11-06. Este fichero basta para continuar desde cualquier máquina: qué es pycee, cómo se trabaja, dónde está cada cosa, qué está hecho y qué toca ahora.

## Cómo retomar en 5 minutos
1. Compilar y comprobar que todo está verde:
   ```
   make && make test        # 394 tests (lexer, parser, resolver, imports, eval, run) + la regla de nombres de los módulos, con ASan + UBSan
   make stress              # los mismos tests con el GC recolectando en cada reserva
   make release             # ./pycee-release (-O2), para valgrind y medir
   sh tests/check_signals.sh    # señales reales desde fuera (también lo lanza make test)
   sh tests/run_torturette.sh   # inyección de fallos de malloc: 92 casos OK (unos 40 s)
   ```
   torturette (proyecto del autor, MIT) va dentro del repositorio: `tests/torturette/` tiene el ejecutable y el `injector.so` que precarga (lo busca junto a sí mismo), compilados para Linux x86-64 con glibc. El proyecto no depende de nada externo salvo `gcc`, `make`, la libc y valgrind. En otra arquitectura habría que recompilar torturette desde su fuente (repositorio propio del autor, con libft como submódulo).
   **Resaltado y errores en VS Code** (en cada máquina nueva, una vez; necesita `python3` y el comando `code`):
   ```
   python3 tools/pack_vscode.py                        # crea build/pycee-0.1.0.vsix
   code --install-extension build/pycee-0.1.0.vsix     # o Ctrl+Shift+P -> "Extensions: Install from VSIX..."
   ```
   Luego `Ctrl+Shift+P` -> "Developer: Reload Window". El error de `pycee --check` aparece al abrir o guardar un `.pce` si `pycee-release` o `pycee` están compilados en la raíz del proyecto (si no, ajustar `pycee.executable` en los ajustes). Si VS Code usa Remote-SSH o WSL, se instala igual desde una ventana conectada a esa máquina. Tras cambiar nativas, errores o módulos: volver a empaquetar e instalar.
2. Leer "Dónde estamos" (abajo) y la fase actual en [ROADMAP.md](ROADMAP.md).
3. La referencia del lenguaje: [TYPES.md](TYPES.md) (cada tipo, operadores y métodos) y [EBNF.md](EBNF.md) (gramática y reglas del resolver). Se mantienen al día con cada cambio.

## Forma de trabajar
- **El autor decide** todo lo que es diseño del lenguaje o comportamiento visible (sintaxis, nombres, mensajes, qué es error). **El código lo escribe quien implementa**, que decide solo los detalles internos y los explica.
- Antes de programar una fase o algo con comportamiento visible: **hacer al autor todas las preguntas abiertas de una vez, al principio**, con una recomendación en cada una. Lo que se decida sin él se apunta en este fichero como "decidido en la implementación" para que lo revise.
- **No se hacen commits: los hace el autor.** Quien implementa deja los cambios sin confirmar al terminar cada paso.
- Estilo del código: `return (x);`, declaraciones al principio de la función, 4 espacios, comentarios de bloque en inglés, X-macros para tablas, errores que suben con `t_error` (nunca `exit`, salvo `fatal_oom`). Mensajes de error del lenguaje en inglés. Documentación en español (salvo TYPES.md y EBNF.md, en inglés).
- **Tras cada paso:** `make test`, `make stress`, valgrind sobre los tests nuevos (`valgrind -q --leak-check=full --error-exitcode=9 ./pycee-release f.pce`) y torturette. Un test nuevo de un comportamiento importante se añade también a `tests/run_torturette.sh`.

## Objetivos del lenguaje (del autor)
- **Gestión de memoria 100 % segura**, en el intérprete (ninguna fuga, doble free ni acceso inválido en ningún camino, incluidos los fallos de reserva) y en el lenguaje (ningún programa pycee puede corromper memoria). Un hueco de seguridad de memoria es siempre un bloqueo.
- **Juntar la parte cómoda de todos los lenguajes**: ante varias opciones, la que más facilite la vida a quien programa (y decir de qué lenguaje viene).
- Al terminar: un **manual en el REPL** (`man`) y un **tutorial con ejercicios al estilo 42** (ver ROADMAP).

## Dónde estamos
- **Fases 0 a 10 terminadas** (diseño, infraestructura, lexer, parser, resolver, expresiones, sentencias, funciones y closures, errores capturables, GC, tipos y objetos, librería estándar).
- Hechos además, tras revisar con el autor lo de la fase 9: comprensiones con varios `for` e `if`; los métodos que modifican una lista/set/dict la devuelven (`l.append(3).sort()`); `extend`; `any()`/`all()` sin función; globales `any/all/sum/min/max`; `x.id()`; `from m import a as b`; `input()` en un terminal vuelve a esperar tras Ctrl+D.
- **Fase 10 terminada (2026-11-06)**: módulos `sys`, `math`, `io` (tipo `file`), `random`, `time`, `os`, `signal` (y `KeyboardInterrupt`) y `json`, cada uno documentado en TYPES.md. Hito: `examples/example8.pce`, un contador de palabras que usa `sys`, `io`, `os` y `json` (test: `tests/run/word_count.pce`, enlace al ejemplo, con sus argumentos en `word_count.args`). Además: extensión de VS Code (`editors/vscode/`) y primer ejercicio para el autor (`exercises/00_notas.md`).
- **Manual `man` hecho (2026-11-06)**: 314 temas en 165 páginas de `man/`, todos los ejemplos comprobados por `make test` (163), y `tools/check_man_coverage.py` exige una página para cada palabra reservada, nativa, miembro de módulo y método (291 nombres). **Siguiente: los ejercicios** (`exercises/es/`, `exercises/en/`). Luego la fase 11 (eficiencia): medir primero (`perf`, `callgrind`); las llamadas son ~3,7× más lentas que en Python; internar nombres, umbrales del GC. Después, fase 12 (calidad, decoradores `@`, dudas pendientes).
- Visto al escribir el manual (para el paso 0 de la fase 11, con las f-strings): `1 == 1` da "expected expression but found '='"; debería decir que pycee no tiene `==` y que se usa `is`.
- Pendientes de decidir por el autor: `key` en `max`/`min` (propuesta: como Python, solo con nombre, devuelve el elemento); avisos por inferencia de tipos en el editor (`s.foo()` con `s` un string conocido) para después de la fase 12.

## Ficheros
| Ruta | Contenido |
|---|---|
| [README.md](../README.md) | Qué es pycee, por qué existe, ejemplos |
| [docs/ROADMAP.md](ROADMAP.md) | Hoja de ruta en 13 fases (0–12) con hitos, y lo que va después |
| [docs/TYPES.md](TYPES.md) | Referencia de cada tipo: sintaxis, cómo se muestra, operadores y métodos |
| [docs/EBNF.md](EBNF.md) | Gramática completa y reglas del resolver |
| [examples/](../examples/) | `example1..8.pce`, cada uno con un comentario de lo que muestra (8: el hito de la fase 10) |
| `src/`, `include/` | El intérprete (ver "Arquitectura") |
| `tests/` | `run_tests.sh`, `run_torturette.sh`, `check_members.sh` y las suites |
| `tests/torturette/` | torturette (inyección de fallos de `malloc`) + `injector.so` + su licencia |
| `tools/gen_unicode.py` | Genera `src/types/unicode_data.c` desde Python (Unicode 16.0.0) |
| `man/` | Las páginas del manual (`man(tema)` en el REPL), una por fichero; `man/_notes/` son los apuntes del autor (no se compilan). Tras editar: `python3 tools/gen_man.py` |
| `editors/vscode/` | Extensión de VS Code: resaltado (gramática TextMate), comentarios/llaves/sangría y el error de `pycee --check` subrayado al abrir y guardar. Ver su README |
| `tools/gen_vscode_grammar.py`, `tools/pack_vscode.py` | Generan la gramática desde `natives.h` y empaquetan `build/pycee-0.1.0.vsix` (sin npm ni red). Tras añadir nativas, tipos de error o módulos: volver a empaquetar e instalar |
| `test.pce` | Fichero de pruebas del autor: no tocarlo |
| `exercises/` | Ejercicios para que el autor practique su lenguaje (embrión del tutorial al estilo 42); las soluciones no van en el repositorio |

## Arquitectura
- Intérprete **tree-walking** en C (libc permitida), tipado dinámico. Etapas: lexer → parser (AST en una arena) → resolver (comprueba reglas y anota cada nombre con su slot) → intérprete.
- Capas (una capa solo usa las de arriba; el Makefile tiene una variable por capa, añadir un `.c` = añadir su nombre):
  ```
  src/core/      arena errors reader source stack buffer utf8
  src/front/     tokens lexer parser ast_print
  src/resolver/  symtab scope declarations objects expressions statements modules resolver
  src/runtime/   value heap interp eval exec call ops format fmtspec methods signals
  src/types/     number range tuple list string seq table dict object error file unicode_data
  src/man/       man (búsqueda y descompresión) pager (paginador) man_data (generado por tools/gen_man.py)
  src/natives/   natives (las globales) y un fichero por módulo del lenguaje: sys math io random time os signal json
  ```
  Cabeceras en `include/<capa>/`; `#include "core/arena.h"` con `-Iinclude`. `resolver_internal.h` (prefijo `rs_`) e `interp_internal.h` (prefijo `rt_`) son internas.
- **Posiciones**: cada fuente cargada (fichero, módulo, línea del REPL `<stdin:N>`) tiene un rango de offsets global; un `size_t` identifica fichero y posición (`source.c`).
- **Errores**: `t_error` (tipo, offset, len, mensaje, traceback de 16 marcos internos y 16 externos, contexto encadenado) sube hasta `main`. Errores de análisis sin traceback; de ejecución, al estilo Python. La falta de memoria es fatal (`fatal_oom` libera la sesión entera antes de salir). Guarda de pila real (`core/stack.c`): anidación o recursión profundas dan `SyntaxError: too deeply nested` / `RecursionError: stack exhausted`, nunca *segfault*. Códigos de salida: 0 bien, 1 error del lenguaje, 2 uso incorrecto o fichero ilegible.
- **Resolver**: símbolos en `t_symtab` (globales con índice hash); sin shadowing; `t_ref` (`REF_GLOBAL/LOCAL/CAPTURE/NATIVE/FUNC/OBJ/MODULE`, slot, módulo); `FUNC.nslots` = tamaño del marco; closures con `t_capture`; módulos `.pce` cargados una vez, import circular = error. Objetos en `objects.c`: posiciones de campos (los del padre primero), estáticos como globales ocultas, `self` = parámetro oculto en el slot 0, `super` resuelto al analizar.
- **Valores**: `t_value` de 16 bytes (`VAL_NONE/BOOL/INT/FLOAT/OBJ`). Objetos del heap con cabecera `t_obj` (tipo, marca, `id`, `next`): `OBJ_STRING RANGE TUPLE FUNCTION NATIVE BOUND CELL ERROR LIST DICT SET INSTANCE CLASS METHOD FILE`. Un `OBJ_FILE` (`types/file.c`) guarda su `FILE *` y su ruta; el GC lo cierra al liberarlo y el fin de la sesión cierra todos (salvo los estándar). Strings siempre UTF-8 válido (lo de fuera se sanea con U+FFFD), con `nchars` en caché.
- **GC** mark & sweep no móvil (`runtime/heap.c`), con pila gris. Raíces: la pila de valores del intérprete, las globales de cada módulo e `in->ret`. **Regla de oro: un valor que deba sobrevivir a otra reserva va en la pila de valores, nunca solo en una variable de C.** `make stress` (`PYCEE_GC_STRESS=1`) recolecta en cada reserva y lo comprueba. `x.id()` es un contador en `heap_alloc` (no la dirección).
- **Ejecución**: marcos en la pila de valores (`in->frame`); `t_exec` = ERROR/OK/BREAK/CONTINUE/RETURN con `jump_levels` para `break outer`; variables capturadas en celdas (`t_cell`). Métodos: una tabla `t_method` por tipo más una común (`type`, `is_type`, `id`), búsqueda por nombre (internar en la fase 11). Nativas globales: una sola X-macro `NATIVE_LIST` (`include/resolver/natives.h`) para resolver y runtime. `rt_call` deja que C llame a funciones del programa; `rt_collect` recorre cualquier iterable con el mismo `for` del lenguaje. Mostrar valores: `rt_show` con banco de buffers (`rt_buf_take/give`) para que `to_str` pueda ejecutarse en medio.
- **Nativas y módulos del lenguaje** (fase 10): una sola tabla, `g_native_defs` (`src/resolver/native_defs.c`), hecha con las X-macros de `include/resolver/natives.h`: `NATIVE_LIST` (globales, slots 0..N-1) y `NATIVE_MEMBERS` (funciones `F` y constantes `C` de cada módulo). El runtime tiene `g_native_fns` en los mismos slots (`native_<nombre>` y `nat_<módulo>_<nombre>`, en `src/natives/<módulo>.c`, con `include/natives/modules.h`). Un módulo del lenguaje es un `t_module` sin fichero (`native` = `NMOD_*`): sus funciones son `SYM_NATIVE` y sus constantes `SYM_GLOBAL` de solo lectura, que el runtime rellena al inicializarlo (`nconst_<módulo>_<nombre>`). Argumentos con nombre: `names` (posicionales que se pueden nombrar) y `kw` (solo con nombre) de cada nativa; `rt_call_native` recoloca los argumentos y la nativa lee los `kw` con `KW(i)` (`none` si no se dieron). Lo que aún no está escrito usa `NATIVE_NOT_YET` (`NotImplementedError`). `./pycee --members` lista los miembros y `tests/check_members.sh` (dentro de `make test`) comprueba la regla de nombres.
- **Señales** (`runtime/signals.c`): el manejador en C solo apunta la señal (`g_pending`, `g_signal_pending`); `RT_POLL` la atiende en los puntos seguros (sentencia, bloque, llamada) y `rt_interrupted` donde una espera devolvió `EINTR`. Los manejadores del programa están en `in->sig_handlers` (raíces del GC). SIGINT sin manejador → `ERR_KEYBOARD_INTERRUPT` (no desciende de `Error`; `find_handler` solo lo da a un `except` que lo nombra). `main` instala SIGINT al empezar (`interp_signals_init`); salida 130.
- **Manual** (`man/`, `src/man/`): una página por `man/<tema>.txt` (primera línea `# tema [tema...]`: varios temas pueden compartir página, p. ej. `# list.map tuple.map`; bajo NAME, `tema - resumen`). `tools/gen_man.py` las comprime juntas (LZ77 propio) en `src/man/man_data.c`, con el índice de temas sin comprimir y el índice completo añadido a `pycee`; `make test` comprueba que está al día (`--check`) y ejecuta todos los EXAMPLES (`tools/check_man.py`: sesiones `>>>` y programas `$ cat`/`$ ./pycee [< entrada]`, los errores solo por su primera línea, y una línea que acaba en `...` solo exige su principio) y comprueba que todo nombre del lenguaje tiene página (`tools/check_man_coverage.py`). Al añadir una nativa, un método o un miembro: escribir su página o `make test` falla. `NODE_MAN` lo analiza solo `parse_repl_item`; `run_man` (interp.c) busca y muestra con buffers del pool. El paginador usa la pantalla alternativa y modo crudo, sin reservar memoria mientras corre, y restaura el terminal siempre (también con Ctrl+C, que se descarta).
- **Comprensiones**: `NODE_COMPREHENSION` = cadena de `FOR`/`IF` cuyo cuerpo final es `NODE_COMP_ADD`; el resultado crece en un slot oculto del marco. Dentro de `tuple/set/dict/any/all/sum/min/max(...)` el resolver convierte la llamada en la comprensión (`COMP_*`); `any`/`all` se paran al saber la respuesta.
- **Tests**: `tests/run_tests.sh [--update] [suite]`. Cada `.pce` con su `.out` (stdout + stderr + `exit=N`); un `.in` al lado es su stdin; un `.args`, sus argumentos; un `.out` que empieza por `#contains` solo exige esas líneas. Suites: `lexer` (`--tokens`), `parser` (`--ast`), `resolver` e `imports` (`--check`: analiza sin ejecutar), `eval` (el fichero se teclea en el REPL), `run` (ejecuta el `prog`).

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

**Fase 10 (API decidida el 2026-11-05; detalles cerrados el 2026-11-06, ver abajo)**
- **Regla general:** ningún miembro de un módulo del sistema se llama como una palabra reservada o una nativa global, para que todo se pueda importar suelto (`from math import sqrt, pi`). Un `m.pce` del usuario con el nombre de un módulo del sistema → `ImportError` al importarlo ("'math' is a module of the language: rename math.pce").
- `sys`: `argv`, `exit(code)`, `recursion_limit(n)`, `platform`, `version`; `gc_collect()` (devuelve los objetos liberados), `memory()` (bytes en uso), `gc_stats()` (dict con `collections`, `objects`, `bytes`, `next_gc`), `gc_enable()`, `gc_disable()`, `gc_set_min(bytes)` (por debajo nunca recolecta) y `gc_set_max(bytes)` (al llegar siempre recolecta), independientes y cambiables durante la ejecución.
- `math`: `pi e tau inf nan`; `sqrt abs round floor ceil pow trunc` (también como métodos de número); `sin cos tan asin acos atan atan2 sinh cosh tanh degrees radians exp log(x[, base]) log2 log10 hypot gcd lcm factorial is_nan is_inf is_finite`. Fuera de dominio → `ValueError`.
- `io`: funciones directas `read_file`, `read_lines` (**cada línea conserva su "\n"**), `write_file`, `append_file`; y objeto `file` con `open(path, "r"|"w"|"a")`: `read`, `read_line` (`""` al final), `read_lines`, `write` (devuelve el fichero), `has_next`, `close`, `is_closed`, `for (linea in f)`; se muestra `<file "a.txt">`; usarlo cerrado → `IOError`; se cierra al liberarlo el GC o al salir. `io.stdin/stdout/stderr` (no se cierran); `print(..., sep, end, file = io.stderr)` **y** `io.eprint(...)`. UTF-8; rutas relativas al directorio actual. Errores: `IOError` (hijo de `Error`) → `FileNotFoundError`, `PermissionError`; el mensaje lleva la ruta.
- `random`: `rand_int(a, b)` (extremos incluidos), `rand_float()` [0, 1), `choice`, `shuffle(l)` (en el sitio, devuelve la lista), `sample(seq, k)`, `seed(n)`; sin semilla, distinto en cada ejecución.
- `time`: `now()`, `clock()` (monótono), `sleep(s)`.
- `os`: `exists is_file is_dir list_dir(p = ".")` (ordenado) `remove rename make_dir remove_dir cwd env(name)` (`none` si no existe) `join base_name dir_name`; errores `IOError`.
- `json`: `parse(text)` (`ValueError` con línea y columna) y `dump(value, indent = n)` (tuplas → listas; claves no string, sets, objetos y funciones → `TypeError`; `nan`/`inf` → `ValueError`).
- `input()`: tras Ctrl+D en un terminal, la siguiente llamada vuelve a esperar (como Python); con fichero o tubería sigue en EOF.

**Fase 10: detalles decididos el 2026-11-06** (mandan sobre lo anterior si chocan)
- Las funciones de un módulo son valores: `var f = math.sqrt;`, `l.map(math.sqrt)`; se muestran `<built-in math.sqrt>`. El módulo no es un valor (`var m = math;` es error).
- Argumentos con nombre en nativas solo donde está documentado: `print(sep, end, file)`, `io.eprint(sep, end)`, `json.dump(indent)` (solo con nombre: `json.dump(v, 2)` es `TypeError`) y `os.list_dir(p)` (posicional o con nombre). El resto, solo posicionales.
- `exit` es la única excepción a la regla de nombres: `sys.exit` existe, pero en el REPL `from sys import exit` es error (allí ya está `exit()`).
- `IOError`, `FileNotFoundError` y `PermissionError` son globales como los demás errores (se capturan sin `import io`).
- `sys.argv`: la misma tupla que `argv` de `prog main(argc, argv)`. `sys.exit()` = 0, `sys.exit(n)` de 0 a 255, atraviesa los `except` y pasa por los `finally`; en el REPL termina la sesión.
- `sys.recursion_limit` (singular): sin argumento da el límite (1000 por defecto); `recursion_limit(n)` lo cambia (int ≥ 1, `ValueError` si no). La guarda de pila real sigue protegiendo.
- `sys.platform` = `"linux"` (de `uname`, en minúsculas); `sys.version` = `"0.10.0"` (la fase como versión menor hasta la 1.0).
- GC: `gc_set_min` vale 1 MiB por defecto; sin máximo por defecto (`gc_set_max(0)` lo quita); bytes negativos → `ValueError`. `gc_disable()` apaga el GC del todo (también el máximo): es responsabilidad de quien programa; si la memoria se agota el intérprete sale limpio (`fatal_oom`). `gc_collect()` funciona aunque esté desactivado.
- `math.round(x, n)` y `x.round(n)`: float con `n` decimales; sin `n`, int. `gcd`/`lcm`: dos o más ints. `factorial`, `pow`, `lcm` que no caben en 64 bits → `OverflowError`. `is_nan/is_inf/is_finite` con ints: `false/false/true`.
- `io`: `write` acepta cualquier string (literal, variable o expresión) y un valor que no es string es `TypeError` (`f.write(str(5))`); `write_file`, `append_file` y `close` dan `none`. `open` solo con `"r"` (por defecto), `"w"` o `"a"` (otro → `ValueError`); leer de un fichero de escritura o al revés → `IOError`. `read_line()` y `for (linea in f)` conservan el `"\n"`. `f.flush()` (también `io.stdout.flush()`); `print` a stderr e `input()` vacían stdout antes. Un directorio en `read_file`/`open` → `IOError` ("is a directory").
- `random`: generador propio (xoshiro256**), la misma secuencia con `seed(n)` en cualquier máquina; sin semilla, de `getrandom()`. `choice` de algo vacío → `IndexError`; `rand_int(a, b)` con `a > b`, `sample` con `k > len` → `ValueError`; `sample` da una lista; `shuffle` de una tupla → `TypeError`; `choice`/`sample` sobre listas, tuplas, strings y `range` (no sets).
- `time`: `now()` y `clock()` floats en segundos; `sleep` negativo → `ValueError`.
- `os`: `make_dir(p)` un nivel (error si existe); `make_dir(p, "r")` todos los que falten (que ya exista no es error; otro segundo argumento → `ValueError`). `remove_dir` solo vacíos: si no, `IOError: directory not empty: "ruta"`. `remove` de un directorio → `IOError`. `list_dir` da solo nombres, sin `.` ni `..`. `join(a, b, ...)` con cualquier número de partes. Errores: `IOError`, `FileNotFoundError`, `PermissionError` (sin `FileExistsError`).
- `json`: `parse`: objeto → `dict`, array → `list`, `null` → `none`; número sin `.` ni exponente → int (`OverflowError` si no cabe); clave repetida: gana la última. `dump(v)` en una línea al estilo Python (`{"a": 1, "b": [1, 2]}`), `dump(v, indent = 2)` con sangría; no ASCII se queda en UTF-8; una lista que se contiene → `ValueError` ("circular reference").
- **`signal`** (módulo nuevo, paso 7): `handle(sig, f)` (f recibe el número de la señal), `ignore(sig)`, `reset(sig)` (comportamiento por defecto), `send(sig)` (al propio programa). Constantes `SIGINT SIGTERM SIGHUP SIGQUIT SIGUSR1 SIGUSR2 SIGALRM SIGPIPE SIGCHLD` (números de Linux); otra → `ValueError` (`SIGKILL`/`SIGSTOP` no se pueden; `SIGSEGV`/`SIGFPE`/`SIGBUS` no se permiten: romperían la seguridad de memoria). El manejador nunca corre dentro de la señal: se apunta y corre en el siguiente punto seguro (entre sentencias, cada vuelta de bucle); `input()` y `sleep()` se interrumpen para atenderla; si el manejador lanza un error, sale desde donde estaba el programa.
- **`KeyboardInterrupt`**: no es hijo de `Error` (como `SystemExit`): solo lo captura `except KeyboardInterrupt` explícito. Sin capturar, en un fichero: traceback + `KeyboardInterrupt`, código 130; en el REPL, Ctrl+C cancela la línea o lo que corre y vuelve al prompt. Con un manejador de `SIGINT`, Ctrl+C lo llama y no hay `KeyboardInterrupt`.
- Decidido en la implementación (paso 2): con `gc_set_max` y `gc_set_min` a la vez, el máximo manda (es la red de seguridad). `sys.argv` en el REPL es `tuple()`. `recursion_limit(n)` acepta hasta 2³¹−1 y devuelve `none`. `gc_stats()["next_gc"]` sigue informando aunque el GC esté desactivado.
- Decidido en la implementación (paso 3): `round(x, n)` redondea en `long double` para que `2.675` → `2.67` como Python, y los empates exactos (`0.125` → `0.13`) van lejos del cero, como `round()`; `round(x, n)` con `n` negativo vale (`1234.5` → `1200.0`). `x.pow(y)` de floats que da inf → `OverflowError` (como Python). `log(0)` → `ValueError` ("needs a number > 0").
- Decidido en la implementación (paso 4): `io.stdout.close()` (y los otros dos) → `IOError` ("the standard streams cannot be closed"); cerrar dos veces un fichero no es error; `close()` da `none`. Cuando `fopen` falla por falta de descriptores (`EMFILE`), se recolecta (cierra los ficheros inalcanzables) y se reintenta una vez. Abrir un directorio → `IOError: is a directory`. Mensajes: el texto del sistema en minúsculas y la ruta entre comillas (`no such file or directory: "a.txt"`). `io.stdin` en un terminal se puede volver a leer tras Ctrl+D, como `input()`.
- Decidido en la implementación (paso 5): `seed(n)` solo con enteros (negativos también). `rand_int` sin sesgo (rechazo, no módulo simple) y con todo el rango de 64 bits. `sample` con el algoritmo de Floyd (sirve para `range` enormes sin recorrerlos) y el resultado barajado. `sleep(inf)` espera ~31 millones de años; una señal no acorta la espera (el paso 7 la interrumpirá para atender manejadores). `now()`/`clock()` son siempre floats. El generador está comprobado bit a bit contra una implementación de referencia de xoshiro256**.
- Decidido en la implementación (paso 6): `join`, `base_name` y `dir_name` se comportan exactamente como `os.path` de Python (comprobado con los mismos casos). "not a directory" (`ENOTDIR`) es `IOError`, no `FileNotFoundError` (en el paso 4 era lo segundo: corregido). `list_dir` ordena por bytes y acepta `none` como `"."`. `remove` de un enlace a un directorio borra el enlace. El directorio que `list_dir` lee lo guarda el intérprete (`in->dir`) para cerrarlo aunque falte memoria a mitad.
- Decidido en la implementación (paso 7): los puntos seguros son el principio de cada sentencia, de cada bloque (cubre bucles vacíos) y de cada llamada; un cálculo largo dentro de C (un `sort` enorme) no se interrumpe hasta acabar. Los manejadores se instalan sin `SA_RESTART`, y cada espera o lectura que ve `EINTR` atiende la señal y sigue (`input`, `sleep`, `read_line`, `read`, `has_next`, `write`). En el prompt del REPL, Ctrl+C descarta la línea y escribe `KeyboardInterrupt` en stderr; las demás señales que lleguen en el prompt se descartan. `send` atiende la señal antes de volver (determinista en los tests). Un manejador debe ser una función (`TypeError` si no). `KeyboardInterrupt()` existe como constructor (`raise KeyboardInterrupt()`). Arreglado de paso: un error sin mensaje se mostraba `ValueError:  (...)`; ahora `ValueError (...)`.
- Decidido en la implementación (paso 8): `parse` es estricto como el RFC 8259 (sin `NaN`, `Infinity`, comas finales ni comentarios); un float que no cabe (`1e400`) es `OverflowError`, simétrico con `dump`, que no escribe inf. `indent` va de 0 a 64 (`indent = 0`: saltos de línea sin sangría, como Python). Los mensajes de `parse` dan línea y columna (en caracteres). El anidamiento máximo de `parse` y `dump` es el límite de recursión (como Python; antes era agotar la pila, y en modo estrés un test de 200.000 niveles tardaba horas). `dump` no reserva memoria del heap hasta el string final (el GC no puede correr mientras recorre el valor) y detecta los ciclos con una cadena en la pila de C.
- Hito: contador de palabras (lee un `.txt`, frecuencia en minúsculas y sin puntuación, escribe un `.json` con el top 10; usa `io`, `os`, `json` y `sys.argv`).

**Manual `man` (decidido el 2026-11-06; se hace antes de la fase 11)**
- Objetivo: un manual tan detallado que se pueda programar en pycee solo leyéndolo, como el de C. No hay otra documentación. "Mejor que sobre a que falte": una entrada por cada palabra reservada, tipo, método, nativa, módulo, miembro de módulo y tipo de error, más entradas de conceptos (`operators`, `types`, `errors`, `modules`, `comprehensions`, `fstrings`, `memory`, `signals`, `pycee`).
- En inglés (como los mensajes de error). Plantilla al estilo Unix: NAME, SYNOPSIS, DESCRIPTION, ERRORS, EXAMPLES (con su salida real, comprobada por `make test`), SEE ALSO.
- Nombres reales del lenguaje: `man(string)` es el tipo, `man(str)` la función de conversión, `man(lower)` el método. Un método de varios tipos (`man(len)`) da la lista de tipos que lo tienen y remite a `man(list.len)`; los de un solo tipo van directos. Módulos: `man(math)` y `man(math.sqrt)`.
- Operadores: una sola entrada, `man(operators)`, muy detallada. `man(+)` (o cualquier operador) es un error que remite a `man(operators)`.
- Solo en el REPL: en un fichero, `man` es `SyntaxError`; tampoco hay `./pycee --man`. `man()` sin argumento = `man(pycee)` (versión, descripción y el índice de todas las entradas). Una entrada que no existe: `no manual entry for 'x'` con las más parecidas.
- Paginador propio (sin `less`): espacio/`f` página siguiente, `b` anterior, Intro/flecha abajo una línea, flecha arriba atrás, `q` vuelve al REPL; el terminal siempre queda como estaba (también con Ctrl+C). Sin terminal (tubería, tests) escribe el texto entero.
- Almacenamiento: un `.txt` editable por entrada en `man/`; `tools/gen_man.py` los comprime en `src/man/man_data.c` (generado y confirmado, como `unicode_data.c`, para no depender de Python al compilar).
- Después del `man`: los ejercicios, en `exercises/es/` y `exercises/en/`, de menos a más, para gente que ya programa; retos que llevan de la mano y luego sueltan, enseñan los puntos fuertes y los flojos del lenguaje y remiten al `man`. Sin tester.

**Para la fase 12** (decidido aplazar): decoradores `@` para getters/setters de campos `private` (idea del autor, a concretar con él), si `sort` acepta un comparador de dos elementos. (El mensaje "found end of file" de un error dentro de un campo de f-string pasa a la fase 11, paso 0.)

## Decidido en la implementación (el autor lo vio el 2026-11-05; se puede cambiar)
- Parámetros: sin defecto no puede ir tras uno con defecto; `...rest` el último; posicionales antes que con nombre. `var a, b = 1, 2;` vale. El traceback solo aparece si el error cruzó una llamada.
- `const` dentro de una función vive en su bloque (se permite en bucles). En el REPL el eco solo es del nivel superior y un `else` va en la misma línea (`} else {`).
- Un error capturado es un valor nuevo; repr `ValueError("msg")`, str `ValueError: msg`; mensaje de `assert` opcional.
- f-strings: sin `{x=}`, sin `!r`, sin campos anidados en el formato; `f"{1.0:.2}"` da `1` (formato `g`; Python da `1.0`).
- Listas: `is` compara elementos (`l.copy() is l` es `true`; para la identidad, `id()`); `filter/any/all` exigen bool; cambiar el tamaño de la lista dentro de `map/filter/sort...` → `RuntimeError`; `map`/`filter` sobre una tupla dan tupla; `reduce` sin inicial empieza por el primero; `min/max/reduce` de algo vacío → `ValueError`; `sum` solo números; `insert` fuera de rango inserta en el extremo; `[[...]]` para una lista que se contiene.
- Sets y dicts: también devuelven el propio valor al modificarse (`add remove clear`); los sets van en orden de inserción; `1` y `1.0` son la misma clave; `d.get(k)` sin defecto da `none`; un `nan` como clave no se encuentra nunca.
- Objetos: un `private` es solo del `obj` que lo declara y solo por `self`; un hijo puede redefinir un método con otra aridad; `type`/`is_type` no se pueden redefinir, `id` sí (es un nombre de campo habitual); sin `create` no acepta argumentos (si extiende un error, un mensaje opcional); `<Point>`, `<obj Point>`, `<method Point.move>`; `to_str` se usa también en listas y en el REPL; `compare` funciona con el objeto a la derecha; `to_str` que no da string o `compare` que no da número → `TypeError`.
- En el REPL, `l.append(2)` muestra la lista (ya no devuelve `none`).

## Lecciones (para no repetir errores)
- **torturette encuentra lo que valgrind no ve**: los locales de la libc (`setlocale`/`newlocale`) dejan memoria sin liberar para siempre y valgrind lo oculta con `__libc_freeres`. Por eso las tablas Unicode son propias (`unicode_data.c`, comprobadas idénticas a Python en los 1.112.064 códigos).
- Nunca pasar como parámetro de salida un puntero a la pila de valores: puede crecer (realloc) mientras se usa. Usar una variable local y copiar después.
- La limpieza de la sesión se registra después de inicializar todo lo que no reserva (un fallo de `malloc` temprano liberaba basura).
- Un `for` comprueba también al terminar si la lista cambió de tamaño.
- El binario release (-O2) puede dar avisos que el debug no da (`maybe-uninitialized`): compilar los dos.
- gcc 15 trae funciones del próximo estándar de C como *built-ins* (`uabs`...): una función `static` con ese nombre y otro tipo no compila. `math.c` usaba `uabs` (renombrada a `abs_u64` el 2026-11-06, en una máquina con gcc 15.2). Evitar nombres cortos tipo libc en funciones propias.
- **torturette repite el programa una vez por cada `malloc`**: un test con 100.000 reservas tarda más de media hora (y parece colgado: el proceso duerme esperando al hijo). Los casos de `run_torturette.sh` deben reservar poco (`sys_light.pce` en vez de `sys_module.pce`). Y nunca lanzar dos torturette a la vez: comparten `build/torture/pycee`.
- No editar código mientras corre una verificación en segundo plano: se mezclan resultados de dos versiones.
- **En `make stress` el GC recolecta en cada reserva y recorre todo lo vivo**: un test que crea N objetos vivos cuesta ~N². Los tests de "muy profundo" o "muy grande" deben llegar al límite con pocos objetos (`json_deep.pce` usa el límite de recursión, no 200.000 niveles).
- `pkill -f patrón` también mata la propia orden que lo contiene: matar por pid.

## Medidas
- `tests/big.pce` (3 MB): lexer 0,09 s, parser 0,22 s (44 MB), resolver 0,09 s con la tabla hash (1,31 s con búsqueda lineal).
- Cálculo: tree-walking ≈ 2–5× más lento que un bytecode tipo CPython; las llamadas a funciones, ~3,7× más lentas que en Python (objetivo de la fase 11).
