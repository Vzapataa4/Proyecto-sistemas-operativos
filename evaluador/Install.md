# Instalación

## Requisitos

| Sistema | Qué instalar |
|---|---|
| Linux | `gcc` (o `clang`) y `make`. En Ubuntu: `sudo apt install build-essential` |
| macOS | Las herramientas de línea de comandos de Xcode: `xcode-select --install` |
| Windows | [MSYS2](https://www.msys2.org) y, desde la terminal **MSYS2 MINGW64**: `pacman -S mingw-w64-x86_64-gcc make` |

## Compilar

Dentro de la carpeta `evaluador/`:

```bash
make
```

Los programas quedan en `bin/`:

```
bin/evaluator        (el envoltorio, el que se usa)
bin/evaluator-init
bin/evaluator-reg
bin/evaluator-ctrl
bin/evaluator-rep
```

En Windows llevan la extensión `.exe`. El `Makefile` detecta solo el sistema operativo.

Para borrar lo compilado: `make clean`.

## Instalar en otro directorio

```bash
make install PREFIX=/ruta/destino
```

Copia los cinco programas a `/ruta/destino/bin`. Si no se indica `PREFIX`, se usa `/usr/local`
(en ese caso puede necesitar `sudo`).

Ejemplo, instalando en la carpeta personal:

```bash
make install PREFIX=$HOME/evaluador
export PATH="$HOME/evaluador/bin:$PATH"
evaluator init
```

`evaluator` busca los programas reales en su misma carpeta, así que deben quedar juntos.
Si se quieren tener en otra carpeta, se indica con la variable `EVALUATOR_BIN`:

```bash
export EVALUATOR_BIN=/otra/carpeta
```

En Windows (terminal MSYS2 MINGW64):

```bash
make install PREFIX=/c/evaluador
```

y luego se pueden usar desde `cmd` o PowerShell con `C:\evaluador\bin\evaluator.exe`.
