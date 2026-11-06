# Ejercicio 02: fizzbuzz

| | |
|---|---|
| Fichero a entregar | `fizzbuzz.pce` |
| Se usa | `var` y `vemp`, `for` sobre `range`, `if`/`elif`/`else`, `%`, `++`, `int()`, `try`/`except ValueError`, funciones |
| En el manual | `man(var)`, `man(vemp)`, `man(range)`, `man(if)`, `man(int)`, `man(try)`, `man(func)` |

El clásico, con argumentos opcionales. Sirve para chocar con las dos reglas que más sorprenden al llegar a pycee: `var` no se puede declarar dentro de un bucle, y las condiciones solo aceptan `true` o `false`.

## Enunciado

```
./pycee fizzbuzz.pce N [a b]
```

Escribe los números del 1 al `N`, uno por línea, cambiando:

- los múltiplos de `a` y de `b` a la vez por `FizzBuzz`;
- los múltiplos solo de `a` por `Fizz`;
- los múltiplos solo de `b` por `Buzz`.

Si no se dan, `a` es 3 y `b` es 5. Al final se escribe `FizzBuzz: K`, donde `K` es cuántas veces salió `FizzBuzz`.

### Reglas

1. **Número de argumentos.** Se aceptan 1 (`N`) o 3 (`N a b`); con cualquier otro número, `uso: fizzbuzz.pce N [a b]` y código de salida **2**.

   > Cuidado: también hay que rechazar 2 argumentos (`N a` sin `b`). El ejemplo de abajo no lo muestra: piénsalo tú.

2. **Cada argumento** debe ser un entero mayor que 0. Si alguno no lo es, se escribe `error: 'X' no es un entero positivo` (con `X` tal como se escribió) y el código de salida es **1**. Se comprueban en orden, y se avisa solo del primero que falle.
3. Si todo va bien, el código de salida es **0**.

## Ejemplos

```
$ ./pycee fizzbuzz.pce 15
1
2
Fizz
4
Buzz
Fizz
7
8
Fizz
Buzz
11
Fizz
13
14
FizzBuzz
FizzBuzz: 1
```

```
$ ./pycee fizzbuzz.pce 10 2 3
1
Fizz
Buzz
Fizz
5
FizzBuzz
7
Fizz
Buzz
Fizz
FizzBuzz: 1
```

```
$ ./pycee fizzbuzz.pce 10 x 5; echo "exit=$?"
error: 'x' no es un entero positivo
exit=1
$ ./pycee fizzbuzz.pce 0; echo "exit=$?"
error: '0' no es un entero positivo
exit=1
```

## Pistas (léelas solo si te atascas)

<details>
<summary>var, vemp y los bucles</summary>

- `var` vive desde su línea hasta el final de la función, aunque se declare dentro de un bloque. Por eso **no se permite dentro de un bucle**: se declararía otra vez en cada vuelta.
- Dentro del bucle se usa `vemp`, que vive solo en su bloque y es nueva en cada vuelta: `vemp fizz = i % a is 0;`.
- Un contador que sobrevive al bucle se declara antes con `var` y se suma con `count++;` (o `count += 1;`).

</details>

<details>
<summary>Condiciones</summary>

- `if (i % 3)` es un error: la condición tiene que ser un bool. Se escribe `if (i % 3 is 0)`.
- `i % a is 0` ya es un bool, así que puedes guardarlo en una variable y usarlo después en `if (fizz and buzz)`.

</details>

<details>
<summary>Convertir los argumentos</summary>

- `int("12")` da `12`, e `int("x")` lanza `ValueError`.
- Una función pequeña que devuelva el número, o `none` si no lo es, deja `main` limpio:

  ```
  func to_int(s) {
      try {
          return int(s);
      } except ValueError {
          return none;
      }
  }
  ```

- Para guardar varios valores con un valor por defecto, empieza con una lista `var nums = [10, 3, 5];` y sustituye los que lleguen. Después puedes desempaquetarla: `var limit, a, b = nums[0], nums[1], nums[2];`.

</details>

## Para ir más allá (opcional)

- Hazlo también con `switch` en vez de `if`/`elif` (`man(switch)`): ¿sobre qué valor tendrías que hacer el `switch`?
- Admite cualquier número de reglas: `fizzbuzz.pce 30 3:Fizz 5:Buzz 7:Bazz`.
