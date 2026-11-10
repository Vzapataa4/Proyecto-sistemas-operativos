/*
 * evaluator.c
 * El envoltorio. No hace el trabajo: mira el comando (init, reg, ctrl o rep)
 * y ejecuta el programa real evaluator-<comando>.
 *
 * Dónde lo busca:
 *   1. en la carpeta de la variable EVALUATOR_BIN, si existe
 *   2. si no, en la misma carpeta donde está este ejecutable
 */
#include "plataforma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ayuda(void)
{
    fprintf(stderr,
        "uso: evaluator <comando> [opciones]\n"
        "\n"
        "comandos:\n"
        "  init  inicia el sistema (memoria compartida, semaforos y evaluador)\n"
        "  reg   registra muestras desde ficheros o desde la entrada estandar\n"
        "  ctrl  consola para consultar (list) y recargar reactivo (update)\n"
        "  rep   retira y muestra los resultados\n");
}

int main(int argc, char **argv)
{
    static const char *comandos[] = { "init", "reg", "ctrl", "rep" };
    char carpeta[4096], ruta[4200];
    const char *bin;
    char *sep;
    int i, valido = 0;

    if (argc < 2) {
        ayuda();
        return 2;
    }
    for (i = 0; i < 4; i++)
        if (strcmp(argv[1], comandos[i]) == 0)
            valido = 1;
    if (!valido) {
        fprintf(stderr, "evaluator: comando desconocido '%s'\n\n", argv[1]);
        ayuda();
        return 2;
    }

    bin = getenv("EVALUATOR_BIN");
    if (bin && *bin) {
        snprintf(carpeta, sizeof(carpeta), "%s", bin);
    } else {
        if (ruta_ejecutable(carpeta, sizeof(carpeta), argv[0]) < 0) {
            fprintf(stderr, "evaluator: no se pudo saber donde esta el ejecutable\n");
            return 127;
        }
        /* quitar el nombre del archivo y dejar solo la carpeta */
        sep = strrchr(carpeta, SEPARADOR);
#ifdef _WIN32
        {
            char *otra = strrchr(carpeta, '/');
            if (otra && (!sep || otra > sep))
                sep = otra;
        }
#endif
        if (sep)
            *sep = '\0';
        else
            snprintf(carpeta, sizeof(carpeta), ".");
    }

    snprintf(ruta, sizeof(ruta), "%s%cevaluator-%s%s", carpeta, SEPARADOR, argv[1], EXTENSION_EXE);
    if (!existe_archivo(ruta)) {
        fprintf(stderr, "evaluator: no se encontro el programa %s\n", ruta);
        return 127;
    }

    /* el programa real recibe su propia ruta como argv[0] y luego
     * el resto de argumentos (sin el nombre del comando) */
    argv[1] = ruta;
    i = lanzar_programa(ruta, argv + 1);
    if (i == 127)
        fprintf(stderr, "evaluator: no se pudo ejecutar %s\n", ruta);
    return i;
}
