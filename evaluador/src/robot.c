/*
 * robot.c
 * Hay un solo robot. Atiende los pedidos de reactivo en orden de llegada
 * (sección 6.4 del diseño). Mientras carga, el analizador que pidió
 * se queda quieto esperando.
 */
#include "evaluador.h"

void pedir_robot(evaluador_t *ev, int tipo)
{
    marcar_espera_robot(ev, tipo, 1);

    mutex_bloquear(&ev->robot_mutex);
    ev->pedidos[ev->n_pedidos++] = tipo;
    cond_avisar(&ev->robot_hay_pedido);
    while (!ev->terminado[tipo] && sigue_activo(ev))
        cond_esperar(&ev->robot_listo[tipo], &ev->robot_mutex);
    ev->terminado[tipo] = 0;
    mutex_soltar(&ev->robot_mutex);

    marcar_espera_robot(ev, tipo, 0);
}

void *hilo_robot(void *arg)
{
    evaluador_t *ev = ((arg_hilo_t *)arg)->ev;
    sistema_t *sis = ev->sis;
    int t, i, nivel;

    mutex_bloquear(&ev->robot_mutex);
    for (;;) {
        while (ev->n_pedidos == 0 && sigue_activo(ev))
            cond_esperar(&ev->robot_hay_pedido, &ev->robot_mutex);
        if (!sigue_activo(ev))
            break;

        /* sacar el pedido más viejo */
        t = ev->pedidos[0];
        for (i = 1; i < ev->n_pedidos; i++)
            ev->pedidos[i - 1] = ev->pedidos[i];
        ev->n_pedidos--;
        mutex_soltar(&ev->robot_mutex);

        /* ir al almacén y volver */
        dormir_simulado(ev, TIEMPO_VIAJE_MS);

        semaforo_esperar(&sis->sem.reactivo);
        nivel = sis->cab->reactivo[t] + sis->cab->cap_robot;
        sis->cab->reactivo[t] = nivel > REACTIVO_MAX ? REACTIVO_MAX : nivel;
        semaforo_avisar(&sis->sem.reactivo);

        /* cargarlo en el analizador */
        dormir_simulado(ev, TIEMPO_CARGA_MS);

        mutex_bloquear(&ev->robot_mutex);
        ev->terminado[t] = 1;
        cond_avisar(&ev->robot_listo[t]);
    }
    mutex_soltar(&ev->robot_mutex);
    return NULL;
}
