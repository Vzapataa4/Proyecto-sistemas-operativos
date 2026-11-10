/*
 * despachador.c
 * Un hilo por bandeja de entrada. Saca cada muestra de su bandeja y la
 * pone en la cola interna de su tipo (sección 6.2 del diseño).
 */
#include "evaluador.h"

/* mete la muestra en la cola interna respetando el turno de llegada.
 * Devuelve -1 si el sistema se está apagando. */
static int meter_en_interna(evaluador_t *ev, const muestra_t *m, int bandeja)
{
    cola_interna_t *c = &ev->internas[m->tipo];
    uint64_t mi_turno;

    mutex_bloquear(&c->mutex);
    mi_turno = c->turno_siguiente++;
    while (sigue_activo(ev) &&
           (c->datos.cuenta == c->datos.capacidad || mi_turno != c->turno_actual))
        cond_esperar(&c->hay_espacio, &c->mutex);

    if (!sigue_activo(ev)) {
        mutex_soltar(&c->mutex);
        return -1;
    }

    cola_meter(&c->datos, c->items, m);
    c->turno_actual++;
    publicar_cola_interna(ev, m->tipo, bandeja);

    cond_avisar_todos(&c->hay_espacio);   /* que pase el siguiente turno */
    cond_avisar(&c->hay_muestras);
    mutex_soltar(&c->mutex);
    return 0;
}

void *hilo_despachador(void *arg)
{
    arg_hilo_t *a = (arg_hilo_t *)arg;
    evaluador_t *ev = a->ev;
    sistema_t *sis = ev->sis;
    int k = a->numero;
    cola_t *bandeja = cola_entrada(sis, k);
    muestra_t m;

    while (sigue_activo(ev)) {
        semaforo_esperar(&sis->sem.e_llenos[k]);
        if (!sigue_activo(ev))
            break;

        semaforo_esperar(&sis->sem.e_mutex[k]);
        cola_sacar(bandeja, items_de(sis, bandeja), &m);
        semaforo_avisar(&sis->sem.e_mutex[k]);
        semaforo_avisar(&sis->sem.e_vacios[k]);
        marcar_despachador(ev, k, &m);

        if (meter_en_interna(ev, &m, k) < 0)
            break;
    }
    return NULL;
}
