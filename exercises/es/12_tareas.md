# Ejercicio 12: lista de tareas

| | |
|---|---|
| Ficheros a entregar | `tareas.pce` y `almacen.pce` |
| Se usa | módulos propios (`import`, `from ... import ... as`), `io` (ficheros y `eprint`), `os` (`exists`, `env`), `json`, `sys` (`exit`, `argv`) |
| En el manual | `man(modules)`, `man(import)`, `man(io)`, `man(io.read_file)`, `man(io.write_file)`, `man(io.eprint)`, `man(os.exists)`, `man(os)`, `man(json)`, `man(sys.exit)` |

Un programa de verdad: se usa desde la terminal, guarda su estado en un fichero y está repartido en dos módulos.

## Enunciado

```
./pycee tareas.pce add TEXTO...
./pycee tareas.pce list
./pycee tareas.pce done N
./pycee tareas.pce remove N
./pycee tareas.pce clear
```

Las tareas se guardan en `tareas.json`, en el directorio actual; si existe la variable de entorno `TAREAS`, en el fichero que diga. Si el fichero no existe, se empieza sin tareas.

| Orden | Qué hace | Escribe |
|---|---|---|
| `add TEXTO...` | Añade una tarea con los argumentos unidos por espacios y el número siguiente | `añadida #3: escribir el informe` |
| `list` | Las tareas en orden, y cuántas están hechas | `[x]  2. llamar a Eva` por tarea (el número alineado a la derecha en 2), y al final `1/3 hechas`. Sin tareas, `no hay tareas` antes del total. |
| `done N` | Marca la tarea `N` como hecha | `hecha #2: llamar a Eva` |
| `remove N` | Borra la tarea `N` | `borrada #2: llamar a Eva` |
| `clear` | Borra las tareas hechas | `borradas K tareas hechas` |

- **Los números no se reutilizan**: si borras la 2, la siguiente que añadas será la 4, no la 2.
- Tras cada orden que funciona, el fichero se guarda.

### El fichero

JSON, con este formato (sangrado de 2 espacios, como lo escribe `json.dump(datos, indent = 2)`, y un salto de línea al final):

```
{
  "next": 4,
  "tasks": [
    {
      "id": 1,
      "text": "comprar pan",
      "done": false
    }
  ]
}
```

### Errores

Los mensajes de error van a la **salida de errores** (stderr), no a la normal.

| Situación | Mensaje | Código |
|---|---|---|
| sin orden, orden desconocida o número de argumentos incorrecto | `uso: tareas.pce add TEXTO... \| list \| done N \| remove N \| clear` | 2 |
| `N` no es un número | `error: 'dos' no es un número de tarea` | 1 |
| no hay tarea con ese número | `error: no existe la tarea #9` | 1 |
| el fichero no es JSON válido, o no es un dict con `next` y `tasks` | `error: malo.json no es un fichero de tareas válido: <motivo>` | 1 |

En un error no se guarda nada.

### Requisitos de diseño

- `almacen.pce` es un módulo con `cargar(ruta)` (devuelve los datos; si el fichero no existe, unos datos vacíos) y `guardar(ruta, datos)`. Un fichero estropeado lanza un error propio del módulo, `ErrorAlmacen`.
- `tareas.pce` importa lo que necesita: `from almacen import cargar, guardar as save, ErrorAlmacen;`. Leer, interpretar y escribir el fichero es cosa de `almacen.pce`; los mensajes y los códigos de salida, de `tareas.pce`.

## Ejemplo

```
$ ./pycee tareas.pce list
no hay tareas
0/0 hechas
$ ./pycee tareas.pce add comprar pan
añadida #1: comprar pan
$ ./pycee tareas.pce add llamar a Eva
añadida #2: llamar a Eva
$ ./pycee tareas.pce add "escribir el informe"
añadida #3: escribir el informe
$ ./pycee tareas.pce done 2
hecha #2: llamar a Eva
$ ./pycee tareas.pce list
[ ]  1. comprar pan
[x]  2. llamar a Eva
[ ]  3. escribir el informe
1/3 hechas
$ ./pycee tareas.pce done 9; echo "exit=$?"
error: no existe la tarea #9
exit=1
$ ./pycee tareas.pce clear
borradas 1 tareas hechas
$ ./pycee tareas.pce list
[ ]  1. comprar pan
[ ]  3. escribir el informe
0/2 hechas
$ echo '{"next": 1' > malo.json
$ TAREAS=malo.json ./pycee tareas.pce list; echo "exit=$?"
error: malo.json no es un fichero de tareas válido: json.parse(): expected ',' or '}' but the text ended at line 2, column 1
exit=1
```

Para comprobar que los errores van a stderr: `./pycee tareas.pce done 9 2> /dev/null` no debe escribir nada.

## Para ir más allá (opcional)

- Fechas: guarda cuándo se creó cada tarea (`man(time)`) y muéstralo en `list`.
- `undo`: deshace la última orden (guarda la versión anterior del fichero).
