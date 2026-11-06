# Ejercicio 08: funciones que hacen funciones

| | |
|---|---|
| Fichero a entregar | `funciones.pce` |
| Se usa | lambdas `(x) => ...` y con bloque `(x) => { ... }`, closures, valores por defecto, argumentos con nombre, `...rest`, `f(...l)`, `reduce`, `dict` como caché |
| En el manual | `man(function)`, `man(func)`, `man(list.reduce)`, `man(list.map)`, `man(tuple)`, `man(dict)` |

En pycee las funciones son valores: se guardan en variables, se pasan y se devuelven. Las closures capturan **la variable**, no una copia de su valor, así que una función puede llevar su propio estado.

## Enunciado

Escribe estas cinco funciones. El programa principal ya está hecho (abajo): cópialo tal cual al final de tu fichero, sin cambiarlo, y la salida debe ser exactamente la del ejemplo.

| Función | Qué devuelve |
|---|---|
| `contador(inicio = 0, paso = 1)` | Una función sin argumentos que, cada vez que se llama, devuelve el siguiente valor: `inicio`, `inicio + paso`, `inicio + 2 * paso`... |
| `memo(f)` | Una función de un argumento que da lo mismo que `f`, pero llama a `f` **una sola vez** por cada argumento distinto: la segunda vez devuelve el resultado guardado. |
| `componer(...fs)` | Una función de un argumento que aplica las funciones **de derecha a izquierda**: `componer(f, g)(x)` es `f(g(x))`. Sin funciones, devuelve `x` tal cual. |
| `repetir(f, n)` | Una función de un argumento que aplica `f` `n` veces seguidas: `repetir(f, 3)(x)` es `f(f(f(x)))`. Con `n` 0, devuelve `x`. |
| `parcial(f, ...primeros)` | Una función que recibe el resto de argumentos y llama a `f` con los `primeros` delante: `parcial(f, 1)(2, 3)` es `f(1, 2, 3)`. |

El programa principal:

```
# ---- dado: no cambies nada de aquí abajo ----
var llamadas = 0;
var fib = none;

func fib_lento(n) {
    llamadas++;
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

prog main() {
    var c = contador();
    var d = contador(10, paso = -5);
    print(c(), c(), c(), d(), d(), d());

    fib = memo(fib_lento);
    print(fib(80), "llamadas:", llamadas);
    print(fib(80), "llamadas:", llamadas);

    var inc = (x) => x + 1;
    var doble = (x) => x * 2;
    print(componer(inc, doble)(5), componer(doble, inc)(5), componer()(5));
    print(repetir(doble, 10)(1), repetir(inc, 0)(7));

    var suma3 = (a, b, c) => a + b + c;
    print(parcial(suma3, 1)(2, 3), parcial(suma3, 1, 2, 3)(), parcial(suma3)(4, 5, 6));

    var fs = [];
    for (i in range(3)) {
        fs.append(() => i * i);
    }
    print(fs.map((f) => f()));
    return 0;
}
```

## Ejemplo

```
$ ./pycee funciones.pce
0 1 2 10 5 0
23416728348467685 llamadas: 81
23416728348467685 llamadas: 81
11 12 5
1024 7
6 6 15
[0, 1, 4]
```

Fíjate en tres cosas de esta salida:

- `fib(80)` sale al instante y con **81 llamadas**: sin `memo` serían miles de millones. Los enteros son exactos hasta 2⁶³ − 1; por encima, `OverflowError` (no se pasan a float en silencio).
- La segunda vez, `llamadas` no cambia: el resultado salía de la caché.
- La última línea es `[0, 1, 4]`: cada vuelta del `for` crea una `i` nueva, y cada lambda captura la suya. (En Python saldría `[4, 4, 4]`.)

## Pistas (léelas solo si te atascas)

<details>
<summary>Estado en una closure</summary>

- Una variable de la función de fuera que la lambda modifica sigue viva después de que la de fuera termine:

  ```
  func contador(inicio = 0, paso = 1) {
      var next = inicio;
      return () => {
          vemp value = next;
          next += paso;
          return value;
      };
  }
  ```

- Una lambda con bloque `{ ... }` necesita `return`; una de expresión, `(x) => x + 1`, no.

</details>

<details>
<summary>memo</summary>

- Guarda los resultados en un `dict` creado dentro de `memo` y capturado por la lambda. `n in cache` dice si ya está.

</details>

<details>
<summary>componer y parcial</summary>

- `...fs` recoge los argumentos en una **tupla**. `fs[::-1]` la da al revés y `reduce((acc, f) => f(acc), x)` las va aplicando partiendo de `x`.
- **Las tuplas no se concatenan con `+`.** Para juntar `primeros` y `resto` pásalas a lista: `primeros.to_list() + resto.to_list()`, y despliégala en la llamada con `f(...lista)`.

</details>

<details>
<summary>Nombres</summary>

- Una lambda puede tener un parámetro que se llame como una variable de la función que la contiene (`(a, b, c)` aunque `main` tenga una `c`): dentro de la lambda, `c` es su parámetro. Lo que no se puede es repetir un nombre dentro de la misma función.

</details>

## Para ir más allá (opcional)

- `memo` solo admite funciones de un argumento. Haz que admita varios (`...args`): ¿qué usarías como clave del dict, si una lista no puede serlo pero una tupla sí? (`man(dict)`)
- Escribe `tubo(...fs)`, que aplica de izquierda a derecha, usando `componer`.
