/*
 * cola.h
 * Cola circular. Se usa igual para las bandejas de la memoria compartida
 * y para las colas internas del evaluador; solo cambia dónde están las
 * casillas (items).
 *
 * Estas funciones NO bloquean nada: quien las llama ya debe tener el
 * mutex que corresponda.
 */
#ifndef COLA_H
#define COLA_H

#include "memoria.h"

void cola_preparar(cola_t *c, int capacidad, uint32_t desp_items);
void cola_meter(cola_t *c, muestra_t *items, const muestra_t *m);
void cola_sacar(cola_t *c, muestra_t *items, muestra_t *m);

/* copia el contenido en orden (del más viejo al más nuevo) y
 * devuelve cuántos copió */
int  cola_copiar(const cola_t *c, const muestra_t *items, muestra_t *destino);

#endif
