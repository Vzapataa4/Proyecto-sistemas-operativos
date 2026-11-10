/*
 * args.c
 */
#include "args.h"
#include "memoria.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int leer_entero(const char *txt, long min, long max, long *salida)
{
    char *fin;
    long v;

    if (!txt || !*txt)
        return -1;
    errno = 0;
    v = strtol(txt, &fin, 10);
    if (errno || *fin != '\0' || v < min || v > max)
        return -1;
    *salida = v;
    return 0;
}

int nombre_valido(const char *nombre)
{
    size_t i, len = strlen(nombre);

    if (len == 0 || len > MAX_NOMBRE)
        return 0;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)nombre[i];
        if (!isalnum(c) && c != '_' && c != '-')
            return 0;
    }
    return 1;
}

const char *nombre_por_omision(void)
{
    const char *n = getenv("EVALUATOR_NAME");
    return (n && *n) ? n : "evaluator";
}

long escala_tiempo(void)
{
    long v;
    const char *e = getenv("EVALUATOR_SCALE");
    if (e && leer_entero(e, 1, 1000, &v) == 0)
        return v;
    return 1;
}

unsigned semilla_inicial(void)
{
    long v;
    const char *e = getenv("EVALUATOR_SEED");
    if (e && leer_entero(e, 0, 2147483647L, &v) == 0)
        return (unsigned)v;
    return (unsigned)time(NULL);
}

unsigned azar(unsigned *estado)
{
    /* xorshift de 32 bits: rápido y suficiente para la simulación */
    unsigned x = *estado ? *estado : 2463534242u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *estado = x;
    return x;
}

int azar_entre(unsigned *estado, int min, int max)
{
    return min + (int)(azar(estado) % (unsigned)(max - min + 1));
}
