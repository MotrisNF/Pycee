# Ejercicio 01: saludo

| | |
|---|---|
| Fichero a entregar | `saludo.pce` |
| Se usa | `prog main(argc, argv)`, `for`, f-strings, `return` como código de salida, el ternario `c ? a : b` |
| En el manual | `man(prog)`, `man(sys.argv)`, `man(for)`, `man(fstrings)`, `man(string.capitalize)`, `man(operators)` |

El primer contacto: cómo empieza un programa, cómo recibe sus argumentos y cómo le dice al sistema si ha ido bien.

## Enunciado

Escribe un programa que salude a cada nombre que recibe como argumento y al final diga cuántos saludos ha dado.

```
./pycee saludo.pce ana LUIS
```

### Reglas

1. **Cada argumento es un nombre.** Por cada uno se escribe `hola, Nombre!`, con la primera letra en mayúscula y el resto en minúscula (`LUIS` → `Luis`).
2. **Al final**, `N saludos`. Si solo hay uno, `1 saludo`, en singular.
3. **Sin argumentos** se escribe `uso: saludo.pce nombre...` y el programa termina con código **2**.
4. Si todo va bien, el código de salida es **0**.

## Ejemplos

```
$ ./pycee saludo.pce ana LUIS; echo "exit=$?"
hola, Ana!
hola, Luis!
2 saludos
exit=0
```

```
$ ./pycee saludo.pce eva; echo "exit=$?"
hola, Eva!
1 saludo
exit=0
```

```
$ ./pycee saludo.pce; echo "exit=$?"
uso: saludo.pce nombre...
exit=2
```

## Cómo comprobarlo

Ejecuta los tres ejemplos. La salida debe coincidir carácter a carácter, y el `exit=` también.

## Pistas (léelas solo si te atascas)

<details>
<summary>El punto de entrada</summary>

- Un programa empieza en `prog main(argc, argv)`. `argc` es el número de argumentos y `argv` una **tupla** de strings.
- Como en C, `argv[0]` es el propio programa (`saludo.pce`): los nombres empiezan en `argv[1]`. Por eso, sin argumentos, `argc` vale 1.
- Lo que devuelve `prog` con `return` es el código de salida: `return 2;`.

</details>

<details>
<summary>Recorrer los nombres</summary>

- Un corte da el resto de la tupla: `argv[1:]`.
- `for (name in argv[1:]) { ... }` recorre cada nombre. La variable del `for` solo existe dentro del bucle.
- `"LUIS".capitalize()` da `"Luis"`.

</details>

<details>
<summary>Singular y plural</summary>

- El ternario elige entre dos valores: `n is 1 ? "saludo" : "saludos"`.
- Dentro de un f-string puede ir cualquier expresión, también un ternario: `f"{n} saludo{n is 1 ? "" : "s"}"`.
- No hay `==`: se compara con `is`.

</details>

## Para ir más allá (opcional)

- Si un nombre se repite, salúdalo solo una vez (pista: `man(set)`).
- Saluda en orden alfabético (`man(list.sort)`).
