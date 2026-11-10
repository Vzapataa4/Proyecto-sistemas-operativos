/*
 * cola.c
 */
#include "cola.h"

void cola_preparar(cola_t *c, int capacidad, uint32_t desp_items)
{
    c->capacidad = capacidad;
    c->inicio = 0;
    c->fin = 0;
    c->cuenta = 0;
    c->desp_items = desp_items;
}

void cola_meter(cola_t *c, muestra_t *items, const muestra_t *m)
{
    items[c->fin] = *m;
    c->fin = (c->fin + 1) % c->capacidad;
    c->cuenta++;
}

void cola_sacar(cola_t *c, muestra_t *items, muestra_t *m)
{
    *m = items[c->inicio];
    c->inicio = (c->inicio + 1) % c->capacidad;
    c->cuenta--;
}

int cola_copiar(const cola_t *c, const muestra_t *items, muestra_t *destino)
{
    int i, pos = c->inicio;

    for (i = 0; i < c->cuenta; i++) {
        destino[i] = items[pos];
        pos = (pos + 1) % c->capacidad;
    }
    return c->cuenta;
}
