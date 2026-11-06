# Ejercicio 03: primos

| | |
|---|---|
| Fichero a entregar | `primos.pce` |
| Se usa | listas de tamaño fijo (`var l[n]`), `while`, `range` con paso, comprensiones con `if`, `enumerate()`, f-strings con ancho, `join` |
| En el manual | `man(var)`, `man(while)`, `man(range)`, `man(comprehensions)`, `man(enumerate)`, `man(fstrings)`, `man(join)` |

La criba de Eratóstenes, que es una excusa para trabajar con listas y con el formato de salida.

## Enunciado

```
./pycee primos.pce N
```

Escribe todos los primos hasta `N` (incluido), y después un resumen.

### Reglas

1. **Los primos** van en filas de 10, cada número alineado a la derecha en 5 caracteres. La última fila puede tener menos.
2. **El resumen**, en este orden:
   - `total: K`: cuántos primos hay.
   - `mayor: P`: el mayor de ellos.
   - `gemelos: G -> (p, q) (p, q) ...`: los pares de primos seguidos que se diferencian en 2, en orden. Si no hay ninguno, solo `gemelos: 0`.
3. **Sin primos** (si `N` es menor que 2, también negativo) se escribe `ningún primo hasta N` y el código de salida es **0**.
4. **Argumentos.** Con un número de argumentos distinto de 1 se escribe `uso: primos.pce N` (código **2**). Si el argumento no es un entero, `error: 'X' no es un entero` (código **1**).

## Ejemplos

```
$ ./pycee primos.pce 50
    2    3    5    7   11   13   17   19   23   29
   31   37   41   43   47
total: 15
mayor: 47
gemelos: 6 -> (3, 5) (5, 7) (11, 13) (17, 19) (29, 31) (41, 43)
```

```
$ ./pycee primos.pce 2
    2
total: 1
mayor: 2
gemelos: 0
$ ./pycee primos.pce 1
ningún primo hasta 1
$ ./pycee primos.pce siete; echo "exit=$?"
error: 'siete' no es un entero
exit=1
```

Hasta 1000 hay 168 primos; el último es 997 y hay 35 pares de gemelos.

## Pistas (léelas solo si te atascas)

<details>
<summary>La criba</summary>

- `var is_prime[n + 1];` crea una lista de `n + 1` elementos, todos `none`. Rellénala con `is_prime[i] = i >= 2;`.
- Para cada `i` desde 2 mientras `i * i <= n`: si `is_prime[i]`, marca como `false` sus múltiplos desde `i * i`. `range(i * i, n + 1, i)` los da todos.
- Al final, una comprensión con `if` saca los primos: `[k for (k in range(n + 1)) if (is_prime[k])]`.

</details>

<details>
<summary>Filas de 10</summary>

- `f"{p:>5}"` alinea a la derecha en 5 caracteres.
- `for (i, p in primes.enumerate())` da a la vez la posición y el valor. La fila se acaba cuando `i % 10 is 9`, o en el último primo.
- Ve juntando la fila en un string (`line += f"{p:>5}";`) y escríbela cuando se acabe.

</details>

<details>
<summary>Los gemelos</summary>

- Recorre las posiciones `i` de 0 a `len - 2` y compara `primes[i + 1] - primes[i] is 2`.
- Puedes construir directamente los textos `"(3, 5)"` en una comprensión y unirlos con `" ".join(...)`.

</details>

## Para ir más allá (opcional)

- Mide cuánto tarda con `N` = 1 000 000 (`time ./pycee-release primos.pce 1000000 > /dev/null`). ¿Cuánta memoria usa? Prueba `import sys` y `sys.memory()` (`man(sys)`).
- Escribe solo los primos que son palíndromos (`man(string.reverse)`).
