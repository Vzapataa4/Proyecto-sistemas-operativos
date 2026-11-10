/*
 * rep.c  (evaluator-rep)
 * Retira resultados de la bandeja de salida y los muestra como [id i k r].
 *   -i <segundos>  escucha durante ese tiempo y luego termina
 *   -m <cantidad>  retira exactamente esa cantidad (espera si faltan)
 * Ver doc/diseno.md, sección 3.4.
 */
#include "memoria.h"
#include "cola.h"
#include "args.h"

#include <stdio.h>
#include <string.h>

static void uso(void)
{
    fprintf(stderr, "uso: evaluator rep [-s <nombre>] { -i <segundos> | -m <cantidad> }\n");
}

/* saca un resultado de la salida (ya se hizo el wait de s_llenos) */
static void sacar_resultado(sistema_t *sis, muestra_t *m)
{
    cola_t *c = cola_salida(sis);

    semaforo_esperar(&sis->sem.s_mutex);
    cola_sacar(c, items_de(sis, c), m);
    semaforo_avisar(&sis->sem.s_mutex);
    semaforo_avisar(&sis->sem.s_vacios);   /* un lugar libre para los analizadores */
}

static void imprimir(const muestra_t *m)
{
    printf("[%llu %d %c %c]\n", (unsigned long long)m->id, m->bandeja,
           letra_tipo(m->tipo), m->resultado);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    sistema_t sis;
    const char *nombre = nombre_por_omision();
    long segundos = -1, cantidad = -1, i;
    int64_t fin = 0, falta;
    muestra_t m;
    int a;

    for (a = 1; a < argc; a++) {
        if (a + 1 >= argc) {
            uso();
            return 2;
        }
        if (strcmp(argv[a], "-s") == 0 || strcmp(argv[a], "-n") == 0) {
            nombre = argv[++a];
        } else if (strcmp(argv[a], "-i") == 0) {
            if (leer_entero(argv[++a], 0, 86400L * 365, &segundos) < 0) {
                fprintf(stderr, "rep: -i necesita un numero de segundos\n");
                return 2;
            }
        } else if (strcmp(argv[a], "-m") == 0) {
            if (leer_entero(argv[++a], 1, 1000000000L, &cantidad) < 0) {
                fprintf(stderr, "rep: -m necesita una cantidad positiva\n");
                return 2;
            }
        } else {
            uso();
            return 2;
        }
    }
    if ((segundos < 0) == (cantidad < 0)) {
        fprintf(stderr, "rep: debe indicar exactamente una opcion: -i o -m\n");
        uso();
        return 2;
    }
    if (!nombre_valido(nombre)) {
        fprintf(stderr, "rep: nombre invalido '%s'\n", nombre);
        return 2;
    }
    if (conectar(&sis, nombre) != 0) {
        fprintf(stderr, "rep: el sistema '%s' no esta iniciado (use evaluator init)\n", nombre);
        return 3;
    }

    if (cantidad > 0) {
        for (i = 0; i < cantidad; i++) {
            semaforo_esperar(&sis.sem.s_llenos);
            if (!sis.cab->activo) {
                fprintf(stderr, "rep: el evaluador se cerro\n");
                desconectar(&sis);
                return 3;
            }
            sacar_resultado(&sis, &m);
            imprimir(&m);
        }
    } else {
        fin = reloj_ms() + (int64_t)segundos * 1000;
        while ((falta = fin - reloj_ms()) > 0) {
            if (semaforo_esperar_ms(&sis.sem.s_llenos, (long)falta) != 0)
                break;   /* se acabó el tiempo */
            if (!sis.cab->activo) {
                fprintf(stderr, "rep: el evaluador se cerro\n");
                desconectar(&sis);
                return 3;
            }
            sacar_resultado(&sis, &m);
            imprimir(&m);
        }
    }

    desconectar(&sis);
    return 0;
}
