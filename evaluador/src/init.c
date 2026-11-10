/*
 * init.c  (evaluator-init)
 * Crea la memoria compartida y los semáforos, arranca los hilos del
 * evaluador y se queda ocupando la consola hasta Ctrl+C.
 * Ver doc/diseno.md, secciones 3.1 y 6.
 */
#include "evaluador.h"
#include "args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile int senal_recibida = 0;

static void al_recibir_ctrl_c(void)
{
    senal_recibida = 1;
}

/* ---------- utilidades para los hilos ---------- */

int sigue_activo(evaluador_t *ev)
{
    return ev->sis->cab->activo;
}

void dormir_simulado(evaluador_t *ev, long ms)
{
    /* se duerme en pedacitos para poder salir rápido si se apaga */
    int64_t fin = reloj_ms() + ms / ev->escala;
    int64_t falta;

    while (sigue_activo(ev) && (falta = fin - reloj_ms()) > 0)
        dormir_ms(falta > 100 ? 100 : (long)falta);
}

void publicar_cola_interna(evaluador_t *ev, int tipo, int bandeja)
{
    sistema_t *sis = ev->sis;
    cola_interna_t *c = &ev->internas[tipo];
    tablero_t *tab = tablero_de(sis);

    semaforo_esperar(&sis->sem.tablero);
    tab->en_cola[tipo] = cola_copiar(&c->datos, c->items, tablero_cola(sis, tipo));
    if (bandeja >= 0)
        tab->despachando[bandeja] = 0;
    semaforo_avisar(&sis->sem.tablero);
}

void marcar_despachador(evaluador_t *ev, int bandeja, const muestra_t *m)
{
    sistema_t *sis = ev->sis;
    tablero_t *tab = tablero_de(sis);

    semaforo_esperar(&sis->sem.tablero);
    tab->despachando[bandeja] = 1;
    tab->en_mano[bandeja] = *m;
    semaforo_avisar(&sis->sem.tablero);
}

void marcar_analizador(evaluador_t *ev, int tipo, int ocupado, const muestra_t *m)
{
    sistema_t *sis = ev->sis;
    tablero_t *tab = tablero_de(sis);

    semaforo_esperar(&sis->sem.tablero);
    tab->ocupado[tipo] = ocupado;
    if (m)
        tab->actual[tipo] = *m;
    semaforo_avisar(&sis->sem.tablero);
}

void marcar_espera_robot(evaluador_t *ev, int tipo, int esperando)
{
    sistema_t *sis = ev->sis;

    semaforo_esperar(&sis->sem.tablero);
    tablero_de(sis)->esperando_robot[tipo] = esperando;
    semaforo_avisar(&sis->sem.tablero);
}

/* ---------- argumentos ---------- */

typedef struct {
    long entradas, tam_entrada, tam_salida, tam_interna, cap_robot;
    long reactivo[3];
    const char *nombre;
} config_t;

static void uso(void)
{
    fprintf(stderr,
        "uso: evaluator init [-i <entradas>] [-ie <tam_entrada>] [-oe <tam_salida>]\n"
        "                    [-n <nombre>] [-b <reactivo_B>] [-d <reactivo_D>]\n"
        "                    [-s <reactivo_S>] [-q <tam_interna>] [-ee <cap_robot>]\n");
}

static int leer_config(int argc, char **argv, config_t *cfg)
{
    struct { const char *op; long *dest; long min, max; } numericas[] = {
        { "-i",  &cfg->entradas,    1, MAX_ENTRADAS },
        { "-ie", &cfg->tam_entrada, 1, 1024 },
        { "-oe", &cfg->tam_salida,  1, 4096 },
        { "-b",  &cfg->reactivo[0], 0, REACTIVO_MAX },
        { "-d",  &cfg->reactivo[1], 0, REACTIVO_MAX },
        { "-s",  &cfg->reactivo[2], 0, REACTIVO_MAX },
        { "-q",  &cfg->tam_interna, 1, 1024 },
        { "-ee", &cfg->cap_robot,   1, REACTIVO_MAX },
    };
    int i, j, encontrada;

    cfg->entradas = 5;
    cfg->tam_entrada = 6;
    cfg->tam_salida = 10;
    cfg->tam_interna = 6;
    cfg->cap_robot = 50;
    cfg->reactivo[0] = cfg->reactivo[1] = cfg->reactivo[2] = REACTIVO_MAX;
    cfg->nombre = nombre_por_omision();

    for (i = 1; i < argc; i++) {
        if (i + 1 >= argc) {
            fprintf(stderr, "init: a la opcion %s le falta el valor\n", argv[i]);
            return -1;
        }
        if (strcmp(argv[i], "-n") == 0) {
            cfg->nombre = argv[++i];
            continue;
        }
        encontrada = 0;
        for (j = 0; j < (int)(sizeof(numericas) / sizeof(numericas[0])); j++) {
            if (strcmp(argv[i], numericas[j].op) == 0) {
                if (leer_entero(argv[i + 1], numericas[j].min, numericas[j].max,
                                numericas[j].dest) < 0) {
                    fprintf(stderr, "init: valor invalido para %s: '%s' (debe ir de %ld a %ld)\n",
                            argv[i], argv[i + 1], numericas[j].min, numericas[j].max);
                    return -1;
                }
                encontrada = 1;
                i++;
                break;
            }
        }
        if (!encontrada) {
            fprintf(stderr, "init: opcion desconocida '%s'\n", argv[i]);
            return -1;
        }
    }
    if (!nombre_valido(cfg->nombre)) {
        fprintf(stderr, "init: nombre invalido '%s' (hasta %d letras, numeros, _ o -)\n",
                cfg->nombre, MAX_NOMBRE);
        return -1;
    }
    return 0;
}

/* si quedó un segmento con ese nombre: si su evaluador sigue vivo no se
 * puede iniciar otro; si murió, se limpian los restos. */
static int revisar_existente(const char *nombre, const char *nom_seg)
{
    segmento_t viejo;
    cabecera_t *cab;
    int vivo, n = MAX_ENTRADAS;

    if (seg_abrir(&viejo, nom_seg) < 0)
        return 0;

    cab = (cabecera_t *)viejo.base;
    vivo = viejo.tam >= sizeof(cabecera_t) && cab->magia == MAGIA_EVAL &&
           cab->activo && proceso_vivo(cab->pid_evaluador);
    if (cab->magia == MAGIA_EVAL && cab->num_entradas > 0 && cab->num_entradas <= MAX_ENTRADAS)
        n = cab->num_entradas;
    seg_cerrar(&viejo);

    if (vivo) {
        fprintf(stderr, "init: ya hay un evaluador corriendo con el nombre '%s'\n", nombre);
        return -1;
    }
    fprintf(stderr, "init: se encontraron restos de una ejecucion anterior de '%s', se limpian\n", nombre);
    semaforos_borrar(nombre, n);
    seg_borrar(nom_seg);
    return 0;
}

/* ---------- apagado ---------- */

static void despertar_a_todos(evaluador_t *ev)
{
    sistema_t *sis = ev->sis;
    int k, t, j;

    /* despachadores */
    for (k = 0; k < sis->cab->num_entradas; k++)
        semaforo_avisar(&sis->sem.e_llenos[k]);
    /* procesos reg que estén esperando lugar en una bandeja */
    for (k = 0; k < sis->cab->num_entradas; k++)
        for (j = 0; j < sis->cab->tam_entrada + 16; j++)
            semaforo_avisar(&sis->sem.e_vacios[k]);
    /* colas internas */
    for (t = 0; t < 3; t++) {
        mutex_bloquear(&ev->internas[t].mutex);
        cond_avisar_todos(&ev->internas[t].hay_espacio);
        cond_avisar_todos(&ev->internas[t].hay_muestras);
        mutex_soltar(&ev->internas[t].mutex);
    }
    /* robot y analizadores que lo esperan */
    mutex_bloquear(&ev->robot_mutex);
    cond_avisar_todos(&ev->robot_hay_pedido);
    for (t = 0; t < 3; t++)
        cond_avisar_todos(&ev->robot_listo[t]);
    mutex_soltar(&ev->robot_mutex);
    /* analizadores esperando lugar en la salida */
    for (t = 0; t < 3; t++)
        semaforo_avisar(&sis->sem.s_vacios);
    /* procesos rep esperando resultados */
    for (j = 0; j < 64; j++)
        semaforo_avisar(&sis->sem.s_llenos);
}

static int contar_pendientes(evaluador_t *ev)
{
    sistema_t *sis = ev->sis;
    tablero_t *tab = tablero_de(sis);
    int k, t, total = 0;

    for (k = 0; k < sis->cab->num_entradas; k++)
        total += cola_entrada(sis, k)->cuenta + (tab->despachando[k] ? 1 : 0);
    for (t = 0; t < 3; t++)
        total += ev->internas[t].datos.cuenta + (tab->ocupado[t] ? 1 : 0);
    return total;
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
    config_t cfg;
    sistema_t sis;
    evaluador_t ev;
    cabecera_t plantilla;
    char nom_seg[96];
    hilo_t hilos[MAX_ENTRADAS + 4];
    arg_hilo_t args[MAX_ENTRADAS + 4];
    int n_hilos = 0, k, t, pendientes;
    uint32_t tam;

    if (leer_config(argc, argv, &cfg) < 0) {
        uso();
        return 2;
    }
    nombre_segmento(nom_seg, sizeof(nom_seg), cfg.nombre);
    if (revisar_existente(cfg.nombre, nom_seg) < 0)
        return 1;

    /* tamaño del segmento */
    memset(&plantilla, 0, sizeof(plantilla));
    plantilla.num_entradas = (int32_t)cfg.entradas;
    plantilla.tam_entrada = (int32_t)cfg.tam_entrada;
    plantilla.tam_salida = (int32_t)cfg.tam_salida;
    plantilla.tam_interna = (int32_t)cfg.tam_interna;
    tam = calcular_distribucion(&plantilla);

    memset(&sis, 0, sizeof(sis));
    snprintf(sis.nombre, sizeof(sis.nombre), "%s", cfg.nombre);
    /* por si quedaron semáforos sueltos sin segmento */
    semaforos_borrar(cfg.nombre, MAX_ENTRADAS);
    if (seg_crear(&sis.seg, nom_seg, tam) < 0) {
        fprintf(stderr, "init: no se pudo crear la memoria compartida '%s'\n", cfg.nombre);
        return 1;
    }

    /* cabecera */
    sis.cab = (cabecera_t *)sis.seg.base;
    *sis.cab = plantilla;
    sis.cab->magia = MAGIA_EVAL;
    sis.cab->activo = 0;
    sis.cab->pid_evaluador = pid_actual();
    snprintf(sis.cab->nombre, sizeof(sis.cab->nombre), "%s", cfg.nombre);
    sis.cab->cap_robot = (int32_t)cfg.cap_robot;
    sis.cab->siguiente_id = 1;
    for (t = 0; t < 3; t++)
        sis.cab->reactivo[t] = (int32_t)cfg.reactivo[t];

    /* colas de la memoria compartida */
    {
        uint32_t desp = sis.cab->desp_entradas
                      + (((uint32_t)cfg.entradas * sizeof(cola_t) + 7u) & ~7u);
        for (k = 0; k < cfg.entradas; k++) {
            cola_preparar(cola_entrada(&sis, k), (int)cfg.tam_entrada, desp);
            desp += (uint32_t)cfg.tam_entrada * sizeof(muestra_t);
        }
        cola_preparar(cola_salida(&sis), (int)cfg.tam_salida,
                      sis.cab->desp_salida + ((sizeof(cola_t) + 7u) & ~7u));
        desp = sis.cab->desp_tablero + ((sizeof(tablero_t) + 7u) & ~7u);
        for (t = 0; t < 3; t++) {
            tablero_de(&sis)->desp_cola[t] = desp;
            desp += (uint32_t)cfg.tam_interna * sizeof(muestra_t);
        }
    }

    if (semaforos_crear(&sis.sem, cfg.nombre, sis.cab) < 0) {
        fprintf(stderr, "init: no se pudieron crear los semaforos\n");
        semaforos_cerrar(&sis.sem);
        semaforos_borrar(cfg.nombre, (int)cfg.entradas);
        seg_cerrar(&sis.seg);
        seg_borrar(nom_seg);
        return 1;
    }

    /* estado interno del evaluador */
    memset(&ev, 0, sizeof(ev));
    ev.sis = &sis;
    ev.escala = escala_tiempo();
    ev.semilla = semilla_inicial();
    for (t = 0; t < 3; t++) {
        cola_interna_t *c = &ev.internas[t];
        c->items = calloc((size_t)cfg.tam_interna, sizeof(muestra_t));
        if (!c->items) {
            fprintf(stderr, "init: sin memoria\n");
            return 1;
        }
        cola_preparar(&c->datos, (int)cfg.tam_interna, 0);
        mutex_iniciar(&c->mutex);
        cond_iniciar(&c->hay_espacio);
        cond_iniciar(&c->hay_muestras);
        cond_iniciar(&ev.robot_listo[t]);
    }
    mutex_iniciar(&ev.robot_mutex);
    cond_iniciar(&ev.robot_hay_pedido);

    /* todo listo: ya se puede usar */
    sis.cab->activo = 1;
    al_interrumpir(al_recibir_ctrl_c);

    for (k = 0; k < cfg.entradas; k++) {
        args[n_hilos].ev = &ev;
        args[n_hilos].numero = k;
        hilo_crear(&hilos[n_hilos], hilo_despachador, &args[n_hilos]);
        n_hilos++;
    }
    for (t = 0; t < 3; t++) {
        args[n_hilos].ev = &ev;
        args[n_hilos].numero = t;
        hilo_crear(&hilos[n_hilos], hilo_analizador, &args[n_hilos]);
        n_hilos++;
    }
    args[n_hilos].ev = &ev;
    args[n_hilos].numero = 0;
    hilo_crear(&hilos[n_hilos], hilo_robot, &args[n_hilos]);
    n_hilos++;

    printf("Evaluador '%s' iniciado (pid %d)\n", cfg.nombre, pid_actual());
    printf("  bandejas de entrada: %ld de %ld posiciones\n", cfg.entradas, cfg.tam_entrada);
    printf("  bandeja de salida:   %ld posiciones\n", cfg.tam_salida);
    printf("  colas internas:      %ld posiciones\n", cfg.tam_interna);
    printf("  reactivos:           B %ld  D %ld  S %ld (robot lleva %ld)\n",
           cfg.reactivo[0], cfg.reactivo[1], cfg.reactivo[2], cfg.cap_robot);
    if (ev.escala > 1)
        printf("  tiempos divididos por %ld (EVALUATOR_SCALE)\n", ev.escala);
    printf("Ctrl+C para terminar.\n");
    fflush(stdout);

    while (!senal_recibida)
        dormir_ms(100);

    /* apagado ordenado (sección 6.6) */
    printf("\nApagando el evaluador...\n");
    fflush(stdout);
    sis.cab->activo = 0;
    despertar_a_todos(&ev);
    for (k = 0; k < n_hilos; k++)
        hilo_esperar(hilos[k]);

    pendientes = contar_pendientes(&ev);
    if (pendientes > 0)
        fprintf(stderr, "init: quedaron %d muestra(s) sin procesar\n", pendientes);

    for (t = 0; t < 3; t++) {
        free(ev.internas[t].items);
        mutex_destruir(&ev.internas[t].mutex);
        cond_destruir(&ev.internas[t].hay_espacio);
        cond_destruir(&ev.internas[t].hay_muestras);
        cond_destruir(&ev.robot_listo[t]);
    }
    mutex_destruir(&ev.robot_mutex);
    cond_destruir(&ev.robot_hay_pedido);

    semaforos_cerrar(&sis.sem);
    semaforos_borrar(cfg.nombre, (int)cfg.entradas);
    seg_cerrar(&sis.seg);
    seg_borrar(nom_seg);
    printf("Listo.\n");
    return 0;
}
