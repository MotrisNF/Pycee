# Ejercicio 05: matrices

| | |
|---|---|
| Fichero a entregar | `matrices.pce` |
| Se usa | listas de listas, comprensiones anidadas, `map`, `filter`, `sum`/`min`/`max`/`all` con una comprensión, `raise`, el ternario |
| En el manual | `man(list)`, `man(comprehensions)`, `man(list.map)`, `man(list.filter)`, `man(sum)`, `man(all)`, `man(raise)` |

Aquí el lenguaje empieza a lucirse: con comprensiones y las funciones de lista casi no hacen falta bucles. Y aparece otra carencia: no hay `zip`.

## Enunciado

```
./pycee matrices.pce < matriz.txt
```

Lee una matriz de enteros de la entrada estándar (una fila por línea, los números separados por espacios; las líneas vacías se saltan) y escribe:

```
filas: F, columnas: C
traspuesta:
<la traspuesta, cada número alineado a la derecha en 4 caracteres>
suma de filas: [...]
suma de columnas: [...]
plana: [todos los números, fila a fila]
pares: [los números pares de la plana, en el mismo orden]
máximo: X mínimo: Y
diagonales: D1 D2
cuadrado mágico: sí
```

### Reglas

1. **Diagonales y cuadrado mágico** solo tienen sentido si la matriz es cuadrada. Si no lo es, no se escribe la línea `diagonales` y la última línea es `cuadrado mágico: no (no es cuadrada)`.
2. **`D1`** es la suma de la diagonal principal (arriba a la izquierda → abajo a la derecha) y **`D2`** la de la otra.
3. **Es un cuadrado mágico** si todas las filas, todas las columnas y las dos diagonales suman lo mismo. Se escribe `cuadrado mágico: sí` o `cuadrado mágico: no`.
4. **Errores** (código de salida **1**):
   - un elemento que no es un entero: `error: 'X' en la línea N no es un entero`;
   - una fila con distinto número de elementos que la primera: `error: la línea N tiene A números, se esperaban B`;
   - ninguna fila: `matriz vacía`.

   `N` cuenta todas las líneas de la entrada, también las vacías.

## Ejemplos

Entrada (la línea 3 está vacía):

```
2 7 6
9 5 1

4 3 8
```

Salida:

```
filas: 3, columnas: 3
traspuesta:
   2   9   4
   7   5   3
   6   1   8
suma de filas: [15, 15, 15]
suma de columnas: [15, 15, 15]
plana: [2, 7, 6, 9, 5, 1, 4, 3, 8]
pares: [2, 6, 4, 8]
máximo: 9 mínimo: 1
diagonales: 15 15
cuadrado mágico: sí
```

Entrada `1 2 3` / `4 5 6`:

```
filas: 2, columnas: 3
traspuesta:
   1   4
   2   5
   3   6
suma de filas: [6, 15]
suma de columnas: [5, 7, 9]
plana: [1, 2, 3, 4, 5, 6]
pares: [2, 4, 6]
máximo: 6 mínimo: 1
cuadrado mágico: no (no es cuadrada)
```

Entrada `1 2` / `3 4` (cuadrada, pero no mágica):

```
filas: 2, columnas: 2
traspuesta:
   1   3
   2   4
suma de filas: [3, 7]
suma de columnas: [4, 6]
plana: [1, 2, 3, 4]
pares: [2, 4]
máximo: 4 mínimo: 1
diagonales: 5 5
cuadrado mágico: no
```

Errores:

```
$ printf '1 2\n3 x\n' | ./pycee matrices.pce; echo "exit=$?"
error: 'x' en la línea 2 no es un entero
exit=1
$ printf '1 2\n3 4 5\n' | ./pycee matrices.pce; echo "exit=$?"
error: la línea 2 tiene 3 números, se esperaban 2
exit=1
```

## Pistas (léelas solo si te atascas)

<details>
<summary>Leer y validar</summary>

- `line.split()` sin argumento separa por cualquier cantidad de espacios y descarta los huecos; una línea vacía da `[]`.
- Una función `read_matrix()` que lance un `ValueError` con el mensaje (`raise ValueError(f"...")`) y un único `try`/`except ValueError (e)` en `main` que escriba `e.message()` mantiene el código ordenado.

</details>

<details>
<summary>Sin zip</summary>

- No hay `zip`: la traspuesta se hace con índices, en una comprensión anidada: `[[m[i][j] for (i in range(rows))] for (j in range(cols))]`.
- La lista plana, con dos `for` en la misma comprensión: `[x for (row in m) for (x in row)]`.

</details>

<details>
<summary>Sumas y comprobaciones</summary>

- `m.map((r) => r.sum())` suma cada fila.
- `sum(...)`, `max(...)`, `min(...)` y `all(...)` admiten una comprensión dentro, sin corchetes: `sum(m[i][i] for (i in range(rows)))`. `all` se para en cuanto encuentra un `false`.
- `all(s is target for (s in sums))` dice si todas son iguales.

</details>

## Para ir más allá (opcional)

- Multiplica la matriz por su traspuesta.
- Un cuadrado mágico "de verdad" además usa cada número de 1 a n² exactamente una vez: compruébalo con un `set` (`man(set)`).
