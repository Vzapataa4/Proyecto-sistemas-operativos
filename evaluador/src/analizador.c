/*
 * analizador.c
 * Un hilo por tipo de muestra. Saca de su cola interna, gasta reactivo,
 * "procesa" la muestra y deja el resultado en la bandeja de salida
 * (sección 6.3 del diseño).
 */
#include "evaluador.h"
#include "args.h"

#include <stdio.h>

/* tiempos de procesamiento en segundos (enunciado, 3.2.7) */
static const int tiempo_min[3] = { 1, 5, 8 };
static const int tiempo_max[3] = { 7, 20, 25 };

/* 50 % positivo, 35 % negativo, 15 % no concluyente (enunciado, 3.2.8) */
static char sortear_resultado(unsigned *estado)
{
    int n = azar_entre(estado, 0, 99);
    if (n < 50)
        return 'P';
    if (n < 85)
        return 'N';
    return '?';
}

/* revisa y descuenta el reactivo en el mismo paso. 1 si alcanzó */
static int tomar_reactivo(sistema_t *sis, int tipo, int cantidad)
{
    int alcanzo = 0;

    semaforo_esperar(&sis->sem.reactivo);
    if (sis->cab->reactivo[tipo] >= cantidad) {
        sis->cab->reactivo[tipo] -= cantidad;
        alcanzo = 1;
    }
    semaforo_avisar(&sis->sem.reactivo);
    return alcanzo;
}

void *hilo_analizador(void *arg)
{
    arg_hilo_t *a = (arg_hilo_t *)arg;
    evaluador_t *ev = a->ev;
    sistema_t *sis = ev->sis;
    int t = a->numero;
    cola_interna_t *c = &ev->internas[t];
    cola_t *salida = cola_salida(sis);
    unsigned estado;
    muestra_t m;

    for (;;) {
        /* 1. tomar la siguiente muestra de la cola interna */
        mutex_bloquear(&c->mutex);
        while (sigue_activo(ev) && c->datos.cuenta == 0)
            cond_esperar(&c->hay_muestras, &c->mutex);
        if (!sigue_activo(ev)) {
            mutex_soltar(&c->mutex);
            break;
        }
        cola_sacar(&c->datos, c->items, &m);
        publicar_cola_interna(ev, t, -1);
        cond_avisar_todos(&c->hay_espacio);
        mutex_soltar(&c->mutex);

        /* 2. asegurar el reactivo; si no hay, llamar al robot */
        marcar_analizador(ev, t, 1, &m);
        while (sigue_activo(ev) && !tomar_reactivo(sis, t, m.cantidad))
            pedir_robot(ev, t);
        if (!sigue_activo(ev))
            break;

        /* 3. procesar. El azar depende de la semilla y del id de la muestra,
         * así con la misma semilla cada muestra sale igual sin importar
         * en qué orden llegue */
        estado = (ev->semilla ^ 0x9E3779B9u) + (unsigned)m.id * 2654435761u;
        azar(&estado);
        m.t_proceso = azar_entre(&estado, tiempo_min[t], tiempo_max[t]);
        marcar_analizador(ev, t, 1, &m);
        dormir_simulado(ev, (long)m.t_proceso * 1000);
        if (!sigue_activo(ev))
            break;
        m.resultado = sortear_resultado(&estado);

        /* 4. dejar el resultado en la salida (si está llena, espera) */
        semaforo_esperar(&sis->sem.s_vacios);
        if (!sigue_activo(ev))
            break;
        semaforo_esperar(&sis->sem.s_mutex);
        cola_meter(salida, items_de(sis, salida), &m);
        semaforo_avisar(&sis->sem.s_mutex);
        semaforo_avisar(&sis->sem.s_llenos);

        marcar_analizador(ev, t, 0, NULL);
    }
    return NULL;
}
