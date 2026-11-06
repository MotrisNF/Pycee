# Hoja de ruta: pycee, lenguaje interpretado en C

**Decisiones de partida:** el intérprete recorre el AST directamente (tree-walking), el tipado es dinámico, la memoria se gestiona con un GC de marcado y barrido (mark & sweep) y se puede usar la libc. El diseño del lenguaje está en [EBNF.md](EBNF.md).

**Errores:** al estilo Python (tipo con nombre, línea de código y marca `^`; traceback en ejecución). **El primer error aborta**: no hay recuperación de errores en ninguna fase. En el REPL, un error vuelve al prompt.

Cada fase termina con un **hito** ejecutable. No se pasa a la siguiente fase hasta que el hito funcione.

**Estado (2026-11-06):** fases 0 a 10 terminadas; manual `man` y ejercicios (`exercises/`, 12 en castellano y en inglés) hechos; **siguiente: la fase 11**. Para retomar: [SESSION.md](SESSION.md), sección "Cómo retomar".

---

## Fase 0: Diseño en papel (sin código) ✅
- [x] Escribir 5–10 programas de ejemplo en el lenguaje, tal como deberían verse ([examples/](examples/)).
- [x] A partir de ellos, definir la gramática en EBNF: expresiones, sentencias y precedencia de operadores.
- [x] Decidir qué es una sentencia y qué es una expresión, cómo se delimitan los bloques (`{}`) y si hay `;` (sí, siempre).
- **Hito:** un documento con la gramática y los programas de ejemplo.

## Fase 1: Infraestructura ✅
- [x] Estructura de carpetas, Makefile, y flags `-Wall -Wextra -Werror -g` más un perfil con `-fsanitize=address,undefined` (y un perfil release con `-O2`).
- [x] Leer un fichero fuente entero a memoria y montar un REPL básico.
- [x] Un sistema de errores al estilo Python: tipo, mensaje, fichero, línea y la línea de código con la marca `^`.
- **Hito:** `./pycee fichero.pce` lee el fichero, y `./pycee` sin argumentos abre un REPL que devuelve lo que se escribe.

## Fase 2: Lexer ✅
- [x] Convertir el texto en tokens: identificadores, palabras clave, números, strings con escapes y operadores. Descartar los comentarios (`#` de línea y `/* */` de bloque).
- [x] Cada token guarda su posición como `offset` y `len` dentro del buffer, igual que `t_error`.
- [x] Errores léxicos como `SyntaxError`: carácter no válido, string o comentario de bloque sin cerrar.
- **Hito:** un modo `--tokens` que imprime la lista de tokens.

## Fase 3: Parser y AST ✅
- [x] Posiciones globales: una tabla de ficheros cargados, cada uno con su rango de offsets (prerrequisito de los imports).
- [x] Parser de descenso recursivo, con *Pratt parsing* para las expresiones y su precedencia.
- [x] Representar los nodos del AST y decidir cómo se asigna su memoria (una arena).
- [x] Al primer error de sintaxis se para y se informa con `SyntaxError`.
- [x] Analizar `import` / `from ... import` (tokens, `NODE_IMPORT`).
- **Hito:** un modo `--ast` que imprime el árbol.

## Fase 4: Resolver ✅
- [x] Cargar los módulos importados (una vez cada uno, error en imports circulares) y resolver sus nombres (`m.f`, `from m import f`).
- [x] Recorrer el AST antes de ejecutar y comprobar las reglas que la gramática no expresa (ver [EBNF.md](EBNF.md)): nombres no declarados o repetidos, `var` dentro de bucles, `vemp` fuera de funciones, `break`/`continue` fuera de bucles...
- [x] Registrar las declaraciones de nivel superior (`func`, `prog`, `obj`, `var`) para poder llamar a funciones declaradas más abajo.
- [x] Anotar cada uso de variable con su (ámbito, índice), para que el intérprete no tenga que buscar por nombre (decidido hacerlo ya: es lo que evita un intérprete desproporcionadamente lento).
- [x] Tabla hash para las globales (con búsqueda lineal, `big.pce` tardaba 1,3 s en resolverse).
- **Hito:** los ejemplos válidos pasan sin errores y cada regla tiene un programa de prueba que la viola con su `NameError` o `SyntaxError`.

## Fase 5: Evaluador de expresiones ✅
- [x] **Primero:** guarda de pila real en parser, resolver y `ast_print` (hoy una anidación muy profunda da *segfault*): `RecursionError: stack exhausted` / `SyntaxError` en lugar de romperse. El intérprete la usará también.
- [x] Lexer: palabras reservadas nuevas y `@`; literales numéricos (`1_000`, `0xFF`, `0b1`, `0o7`, `1e9`); tokens `?` `:` `&` `|` `^` `~` `<<` `>>`.
- [x] Números: int de 64 bits (con `OverflowError`) y float; `/` siempre float; `//` y `%` truncados (también con floats); operadores de bits; ternario `c ? a : b`.
- [x] REPL: entrada propia del parser (declaraciones y sentencias sueltas, `;` final opcional, continuación `... `), eco del valor salvo `none`.
- [x] Representar los valores en tiempo de ejecución como una *tagged union* (`none`, `bool`, `number` y `obj*`).
- [x] Despacho de métodos por tipo: cada tipo tiene una tabla de métodos (nativos en C o `FUNC` de un `obj`). Todo valor tiene métodos (ver SESSION.md).
- [x] Aritmética, comparación (`is`, `is not`, `<`...), lógica con cortocircuito (`and`, `or`, `not`, `xor`) y errores de ejecución (`TypeError`, `ZeroDivisionError`, `ValueError`).
- [x] Métodos comunes `type()` e `is_type()`; `int()`, `float()`, `num()`, `str()`.
- **Hito:** una calculadora en el REPL.

## Fase 6: Variables, sentencias y control de flujo ✅
- [x] Marcos en la pila de valores: el cuerpo del `prog` (sin parámetros) y el nivel superior de cada línea del REPL. `var` vive en toda la función y `vemp` solo en su bloque.
- [x] Asignación (y `+=`, `-=`...), bloques, `if`/`elif`/`else`, `while`, `for (x in range(...))` con su `else`, `switch`, `break` y `continue`.
- [x] `range` como en Python (tipo propio, no guarda los números).
- [x] `const`, `do { } while (c);`, `break outer;` / `continue outer;` (el bucle que envuelve al actual), `a, b = b, a;`.
- [x] REPL: control de flujo en el nivel superior y `exit()` / `exit(n)` (solo en el REPL; en ficheros, `sys.exit` en la fase 10). `man` reservada.
- Pasan a la fase 9 (necesitan tuplas o colecciones): desempaquetar un valor (`var a, b = t;`, `a, b = t;`), recorrer strings con `for` y el `RuntimeError` al cambiar una colección que un `for` recorre.
- **Hito:** un FizzBuzz escrito en el lenguaje (`tests/run/fizzbuzz.pce`).

## Fase 7: Funciones ✅
- [x] (Adelantado de la fase 9) tuplas básicas: `tuple(...)`, `t[i]`/`t[-1]` (`IndexError`), `t.len()`, `for`, desempaquetar (`AssignmentError` si no cuadra), igualdad, `in`/`not in` (también sobre `range`).
- [x] Declaración, llamada, aridad (al analizar cuando se sabe qué se llama; si no, al ejecutar, con el mismo mensaje) y recursión.
- [x] Funciones como valores: lambdas (`func (x) {...}`, `(x) => expr`, `(x) => {...}`), closures que capturan la variable (en celdas solo las capturadas), funciones con nombre dentro de otras, métodos ligados de los tipos que ya existen. Se muestran como `<func doble>`.
- [x] Parámetros por defecto (evaluados en cada llamada, pueden usar los anteriores), argumentos con nombre, `...rest` (tupla) y `f(...t)`.
- [x] `prog main()` / `prog main(argc, argv)`; su `return` es el código de salida.
- [x] `return`, `break` y `continue` suben con un código de estado (`t_exec`), sin `longjmp`.
- [x] Traceback como Python (16 marcos internos + 16 externos, repeticiones resumidas). `RecursionError` a las 1000 llamadas, más la guarda de pila real.
- **Hito:** fibonacci recursivo y el traceback de un error en funciones anidadas (`tests/run/functions.pce`, `tests/run/err_traceback.pce`).
- Medido: las llamadas son ~3,7× más lentas que en Python 3.14 (fib(30): 0,26 s frente a 0,07 s). Primer objetivo de la fase 11.
- Pasa a la fase 9: `f(...l)` con listas, `self` dentro de lambdas de un método, métodos de `obj` ligados, `obj` como valor.

## Fase 7b: Errores capturables ✅
- [x] Errores como valores (`e.type()`, `e.message()`, `e.line()`, `e.file()`, `e.traceback()`, `print(e)`), con un constructor por tipo (`ValueError("x")`) y **`Error` como tipo base** de todos (jerarquía en `error_parent`, lista para la herencia de `obj`).
- [x] `try` / `except Tipo, Tipo (e)` / `finally`, `raise e`, `assert(c[, msg])`. `exit()` atraviesa los `except` (pasa por los `finally`). `return`/`break`/`continue` no pueden salir de un `finally`.
- [x] Errores encadenados como Python ("During handling of the above error...").
- [x] `input()` / `input(prompt)` con `EOFError` al final.
- **Hito:** `tests/run/read_numbers.pce`: lee números con `num(input())` y se recupera de los que no lo son.

## Fase 8: Recolector de basura ✅
- [x] La memoria del AST (arena, vive todo el programa) y la de los valores (heap, la gestiona el GC) ya estaban separadas.
- [x] Marcado y barrido sin mover objetos; marcado iterativo (pila de grises en el heap, ninguna anidación agota la pila de C). Raíces: pila de valores, globales de todos los módulos, `in->ret`; la cadena de contextos de error se marca desde cada `t_error_value` y se mantiene como raíz donde un error viaja (finally, except).
- [x] Ritmo: una recolección cuando la memoria viva supera el doble de lo que sobrevivió a la anterior (mínimo 1 MiB).
- [x] Modo estrés `PYCEE_GC_STRESS=1` (recolecta en cada reserva) y `make stress`. Comprobado que caza una raíz olvidada (fallo inyectado a propósito: ASan da use-after-free en el acto).
- **Hito:** 5 millones de vueltas (~25 millones de objetos) con la memoria plana en 3,6 MB (`tests/run/gc_churn.pce` es una versión corta).

## Fase 9: Tipos compuestos y objetos ✅
- [x] Strings Unicode (UTF-8 por caracteres, tablas Unicode propias, inválido → U+FFFD), escapes nuevos, `"""..."""`, `r"..."`, f-strings con formato de Python, métodos.
- [x] Cortes `[a:b:c]`, `in` / `not in`, listas (todos sus métodos, `sort(key, reverse)` estable, `map`/`filter`/`reduce`/`any`/`all`/`each`, `sum`/`min`/`max`, `enumerate`), `for (k, v in ...)`, `RuntimeError` si una colección cambia de tamaño mientras se recorre.
- [x] Sets y dicts (tabla hash propia, orden de inserción, claves inmutables), `dict("a": 1)`.
- [x] Comprensiones en `[...]`, `tuple(...)`, `set(...)`, `dict(...)`, `any/all/sum/min/max(...)`, con varios `for` e `if` como Python.
- [x] Revisión del autor (2026-11-05): los métodos que cambian una lista/set/dict la devuelven (encadenar); `extend`; `any()`/`all()` sin función; `any/all/sum/min/max` globales; `x.id()`.
- [x] Objetos: campos declarados en `create` (`self.x = ...`) o con `var`, métodos, `self` (también en lambdas), herencia (`extends` / `(P)`), `super`, `private`, `static`, métodos ligados, el `obj` como valor; métodos especiales `to_str`, `compare`, `get`/`set`, `iter`/`has_next`/`next`; errores propios (`obj E extends ValueError`).
- **Hito:** listas y objetos de verdad: `tests/run/obj_compare.pce` ordena una lista de objetos con su `compare`.

## Fase 10: Librería estándar nativa ✅
API completa decidida por el autor el 2026-11-05: [SESSION.md](SESSION.md), "Fase 10 (API decidida)". Regla: ningún miembro de un módulo del sistema se llama como una palabra reservada o una nativa global, para poder importarlo suelto.
- [x] Antes de empezar: `from m import a as b`; `input()` vuelve a esperar en un terminal tras Ctrl+D.
- [x] **Paso 1.** Módulos nativos: el resolver conoce `sys`, `math`, `io`, `random`, `time`, `os`, `signal` y `json` (funciones con su aridad y constantes); `import m`, `import m as x`, `from m import a as b`; `ImportError` si un `m.pce` del usuario se llama como uno del sistema; test de que ningún miembro choca con un nombre reservado; argumentos con nombre en las nativas.
- [x] **Paso 2.** `sys`: `argv`, `exit`, `recursion_limit`, `platform`, `version`, `gc_collect`, `memory`, `gc_stats`, `gc_enable`, `gc_disable`, `gc_set_min`, `gc_set_max`.
- [x] **Paso 3.** `math` (y los métodos de número `trunc()` y `round(digits)`).
- [x] **Paso 4.** `io`: funciones directas, tipo `file` (se cierra al liberarlo el GC), `IOError` / `FileNotFoundError` / `PermissionError`, `stdin/stdout/stderr`, `print(..., sep, end, file)` y `eprint`.
- [x] **Paso 5.** `time` y `random`.
- [x] **Paso 6.** `os`.
- [x] **Paso 7.** `signal` (manejadores de señales, una constante por señal) y `KeyboardInterrupt` capturable.
- [x] **Paso 8.** `json`.
- **Hito (paso 9):** `examples/example8.pce`: lee un `.txt`, cuenta las palabras y escribe un `.json` con las 10 más frecuentes (comprobado contra Python).

## Fase 11: Eficiencia sin salir del AST
Estructura propuesta el 2026-11-06 (las preguntas abiertas están en SESSION.md, "Fase 11: por decidir"). Regla de toda la fase: **ninguna optimización cambia lo que hace un programa** (mismas salidas y mismos mensajes de error), y cada paso pasa `make test`, `make stress`, valgrind y torturette. Lo que no gane tiempo medible se deshace.
- [ ] **Paso 0. Mensajes de las f-strings.** Un error dentro de un campo dice "found end of file" en vez de "found '}'" (`f"{1 +}"`); y `f"{(1}"` dice "unterminated f-string" cuando lo que falta es cerrar el `(`.
- [ ] **Paso 1. Medir antes de tocar.** Una batería de benchmarks propia (`bench/`), cada uno con su equivalente en Python y un script que da los tiempos de los dos: llamadas (fib recursivo), bucles y aritmética, strings y f-strings, listas (comprensiones, `sort`), dicts (contador de palabras), objetos y métodos, closures y basura para el GC. Perfiles con `valgrind --tool=callgrind` (y `perf` donde se pueda). Tabla "antes" en `docs/BENCH.md`.
- [ ] **Paso 2. Llamadas** (hoy ~3,7× más lentas que Python): lo que diga el perfil; candidatos: un camino rápido para llamadas solo con posicionales y aridad ya comprobada por el resolver, preparar el marco sin trabajo repetido, no crear valores de función para llamadas directas.
- [ ] **Paso 3. Métodos y nombres.** Hoy cada `x.m()` busca `m` comparando strings en la tabla de su tipo. Internar los nombres (el resolver da a cada nombre de método un número) y una caché por sitio de llamada (*inline cache*: tipo visto → método), que sigue siendo AST.
- [ ] **Paso 4. Strings.** Guardar el hash en cada string (dicts y sets no lo recalculan); internar los nombres y los literales cortos; ver si `s += x` en un bucle merece un camino especial.
- [ ] **Paso 5. Memoria y GC.** Ajustar los umbrales (mínimo, factor de crecimiento) con los benchmarks; y, si el perfil lo pide, un asignador propio para objetos pequeños (listas libres por tamaño) en vez de un `malloc` por objeto.
- [ ] **Paso 6 (opcional). Representación de los valores.** NaN-boxing (`t_value` de 16 a 8 bytes): solo si los pasos anteriores lo justifican; cambia todo el intérprete.
- **Hito:** `docs/BENCH.md` con la tabla antes/después de cada benchmark, contra Python, y el objetivo que se fije.

## Fase 12: Calidad
- [ ] Una batería de tests: programas con su salida esperada y un script que los compara.
- [ ] Revisar los mensajes de error y comprobar que todos los tests pasan sin fugas en valgrind.
- [ ] Decoradores con `@` para getters y setters de campos `private` (idea del autor; la cuenta él cuando toque). Ejemplo orientativo: `@get func edad()` / `@set func edad(v)`, usados como `p.edad` y `p.edad = 5`.
- [ ] Decidir si `sort` acepta también un comparador de dos elementos (hoy solo `key`).

---

Las fases 0 a 7 son el núcleo del proyecto. Lo más difícil está en las fases 7 (funciones y traceback) y 8 (GC).

**Manual en el REPL (`man`): hecho el 2026-11-06.** 314 temas en inglés, al estilo del `man` de Unix, con todos sus ejemplos comprobados; `man()` es la portada con el índice, `man(len)` lista los tipos que lo tienen, solo en el REPL, paginador propio. Detalles en SESSION.md, "Manual `man`".

**Al terminar el lenguaje: tutorial con ejercicios al estilo 42** (pedido por el autor, 2026-11-05). Las decisiones las ha tomado el autor, pero el código no lo ha escrito él, así que necesita aprender a usar su lenguaje *usándolo*: una serie de ejercicios progresivos (enunciado, qué se pide, ejemplos de entrada y salida, un corrector que lo comprueba), desde `print` y variables hasta closures, objetos, módulos y errores. Cada ejercicio introduce una pieza nueva y obliga a usarla.

**Editor (2026-11-06):** hecha la extensión de VS Code (`editors/vscode/`: resaltado y error de `--check` subrayado). Después de la fase 12, junto al `man`: un *language server* con autocompletado, ir a la definición y documentación al pasar el ratón, que reutilice el resolver (y el contenido del `man`).

**Después de la fase 12** (decidido, con sus palabras ya reservadas): `enum`, `match`, `with` (`with (io.open("f") as f) { }`, atajo de `try`/`finally` que llama a `close()`), `export` y `async`/`await`. La herencia (`extends`, `super`) se adelantó a la fase 9 y los decoradores (`@`) pasan a la fase 12.
