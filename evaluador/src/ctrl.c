/*
 * ctrl.c  (evaluator-ctrl)
 * Consola para revisar el sistema (list) y recargar reactivo (update).
 * Termina con fin de fichero (Ctrl+D en Linux, Ctrl+Z + Enter en Windows).
 * Ver doc/diseno.md, sección 3.3.
 */
#include "memoria.h"
#include "cola.h"
#include "args.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* la "foto" que se toma antes de imprimir */
typedef struct {
    muestra_t *esperando;
    int        n_esperando;
    muestra_t  procesando[3];
    int        ocupado[3];
    muestra_t *reportados;
    int        n_reportados;
    int        reactivo[3];
} foto_t;

static int comparar_id(const void *a, const void *b)
{
    const muestra_t *x = a, *y = b;
    return (x->id > y->id) - (x->id < y->id);
}

/* copia cada estructura con su candado y lo suelta enseguida;
 * nunca se tienen dos candados a la vez */
static void tomar_foto(sistema_t *sis, foto_t *f)
{
    cabecera_t *cab = sis->cab;
    tablero_t *tab = tablero_de(sis);
    cola_t *c;
    int k, t;

    f->n_esperando = 0;
    for (k = 0; k < cab->num_entradas; k++) {
        c = cola_entrada(sis, k);
        semaforo_esperar(&sis->sem.e_mutex[k]);
        f->n_esperando += cola_copiar(c, items_de(sis, c), f->esperando + f->n_esperando);
        semaforo_avisar(&sis->sem.e_mutex[k]);
    }

    semaforo_esperar(&sis->sem.tablero);
    for (t = 0; t < 3; t++) {
        memcpy(f->esperando + f->n_esperando, tablero_cola(sis, t),
               (size_t)tab->en_cola[t] * sizeof(muestra_t));
        f->n_esperando += tab->en_cola[t];
        f->ocupado[t] = tab->ocupado[t];
        f->procesando[t] = tab->actual[t];
    }
    for (k = 0; k < cab->num_entradas; k++)
        if (tab->despachando[k])
            f->esperando[f->n_esperando++] = tab->en_mano[k];
    semaforo_avisar(&sis->sem.tablero);
    qsort(f->esperando, (size_t)f->n_esperando, sizeof(muestra_t), comparar_id);

    c = cola_salida(sis);
    semaforo_esperar(&sis->sem.s_mutex);
    f->n_reportados = cola_copiar(c, items_de(sis, c), f->reportados);
    semaforo_avisar(&sis->sem.s_mutex);

    semaforo_esperar(&sis->sem.reactivo);
    for (t = 0; t < 3; t++)
        f->reactivo[t] = cab->reactivo[t];
    semaforo_avisar(&sis->sem.reactivo);
}

static void mostrar_processing(const foto_t *f)
{
    const char *sep = "";
    int t;

    printf("Processing:\n");
    for (t = 0; t < 3; t++) {
        const muestra_t *m = &f->procesando[t];
        if (!f->ocupado[t])
            continue;
        printf("%s[%llu %d %c %d %d]", sep, (unsigned long long)m->id, m->bandeja,
               letra_tipo(m->tipo), m->cantidad, m->t_proceso);
        sep = " ";
    }
    if (*sep)
        printf("\n");
}

static void mostrar_waiting(const foto_t *f)
{
    int i;

    printf("Waiting:\n");
    for (i = 0; i < f->n_esperando; i++) {
        const muestra_t *m = &f->esperando[i];
        printf("%s[%llu %d %c %d]", i ? " " : "", (unsigned long long)m->id, m->bandeja,
               letra_tipo(m->tipo), m->cantidad);
    }
    if (f->n_esperando)
        printf("\n");
}

static void mostrar_reported(const foto_t *f)
{
    int i;

    printf("Reported:\n");
    for (i = 0; i < f->n_reportados; i++) {
        const muestra_t *m = &f->reportados[i];
        printf("%s[%llu %d %c %s]", i ? " " : "", (unsigned long long)m->id, m->bandeja,
               letra_tipo(m->tipo), resultado_largo(m->resultado));
    }
    if (f->n_reportados)
        printf("\n");
}

static void mostrar_reactive(const foto_t *f)
{
    printf("Reactive:\n[B %d] [D %d] [S %d]\n", f->reactivo[0], f->reactivo[1], f->reactivo[2]);
}

static void comando_list(sistema_t *sis, foto_t *f, const char *que)
{
    int todo = (que == NULL || strcmp(que, "all") == 0);

    if (!todo && strcmp(que, "processing") && strcmp(que, "waiting") &&
        strcmp(que, "reported") && strcmp(que, "reactive")) {
        fprintf(stderr, "list: opcion desconocida '%s' (processing, waiting, reported, reactive o all)\n", que);
        return;
    }

    tomar_foto(sis, f);
    if (todo || strcmp(que, "processing") == 0)
        mostrar_processing(f);
    if (todo || strcmp(que, "waiting") == 0)
        mostrar_waiting(f);
    if (todo || strcmp(que, "reported") == 0)
        mostrar_reported(f);
    if (todo || strcmp(que, "reactive") == 0)
        mostrar_reactive(f);
}

static void comando_update(sistema_t *sis, const char *letra, const char *cant)
{
    int tipo, nivel;
    long q;

    if (!letra || !cant || strlen(letra) != 1 || (tipo = tipo_desde_letra(letra[0])) < 0) {
        fprintf(stderr, "uso: update { B | D | S } <cantidad>\n");
        return;
    }
    if (leer_entero(cant, 1, 1000000, &q) < 0) {
        fprintf(stderr, "update: la cantidad debe ser un entero positivo\n");
        return;
    }

    semaforo_esperar(&sis->sem.reactivo);
    nivel = sis->cab->reactivo[tipo] + (int)(q > REACTIVO_MAX ? REACTIVO_MAX : q);
    if (nivel > REACTIVO_MAX)
        nivel = REACTIVO_MAX;
    sis->cab->reactivo[tipo] = nivel;
    semaforo_avisar(&sis->sem.reactivo);

    printf("%c %d\n", letra_tipo(tipo), nivel);
}

int main(int argc, char **argv)
{
    sistema_t sis;
    foto_t foto;
    const char *nombre = nombre_por_omision();
    char linea[256];
    char *cmd, *a1, *a2, *a3;
    int i, interactivo, maximo;

    for (i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-n") == 0) && i + 1 < argc) {
            nombre = argv[++i];
        } else {
            fprintf(stderr, "uso: evaluator ctrl [-s <nombre>]\n");
            return 2;
        }
    }
    if (!nombre_valido(nombre)) {
        fprintf(stderr, "ctrl: nombre invalido '%s'\n", nombre);
        return 2;
    }
    if (conectar(&sis, nombre) != 0) {
        fprintf(stderr, "ctrl: el sistema '%s' no esta iniciado (use evaluator init)\n", nombre);
        return 3;
    }

    /* espacio para la foto: lo máximo que puede haber esperando y en la salida */
    maximo = sis.cab->num_entradas * (sis.cab->tam_entrada + 1) + 3 * sis.cab->tam_interna;
    foto.esperando = malloc((size_t)maximo * sizeof(muestra_t));
    foto.reportados = malloc((size_t)sis.cab->tam_salida * sizeof(muestra_t));
    if (!foto.esperando || !foto.reportados) {
        fprintf(stderr, "ctrl: sin memoria\n");
        return 1;
    }

    interactivo = entrada_es_terminal();
    for (;;) {
        if (interactivo) {
            printf("> ");
            fflush(stdout);
        }
        if (!fgets(linea, sizeof(linea), stdin))
            break;

        cmd = strtok(linea, " \t\r\n");
        if (!cmd)
            continue;
        a1 = strtok(NULL, " \t\r\n");
        a2 = strtok(NULL, " \t\r\n");
        a3 = strtok(NULL, " \t\r\n");

        if (!sis.cab->activo) {
            fprintf(stderr, "ctrl: el evaluador se cerro\n");
            free(foto.esperando);
            free(foto.reportados);
            desconectar(&sis);
            return 3;
        }

        if (strcmp(cmd, "list") == 0 && !a2) {
            comando_list(&sis, &foto, a1);
        } else if (strcmp(cmd, "update") == 0 && !a3) {
            comando_update(&sis, a1, a2);
        } else {
            fprintf(stderr, "comando desconocido. Use:\n"
                            "  list [ processing | waiting | reported | reactive | all ]\n"
                            "  update { B | D | S } <cantidad>\n");
        }
        fflush(stdout);
    }

    if (interactivo)
        printf("\n");
    free(foto.esperando);
    free(foto.reportados);
    desconectar(&sis);
    return 0;
}
