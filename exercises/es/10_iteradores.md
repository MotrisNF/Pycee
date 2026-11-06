# Ejercicio 10: tus propios tipos

| | |
|---|---|
| Fichero a entregar | `iteradores.pce` |
| Se usa | los métodos especiales `len`, `get`, `set`, `iter` / `has_next` / `next`, `to_str`, `compare`; `...rest`; varios valores a la vez (`a, b = b, a + b`) |
| En el manual | `man(obj)`, `man(for)`, `man(types)`, `man(operators)`, `man(IndexError)` |

Un `obj` puede comportarse como un tipo del lenguaje: indexarse con `x[i]`, recorrerse con `for`, mostrarse con `print` y compararse con `<`. Lo que pycee **no** tiene es sobrecarga de `+`, `-` o `*`: para eso se usan métodos.

## Enunciado

Escribe tres `obj`. El programa principal ya está hecho (abajo): cópialo al final de tu fichero sin cambiarlo.

**`Rango(inicio, fin, paso = 1)`**, como `range`, pero que admite decimales.

- `len()` es cuántos valores da: `ceil((fin - inicio) / paso)`, o 0 si sale negativo.
- `r[i]` es `inicio + i * paso`; un `i` negativo cuenta desde el final. Fuera de rango, `IndexError` con el mensaje `índice I fuera de un Rango de N`.
- `for (x in r)` da sus valores en orden.
- Se muestra `Rango(0, 1, 0.25)`.
- Un `paso` de 0 es `ValueError` con el mensaje `el paso no puede ser 0`.

**`Fib(limite)`**: un `for` sobre él da los números de Fibonacci (0, 1, 1, 2, 3...) que no pasan de `limite`. Se puede recorrer más de una vez, y cada vez empieza de nuevo.

**`Vector(...xs)`**, un vector de cualquier número de componentes.

- `len()`, `v[i]` y `v[i] = x` sobre sus componentes.
- `norma()` es la raíz de la suma de los cuadrados.
- `sumar(w)` da un `Vector` nuevo, componente a componente. Con distinto tamaño, `ValueError` con el mensaje `tamaños distintos: A y B`.
- Dos vectores se comparan por su norma (`v > w`, y `sort` los ordena de menor a mayor norma).
- Se muestra `<3, 4>`.

El programa principal:

```
# ---- dado: no cambies nada de aquí abajo ----
prog main() {
    var r = Rango(0, 1, 0.25);
    print(r, r.len(), r[0], r[-1], [x for (x in r)]);
    print([x for (x in Rango(10, 0, -3))], Rango(5, 5).len());
    try {
        print(r[4]);
    } except IndexError (e) {
        print("IndexError:", e.message());
    }
    try {
        Rango(0, 1, 0);
    } except ValueError (e) {
        print("ValueError:", e.message());
    }

    var f = Fib(100);
    print([x for (x in f)]);
    print(sum(x for (x in f) if (x % 2 is 0)));

    var v = Vector(3, 4);
    var w = Vector(1, 1);
    print(v, v.len(), v.norma(), v.sumar(w), v > w);
    v[0] = 0;
    print(v, [Vector(5), Vector(-1), Vector(2, 2)].sort());
    try {
        v.sumar(Vector(1));
    } except ValueError (e) {
        print("ValueError:", e.message());
    }
    return 0;
}
```

## Ejemplo

```
$ ./pycee iteradores.pce
Rango(0, 1, 0.25) 4 0.0 0.75 [0.0, 0.25, 0.5, 0.75]
[10, 7, 4, 1] 0
IndexError: índice 4 fuera de un Rango de 4
ValueError: el paso no puede ser 0
[0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89]
44
<3, 4> 2 5.0 <4, 5> true
<0, 4> [<-1>, <2, 2>, <5>]
ValueError: tamaños distintos: 2 y 1
```

`r[0]` sale `0.0` y no `0` porque `0 + 0 * 0.25` ya es un float. La segunda vez que se recorre `f` (en el `sum`) vuelve a empezar desde 0.

## Pistas (pocas)

<details>
<summary>Cómo usa el lenguaje tus métodos</summary>

- `for (x in obj)` llama a `obj.iter()`, y sobre lo que devuelve, a `has_next()` (que debe dar un bool) y `next()` en cada vuelta. `iter()` puede devolver otro objeto (un "cursor" con su posición) o el propio objeto, reiniciado.
- `obj[i]` llama a `get(i)` y `obj[i] = v` a `set(i, v)`.
- `compare(otro)` devuelve un número: negativo si `self` va antes, 0 si son iguales, positivo si va después. Con él funcionan `<`, `>`... y `sort`.
- Recuerda que los campos se crean en `create`: un `iter()` que reinicia un campo puede cambiarlo, pero el campo tiene que existir desde `create`.

</details>

## Para ir más allá (opcional)

- Añade a `Rango` un método `al_reves()` que devuelva otro `Rango` con los mismos valores en orden inverso.
- Un `Matriz(filas, columnas)` cuyo `for` dé cada fila como un `Vector`.
