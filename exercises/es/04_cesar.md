# Ejercicio 04: cifrado César

| | |
|---|---|
| Fichero a entregar | `cesar.pce` |
| Se usa | strings (`find`, índices, `for` sobre un string, `join`), `const`, `input()` hasta `EOFError`, `%` con negativos |
| En el manual | `man(string)`, `man(string.find)`, `man(const)`, `man(input)`, `man(EOFError)`, `man(operators)` |

Un ejercicio pequeño con dos trampas del lenguaje: no hay `ord` ni `chr` para pasar de letra a número, y `%` funciona como en C, no como en Python.

## Enunciado

```
./pycee cesar.pce K < texto.txt
```

Lee el texto de la entrada estándar, línea a línea hasta que se acabe, y escribe cada línea cifrada: cada letra se desplaza `K` posiciones en el alfabeto, dando la vuelta al llegar al final (`z` + 1 es `a`).

### Reglas

1. **Solo se cambian las 26 letras inglesas**, `a`–`z` y `A`–`Z`, y cada una conserva su caso. Todo lo demás se queda igual: espacios, números, signos y letras como `ñ` o `á`.
2. **`K` puede ser cualquier entero**, también negativo o mayor que 26: `-3` deshace un `3`, y `29` es lo mismo que `3`.
3. **Argumentos.** Con un número de argumentos distinto de 1, `uso: cesar.pce K` (código **2**). Si `K` no es un entero, `error: 'X' no es un entero` (código **1**).

## Ejemplos

Con este `texto.txt`:

```
Hola, Mundo!
zorro XYZ
Año 2026: ¿qué tal?
```

```
$ ./pycee cesar.pce 3 < texto.txt
Krod, Pxqgr!
cruur ABC
Dñr 2026: ¿txé wdo?
$ ./pycee cesar.pce -29 < texto.txt
Elix, Jrkal!
wlool UVW
Xñl 2026: ¿nré qxi?
```

Cifrar y descifrar devuelve el texto original:

```
$ ./pycee cesar.pce 3 < texto.txt | ./pycee cesar.pce -3
Hola, Mundo!
zorro XYZ
Año 2026: ¿qué tal?
```

## Pistas (léelas solo si te atascas)

<details>
<summary>Letras sin ord ni chr</summary>

- pycee no tiene `ord` ni `chr`. La posición de una letra se saca de un alfabeto: `const LOWER = "abcdefghijklmnopqrstuvwxyz";` y `LOWER.find(c)` da su posición, o `-1` si no está.
- La letra nueva es `LOWER[(i + k) % 26]`.

</details>

<details>
<summary>El % de pycee</summary>

- En pycee, `%` sigue a C: el resultado tiene el signo del primer operando. `-7 % 3` es `-1`, no `2` como en Python.
- Con `K` negativo, `(i + k) % 26` puede salir negativo. Aquí da la casualidad de que funciona, porque un índice negativo cuenta desde el final (`LOWER[-3]` es `"x"`, justo la letra que toca). Pero es una casualidad: si usaras ese número para otra cosa (compararlo, guardarlo, calcular con él), estaría mal. Normaliza `K` una vez al empezar, para que quede entre 0 y 25: `k = (k % 26 + 26) % 26;`.

</details>

<details>
<summary>Construir el string</summary>

- Los strings son inmutables: no se puede hacer `s[i] = "x"`.
- Recorre la línea con `for (c in line)` (cada `c` es un string de un carácter, también para `ñ`) y junta el resultado: `"".join([cambiar(c, k) for (c in line)])`.

</details>

<details>
<summary>Leer hasta el final</summary>

```
while (true) {
    try {
        print(encode(input(), k));
    } except EOFError {
        break;
    }
}
```

</details>

## Para ir más allá (opcional)

- Rompe el cifrado sin conocer `K`: prueba los 26 desplazamientos y quédate con el que deja más veces la letra `e` (o la que más se repite en tu idioma).
- Cifra también los dígitos, con su propio alfabeto de 10.
