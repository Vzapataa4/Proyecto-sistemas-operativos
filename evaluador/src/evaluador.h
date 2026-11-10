/*
 * evaluador.h
 * Lo que comparten los hilos del proceso evaluador (el que arranca init).
 * Ver doc/diseno.md, sección 6.
 */
#ifndef EVALUADOR_H
#define EVALUADOR_H

#include "memoria.h"
#include "cola.h"

#define TIEMPO_VIAJE_MS  3000   /* el robot va al almacén y vuelve */
#define TIEMPO_CARGA_MS  1000   /* el robot carga el reactivo en el analizador */

/* cola interna: privada del proceso, solo la usan los hilos */
typedef struct {
    cola_t     datos;
    muestra_t *items;
    uint64_t   turno_siguiente;  /* próximo número que se reparte */
    uint64_t   turno_actual;     /* número al que le toca insertar */
    mutex_t    mutex;
    cond_t     hay_espacio;
    cond_t     hay_muestras;
} cola_interna_t;

typedef struct {
    sistema_t     *sis;
    cola_interna_t internas[3];

    /* robot: una filita de pedidos (máximo uno por tipo) */
    mutex_t robot_mutex;
    cond_t  robot_hay_pedido;
    cond_t  robot_listo[3];
    int     pedidos[3];
    int     n_pedidos;
    int     terminado[3];

    long     escala;
    unsigned semilla;
} evaluador_t;

typedef struct {
    evaluador_t *ev;
    int          numero;   /* bandeja (despachador) o tipo (analizador) */
} arg_hilo_t;

void *hilo_despachador(void *arg);
void *hilo_analizador(void *arg);
void *hilo_robot(void *arg);

/* lo llama un analizador sin reactivo; se bloquea hasta que el robot termine */
void pedir_robot(evaluador_t *ev, int tipo);

/* utilidades que están en init.c */
int  sigue_activo(evaluador_t *ev);
void dormir_simulado(evaluador_t *ev, long ms);
/* copia la cola interna al tablero; hay que tener el mutex de esa cola.
 * Si bandeja >= 0, además marca que ese despachador ya no tiene muestra
 * en la mano (todo en el mismo paso, para que ctrl no la vea dos veces) */
void publicar_cola_interna(evaluador_t *ev, int tipo, int bandeja);
void marcar_despachador(evaluador_t *ev, int bandeja, const muestra_t *m);
void marcar_analizador(evaluador_t *ev, int tipo, int ocupado, const muestra_t *m);
void marcar_espera_robot(evaluador_t *ev, int tipo, int esperando);

#endif
