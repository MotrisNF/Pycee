# Ejercicio 00: notas

| | |
|---|---|
| Fichero a entregar | `notas.pce` |
| Se usa | `input()`, `try`/`except`, strings (`split`, `strip`), `num()`, listas, tuplas, funciones, f-strings con formato, `sort(key, reverse)`, comprensiones |
| No se usa | `import` (ningún módulo) |

## Enunciado

Escribe un programa que lea por la entrada estándar una lista de alumnos con su nota, una por línea, y al final muestre un resumen.

Cada línea válida tiene **exactamente dos palabras**: el nombre y la nota.

```
ana 7.5
```

Una nota es un número (entero o decimal) **entre 0 y 10, ambos incluidos**.

El programa lee líneas con `input()` hasta que se acaba la entrada. En ese momento `input()` lanza un `EOFError`.

### Reglas

1. **Líneas vacías.** Una línea vacía o solo con espacios se salta sin decir nada.
2. **Líneas no válidas.** Una línea que no es válida se salta y se avisa con `línea N ignorada: "texto"`. Una línea no es válida si:
   - no tiene dos palabras;
   - la nota no es un número;
   - la nota está fuera de 0..10.

   `N` es el número de la línea (la primera es la 1, y las vacías también cuentan) y `texto` es la línea tal cual se leyó.
3. **Sin ninguna línea válida.** Se muestra `no hay notas` y el programa termina con código de salida **1**.
4. **Con alguna línea válida.** Se muestra, en este orden:
   - `alumnos: X`: cuántos alumnos válidos hay.
   - `media: M`: la media de sus notas, **con dos decimales**.
   - `mejor: nombre (nota)`: el alumno con la nota más alta. Si hay empate, el que apareció **primero**.
   - `aprobados: a, b, c`: los nombres con nota ≥ 5, separados por `", "` y en el orden en que aparecieron.
   - El **ranking**: todos los alumnos de mayor a menor nota (en caso de empate, el orden de la entrada), con el formato `posición. nombre nota`. El nombre va alineado a la izquierda en 8 caracteres y la nota a la derecha en 5.

   El programa termina con código **0**.

La nota se muestra tal como la da el lenguaje: `7` si era entera y `7.5` si tenía decimales.

## Ejemplos

Con la entrada en un fichero: `./pycee notas.pce < entrada.txt`.

**Ejemplo 1.** Esta es la entrada (la línea 5 está vacía):

```
ana 7.5
luis 5
pepe
marta 9.5

juan 3.25
sara once
leo 11
bea 9.5
```

Esta es la salida:

```
línea 3 ignorada: "pepe"
línea 7 ignorada: "sara once"
línea 8 ignorada: "leo 11"
alumnos: 5
media: 6.95
mejor: marta (9.5)
aprobados: ana, luis, marta, bea
1. marta     9.5
2. bea       9.5
3. ana       7.5
4. luis        5
5. juan     3.25
```

El código de salida es 0. Marta y Bea empatan, pero Marta va primero porque apareció antes.

**Ejemplo 2.** Esta es la entrada (la línea 1 está vacía):

```

hola
```

Esta es la salida:

```
línea 2 ignorada: "hola"
no hay notas
```

El código de salida es 1.

**Ejemplo 3.** Esta es la entrada:

```
ana 7
luis 4.5
```

Esta es la salida:

```
alumnos: 2
media: 5.75
mejor: ana (7)
aprobados: ana
1. ana         7
2. luis      4.5
```

El código de salida es 0.

## Cómo comprobarlo

```
printf 'ana 7.5\nluis 5\npepe\nmarta 9.5\n\njuan 3.25\nsara once\nleo 11\nbea 9.5\n' > entrada.txt
./pycee notas.pce < entrada.txt; echo "exit=$?"
```

La salida debe coincidir carácter a carácter con la de los ejemplos.

## Pistas (léelas solo si te atascas)

<details>
<summary>Estructura del programa</summary>

- Una función `parse(line)` que devuelva `tuple(nombre, nota)` si la línea es válida, o `none` si no lo es.
- En `prog main()`, un `while (true)` que lea con `input()` dentro de un `try`, y un `except EOFError { break; }` para salir.
- El código de salida es lo que devuelve `prog main()` con `return`.

</details>

<details>
<summary>Reglas del lenguaje que te van a saltar</summary>

- **`var` no se permite dentro de un bucle.** Declara antes del bucle o usa `vemp`, que vive solo en su bloque: `vemp line = input();`.
- **Las condiciones exigen un bool.** `if (s)` con una tupla da error, y hay que escribir `if (s is none)`.
- **No hay `==`.** Se compara con `is` y `is not`.
- **`num("once")` lanza `ValueError`.** Captúralo con `except ValueError { ... }`.
- **En una comprensión, para desempaquetar se escribe `for (nombre, nota in alumnos)`,** sin paréntesis extra alrededor de `nombre, nota`.

</details>

<details>
<summary>Formato y orden</summary>

- `f"{x:.2f}"` da dos decimales. `f"{nombre:<8}"` alinea a la izquierda en 8 caracteres y `f"{nota:>5}"` alinea a la derecha en 5.
- `l.copy().sort(key = (s) => s[1], reverse = true)` da una copia ordenada de mayor a menor sin tocar `l`. `sort` es **estable**: con notas iguales se conserva el orden de entrada.
- `l.enumerate()` da `tuple(índice, elemento)`, empezando en 0.
- `", ".join(lista)` une strings.
- Para escribir comillas dentro de un string: `"\""`.

</details>

## Para ir más allá (opcional)

- Muestra también la nota más baja y la mediana.
- Si un nombre se repite, quédate con su última nota y avisa con `nombre repetido: X`.
