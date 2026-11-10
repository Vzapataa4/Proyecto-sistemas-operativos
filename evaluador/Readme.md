# Evaluador de exámenes médicos

Práctica de Sistemas Operativos (ST0257) — EAFIT, 2026-2.

Simulación de un sistema paralelo de evaluación de exámenes médicos usando procesos, memoria compartida, semáforos e hilos.

## Integrantes

- Valentina Zapata Acosta
- Andrés Restrepo

## Lenguaje, herramientas y bibliotecas

- Lenguaje: C (C11)
- Linux: `gcc` o `clang`, `make`. Bibliotecas: `pthread` y `rt` (vienen con el sistema)
- macOS: herramientas de línea de comandos de Xcode
- Windows: MSYS2 con MinGW-w64 (`gcc` y `make`). Solo usa la API de Windows

No se usan bibliotecas externas. Instrucciones completas en [Install.md](Install.md).

## Compilar

```bash
make
```

Los programas quedan en `bin/`.

## Uso rápido

Se usan varias terminales al mismo tiempo.

**Terminal 1** — iniciar el sistema (se queda corriendo hasta Ctrl+C):

```bash
bin/evaluator init
```

**Terminal 2** — registrar muestras:

```bash
bin/evaluator reg examples/muestras1.txt     # crea examples/muestras1.spl con los ids
bin/evaluator reg -                          # interactivo, termina con Ctrl+D (Ctrl+Z en Windows)
0 B 3
1
```

**Terminal 3** — ver el estado y recargar reactivo:

```bash
bin/evaluator ctrl
> list all
> update B 20
```

**Terminal 4** — recoger resultados:

```bash
bin/evaluator rep -m 5      # espera hasta tener 5 resultados
bin/evaluator rep -i 30     # recoge lo que salga durante 30 segundos
```

## Opciones de `init`

| Opción | Qué es | Por omisión |
|---|---|---|
| `-i` | número de bandejas de entrada | 5 |
| `-ie` | posiciones de cada bandeja de entrada | 6 |
| `-oe` | posiciones de la bandeja de salida | 10 |
| `-n` | nombre de la memoria compartida | `evaluator` |
| `-b`, `-d`, `-s` | reactivo inicial de sangre, detritos y piel | 100 |
| `-q` | tamaño de las colas internas | 6 |
| `-ee` | reactivo que lleva el robot en cada viaje | 50 |

En `reg` el nombre se da con `-n`; en `ctrl` y `rep` con `-s` (los tres aceptan cualquiera de las dos).

## Variables de ambiente

| Variable | Para qué |
|---|---|
| `EVALUATOR_BIN` | carpeta donde están los programas reales (por omisión, la de `evaluator`) |
| `EVALUATOR_NAME` | nombre de la memoria compartida si no se da `-n`/`-s` |
| `EVALUATOR_SEED` | semilla del azar: con la misma semilla, cada muestra da el mismo tiempo y resultado |
| `EVALUATOR_SCALE` | divide los tiempos (con `10`, una muestra de 20 s tarda 2 s). Solo para pruebas |

## Ejemplos

En `examples/`:

- `muestras1.txt`: ocho muestras de todos los tipos
- `muestras_invalidas.txt`: líneas buenas y malas, para ver los avisos de `reg`
- `carga_alta.txt`: 100 muestras al azar, para correr varios `reg` al tiempo

## Documentación

El diseño del sistema está en [doc/diseno.md](doc/diseno.md).

## Estado

- [x] Primera entrega: diseño (26 de octubre)
- [x] Segunda entrega: implementación (9 de noviembre)
