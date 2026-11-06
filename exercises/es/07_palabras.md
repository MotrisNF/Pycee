# Ejercicio 07: palabras

| | |
|---|---|
| Fichero a entregar | `palabras.pce` |
| Se usa | `dict` (`get`, `items`, `keys`, `values`, `in`), `for (k, v in d)`, `sort` estable con `key`, strings (`is_alpha`, `lower`, `to_list`) |
| En el manual | `man(dict)`, `man(dict.get)`, `man(dict.keys)`, `man(list.sort)`, `man(string.lower)`, `man(string.to_list)`, `man(for)` |

Contar cosas es lo que mejor se le da a un diccionario. Y para ordenar por dos criterios a la vez hay que conocer otro detalle de pycee: las tuplas no se comparan.

## Enunciado

```
./pycee palabras.pce [N] < texto.txt
```

Lee un texto de la entrada estándar y escribe estadísticas de sus palabras. Una **palabra** es una secuencia seguida de letras (`is_alpha()`), pasada a minúsculas; todo lo demás (espacios, signos, números) separa palabras.

Se escribe, en este orden:

1. `líneas: L, palabras: W, distintas: D`.
2. `más frecuentes:` y, debajo, las `N` palabras más repetidas (5 si no se da `N`), una por línea: la cuenta alineada a la derecha en 4 caracteres, un espacio y la palabra. Se ordenan de más a menos repetida; **con la misma cuenta, por orden alfabético**.
3. `solo una vez: K`: cuántas palabras distintas aparecen exactamente una vez.
4. `anagramas:` y los grupos de dos o más palabras distintas que tienen las mismas letras, cada grupo entre corchetes y con sus palabras en orden alfabético, y los grupos ordenados por su primera palabra. Si no hay ninguno, `anagramas: ninguno`.

Si no hay ninguna palabra, solo se escribe la primera línea.

### Reglas

- Con más de un argumento, `uso: palabras.pce [N]` (código **2**); si `N` no es un entero, `error: 'X' no es un entero` (código **1**).
- Las letras con tilde cuentan como letras (`is_alpha()` dice que sí), así que `qué` es una palabra.

## Ejemplos

Con este `texto.txt` (la línea 2 está vacía):

```
Roma no se hizo en un dia, y amor no es lo mismo que Roma.

El perro y el lobo; el lobo y la oveja.
Ramo de flores: mora, ramo, amor!
```

```
$ ./pycee palabras.pce < texto.txt
líneas: 4, palabras: 31, distintas: 22
más frecuentes:
   3 el
   3 y
   2 amor
   2 lobo
   2 no
solo una vez: 15
anagramas: [amor, mora, ramo, roma] [es, se]
```

`ramo` y `roma` también salen dos veces, pero quedan fuera del top 5 por orden alfabético.

```
$ ./pycee palabras.pce 2 < texto.txt
líneas: 4, palabras: 31, distintas: 22
más frecuentes:
   3 el
   3 y
solo una vez: 15
anagramas: [amor, mora, ramo, roma] [es, se]
$ ./pycee palabras.pce < /dev/null
líneas: 0, palabras: 0, distintas: 0
```

## Pistas (léelas solo si te atascas)

<details>
<summary>Separar las palabras</summary>

- `split()` no basta, porque deja los signos pegados (`dia,`). Recorre la línea carácter a carácter: mientras sea una letra, añádela a la palabra actual (`cur += c.lower();`); cuando no lo sea, la palabra se acaba.
- Un truco para no olvidar la última palabra: recorre `line + " "`.

</details>

<details>
<summary>Contar</summary>

- `count[w] = count.get(w, 0) + 1;`: `get` da un valor por defecto si la clave no está.
- `for (w, n in count)` recorre clave y valor a la vez. Con una sola variable, `for (x in count)` da cada par como un dict de un elemento: cuidado.

</details>

<details>
<summary>Ordenar por dos cosas</summary>

- En pycee **las tuplas no se comparan** (`tuple(1, "a") < tuple(2, "b")` es un `TypeError`), así que no puedes ordenar por `tuple(-cuenta, palabra)` como en Python.
- Pero `sort` es **estable**: si ordenas primero por el criterio secundario y luego por el principal, los empates del segundo conservan el orden del primero.
- `count.items()` da una lista de `tuple(palabra, cuenta)`, y como los métodos que modifican devuelven la lista, se encadenan: `count.items().sort(key = (p) => p[0]).sort(key = (p) => p[1], reverse = true)`.

</details>

<details>
<summary>Anagramas</summary>

- Dos palabras son anagramas si tienen las mismas letras ordenadas: `"".join(w.to_list().sort())`.
- Agrupa en un dict cuya clave es eso y cuyo valor es la lista de palabras.

</details>

## Para ir más allá (opcional)

- Escribe el resultado también en JSON (`man(json.dump)`).
- Ignora una lista de palabras vacías ("el", "la", "y"...) guardada en un `set`.
