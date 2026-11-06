# Ejercicio 09: banco

| | |
|---|---|
| Fichero a entregar | `banco.pce` |
| Se usa | `obj`, `create`, `extends`, `private`, `static`, `to_str`, `compare`, errores propios (`obj E extends ValueError`), `switch` sobre strings, varios `except` |
| En el manual | `man(obj)`, `man(private)`, `man(static)`, `man(super)`, `man(errors)`, `man(raise)`, `man(switch)`, `man(is_type)` |

A partir de aquí las pistas son menos: el enunciado dice qué tiene que pasar, y el cómo es cosa tuya.

## Enunciado

Un banco que lee órdenes de la entrada estándar, una por línea, y las ejecuta.

| Orden | Qué hace | Escribe |
|---|---|---|
| `abrir NOMBRE` | Abre una cuenta normal con saldo 0 | `abierta: <cuenta>` |
| `abrir NOMBRE ahorro` | Abre una cuenta de ahorro con saldo 0 | `abierta: <cuenta>` |
| `ingresar NOMBRE CANTIDAD` | Suma la cantidad al saldo | `<cuenta>` |
| `retirar NOMBRE CANTIDAD` | Resta la cantidad, si hay saldo suficiente | `<cuenta>` |
| `transferir ORIGEN DESTINO CANTIDAD` | Retira de una e ingresa en la otra | `<origen> -> <destino>` |
| `interes PORCENTAJE` | Añade ese porcentaje del saldo a cada cuenta de **ahorro**, redondeado a 2 decimales, en orden de apertura | `<cuenta>` por cada una |
| `listar` | Todas las cuentas, de mayor a menor saldo (con el mismo saldo, en orden de apertura) | `  <cuenta>` (dos espacios delante) |

`<cuenta>` se escribe `nombre (normal): 120.50` o `nombre (ahorro): 220.00`: el saldo siempre con dos decimales.

Las líneas vacías se saltan. Al acabar la entrada se escribe `cuentas: N, total: T, errores: E` (cuentas abiertas, suma de saldos con dos decimales, órdenes que fallaron). El código de salida es **1** si hubo algún error y **0** si no.

### Errores

Una orden que falla no cambia nada (tampoco a medias: una transferencia sin saldo no retira de ninguna), escribe `línea N: mensaje` y se sigue con la siguiente.

| Situación | Mensaje |
|---|---|
| saldo insuficiente | `saldo insuficiente: ana tiene 70.00, no puede retirar 500.00` |
| la cuenta no existe | `no existe la cuenta 'eva'` |
| abrir una que ya existe | `la cuenta 'ana' ya existe` |
| cantidad que no es un número mayor que 0 | `cantidad no válida: '-5'` |
| orden desconocida | `orden desconocida: 'volar'` |
| número de palabras incorrecto | `uso: ...` con la forma de la orden, p. ej. `uso: retirar NOMBRE CANTIDAD` |

### Requisitos de diseño

- Un `obj Cuenta` y un `obj CuentaAhorro extends Cuenta`. El saldo es un campo **`private`**: desde fuera solo se lee con un método `saldo()`.
- `Cuenta.abiertas` es un campo **`static`** que cuenta las cuentas abiertas.
- Cada cuenta se muestra con su `to_str()`, y `listar` ordena con `sort(reverse = true)` usando el `compare()` de las cuentas (sin `key`).
- Los errores son tuyos: `obj ErrorBanco extends ValueError`, y `SaldoInsuficiente` y `CuentaNoExiste` que extienden `ErrorBanco`. Fíjate en que el mensaje de saldo insuficiente lleva delante `saldo insuficiente: `: sale de un `except` propio para ese tipo.

## Ejemplo

Entrada:

```
abrir ana
abrir luis ahorro
abrir ana
ingresar ana 100
ingresar luis 250.50
retirar ana 30
retirar ana 500
transferir luis ana 50.5
interes 10
ingresar eva 10
ingresar ana -5
volar ana

listar
```

Salida (código de salida 1):

```
abierta: ana (normal): 0.00
abierta: luis (ahorro): 0.00
línea 3: la cuenta 'ana' ya existe
ana (normal): 100.00
luis (ahorro): 250.50
ana (normal): 70.00
línea 7: saldo insuficiente: ana tiene 70.00, no puede retirar 500.00
luis (ahorro): 200.00 -> ana (normal): 120.50
luis (ahorro): 220.00
línea 10: no existe la cuenta 'eva'
línea 11: cantidad no válida: '-5'
línea 12: orden desconocida: 'volar'
  luis (ahorro): 220.00
  ana (normal): 120.50
cuentas: 2, total: 340.50, errores: 5
```

## Pistas (pocas)

<details>
<summary>Reglas de los obj que te van a saltar</summary>

- Los campos se crean en `create` con `self.x = ...`; los demás métodos los leen y cambian, pero no pueden crear campos nuevos. Un campo `private` o `static` se declara con `private var` / `static var` en el cuerpo del `obj`.
- Un `private` solo lo ve `self`, y solo dentro del `obj` que lo declara: `CuentaAhorro` tampoco lo ve. Que use los métodos de `Cuenta`.
- Un error propio sin `create` acepta un mensaje: `raise CuentaNoExiste("...")`. Captúralo por su tipo: `except SaldoInsuficiente (e) { ... } except ErrorBanco (e) { ... }`; gana el primero que encaje, así que el más concreto va antes.
- `c.is_type("CuentaAhorro")` dice si una cuenta es de ahorro.

</details>

## Para ir más allá (opcional)

- Una cuenta de ahorro solo permite 3 retiradas; la cuarta es un error nuevo, `LimiteRetiradas`.
- Guarda el estado del banco en un fichero al terminar y cárgalo al empezar (`man(io)`, `man(json)`).
