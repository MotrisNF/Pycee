# Ejercicio 11: calculadora

| | |
|---|---|
| Fichero a entregar | `calculadora.pce` |
| Se usa | todo lo anterior: un analizador léxico, un analizador por descenso recursivo en un `obj`, una jerarquía de errores propios con campos, `super.create`, `except` con varios tipos |
| En el manual | `man(errors)`, `man(super)`, `man(try)`, `man(number.pow)`, `man(number.sqrt)`, `man(OverflowError)`, `man(ZeroDivisionError)` |

Un intérprete pequeño escrito en tu intérprete. Aquí no hay pistas de código: solo la especificación.

## Enunciado

Lee expresiones de la entrada estándar, una por línea, y escribe el resultado de cada una.

### La gramática

```
línea      = NOMBRE "=" expr | expr
expr       = term { ("+" | "-") term }
term       = unario { ("*" | "/" | "%") unario }
unario     = "-" unario | potencia
potencia   = primario [ "^" unario ]
primario   = NÚMERO | NOMBRE | NOMBRE "(" expr ")" | "(" expr ")"
```

- `NÚMERO`: dígitos, con un punto decimal opcional (`3`, `2.5`). Se convierte con `num()`.
- `NOMBRE`: una letra seguida de letras, dígitos o `_`.
- `^` es la potencia y asocia por la derecha (`2 ^ 3 ^ 2` es `2 ^ 9`). Como en matemáticas, `-2 ^ 2` es `-4`.
- Funciones: `sqrt(x)` y `abs(x)`.
- Los operadores se comportan como los de pycee: `/` da siempre float, `%` sigue a C y los enteros son exactos y de 64 bits.

### Qué se escribe

- Una expresión escribe su valor tal como lo escribe `print`: `7`, `3.5`.
- Una asignación `x = expr` guarda el valor y escribe `x = valor`. Las variables duran hasta el final de la entrada.
- `pi` ya está definida (`3.141592653589793`).
- Las líneas vacías y las que empiezan por `#` se saltan (pero cuentan para el número de línea).
- Al final: `L líneas, E errores`. El código de salida es **1** si hubo algún error y **0** si no.

### Errores

Un error escribe un mensaje y se sigue con la línea siguiente.

| Situación | Mensaje |
|---|---|
| error de sintaxis | `línea N, columna C: mensaje` (`C` es la columna del token, desde 1) |
| un carácter que no es de la gramática | `carácter inesperado '$'` |
| falta algo al final | `falta un número`, o `se esperaba ')' y hay el final` |
| sobra algo | `sobra '5'` |
| un número como `1.2.3` | `número mal escrito '1.2.3'` (columna del número) |
| variable o función que no existe | `línea N: variable no definida: z`, `línea N: función desconocida: foo` |
| errores del cálculo | `línea N: Tipo: mensaje`, con el tipo y el mensaje del error de pycee (`ZeroDivisionError`, `ValueError`, `OverflowError`) |

### Requisitos de diseño

- Una jerarquía propia: `obj ErrorCalc extends Error`; `ErrorSintaxis extends ErrorCalc`, con un campo `col` (su `create(msg, col)` llama a `super.create(msg)`); y `ErrorNombre extends ErrorCalc`.
- El analizador es un `obj` (con la lista de tokens y la posición actual como campos) y un método por regla de la gramática.

## Ejemplo

Entrada:

```
1 + 2 * 3
(1 + 2) * 3
2 ^ 3 ^ 2
-2 ^ 2
2 ^ -1
y = 2 ^ 63
7 / 2
7 % 3
# un comentario
x = 2 ^ 62
x + 1
r = sqrt(16) + abs(-3)
2 * pi * r
z * 2
3 +
(4 + 5
1 / 0
2 $ 3
sqrt(-1)
foo(2)
1.2.3
4 5
```

Salida (código de salida 1):

```
7
9
512
-4
0.5
línea 6: OverflowError: integer overflow
3.5
1
x = 4611686018427387904
4611686018427387905
r = 7.0
43.982297150257104
línea 14: variable no definida: z
línea 15, columna 4: falta un número
línea 16, columna 7: se esperaba ')' y hay el final
línea 17: ZeroDivisionError: division by zero
línea 18, columna 3: carácter inesperado '$'
línea 19: ValueError: the square root of a negative number is not a real number
línea 20: función desconocida: foo
línea 21, columna 1: número mal escrito '1.2.3'
línea 22, columna 3: sobra '5'
22 líneas, 10 errores
```

`2 ^ 63` no cabe en un entero de 64 bits: pycee lanza `OverflowError` en vez de perder precisión.

## Para ir más allá (opcional)

- Funciones definidas por el usuario: `f(x) = x * x + 1` y después `f(3)`.
- Comparaciones y el ternario: `x > 3 ? 1 : 0`.
