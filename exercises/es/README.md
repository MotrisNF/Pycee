# Ejercicios de pycee

Doce ejercicios para aprender pycee usándolo, desde el primer `prog` hasta un programa repartido en módulos que guarda su estado en un fichero. Son para gente que ya programa: no explican qué es un bucle, pero sí en qué se diferencia pycee de lo que conoces, sus puntos fuertes y también los flojos.

| # | Ejercicio | Qué introduce |
|---|---|---|
| 01 | [saludo](01_saludo.md) | `prog`, `argv`, `for`, f-strings, códigos de salida |
| 02 | [fizzbuzz](02_fizzbuzz.md) | `var` y `vemp`, condiciones que exigen un bool, `int()` y `try` |
| 03 | [primos](03_primos.md) | listas de tamaño fijo, `while`, comprensiones, salida con ancho |
| 04 | [cesar](04_cesar.md) | strings, `const`, leer hasta `EOFError`, el `%` de C |
| 05 | [matrices](05_matrices.md) | comprensiones anidadas, `map` / `filter`, `sum` / `all` sobre una comprensión, sin `zip` |
| 06 | [notas](06_notas.md) | leer y validar la entrada, tuplas, `sort(key, reverse)`, formato |
| 07 | [palabras](07_palabras.md) | `dict`, ordenar por dos criterios con un `sort` estable (las tuplas no se comparan) |
| 08 | [funciones](08_funciones.md) | lambdas, closures, `...rest`, funciones que hacen funciones |
| 09 | [banco](09_banco.md) | `obj`, herencia, `private`, `static`, errores propios |
| 10 | [iteradores](10_iteradores.md) | métodos especiales: `len`, `get`/`set`, `iter`, `compare`, `to_str` |
| 11 | [calculadora](11_calculadora.md) | un analizador y una jerarquía de errores: todo junto |
| 12 | [tareas](12_tareas.md) | módulos, ficheros, `json`, `os`, `sys`, stderr |

## Cómo trabajar

- Hazlos en orden: cada uno usa lo que introdujeron los anteriores. Los primeros te llevan de la mano; desde el 09 hay menos pistas y el enunciado solo dice qué tiene que pasar.
- Cada ejercicio trae ejemplos con su salida exacta. Tu programa está bien cuando da esa salida **carácter a carácter**, con el mismo código de salida (`echo "exit=$?"`). No hay un corrector automático: ejecuta tú los ejemplos.
- La referencia es el manual: arranca `./pycee` y escribe `man(nombre)` (`man(list.sort)`, `man(fstrings)`, `man()` para el índice). Cada ejercicio dice qué entradas necesita. El manual está en inglés.
- Lee las pistas solo cuando te atasques: están plegadas a propósito.
- Las soluciones no están en el repositorio.

Hay una versión en inglés de los mismos ejercicios en [../en/](../en/README.md).
