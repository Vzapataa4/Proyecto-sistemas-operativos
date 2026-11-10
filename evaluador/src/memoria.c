/*
 * memoria.c
 * Crear, abrir y ubicar las estructuras dentro de la memoria compartida.
 */
#include "memoria.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* redondea a múltiplos de 8 para que todo quede bien alineado */
static uint32_t alinear(uint32_t x)
{
    return (x + 7u) & ~7u;
}

cola_t *cola_entrada(sistema_t *s, int k)
{
    char *base = (char *)s->seg.base;
    return (cola_t *)(base + s->cab->desp_entradas) + k;
}

cola_t *cola_salida(sistema_t *s)
{
    return (cola_t *)((char *)s->seg.base + s->cab->desp_salida);
}

tablero_t *tablero_de(sistema_t *s)
{
    return (tablero_t *)((char *)s->seg.base + s->cab->desp_tablero);
}

muestra_t *items_de(sistema_t *s, cola_t *c)
{
    return (muestra_t *)((char *)s->seg.base + c->desp_items);
}

muestra_t *tablero_cola(sistema_t *s, int tipo)
{
    return (muestra_t *)((char *)s->seg.base + tablero_de(s)->desp_cola[tipo]);
}

uint32_t calcular_distribucion(cabecera_t *cab)
{
    uint32_t pos = alinear(sizeof(cabecera_t));
    uint32_t i = (uint32_t)cab->num_entradas;
    uint32_t ie = (uint32_t)cab->tam_entrada;
    uint32_t oe = (uint32_t)cab->tam_salida;
    uint32_t q = (uint32_t)cab->tam_interna;

    cab->desp_entradas = pos;
    pos += alinear(i * sizeof(cola_t));
    pos += alinear(i * ie * sizeof(muestra_t));

    cab->desp_salida = pos;
    pos += alinear(sizeof(cola_t));
    pos += alinear(oe * sizeof(muestra_t));

    cab->desp_tablero = pos;
    pos += alinear(sizeof(tablero_t));
    pos += alinear(3 * q * sizeof(muestra_t));

    cab->tam_total = pos;
    return pos;
}

void nombre_segmento(char *buf, size_t n, const char *nombre)
{
    nombre_recurso(buf, n, nombre, "shm");
}

/* ---------- semáforos ---------- */

static int crear_uno(semaforo_t *s, const char *base, const char *suf, unsigned v)
{
    char nom[96];
    nombre_recurso(nom, sizeof(nom), base, suf);
    return semaforo_crear(s, nom, v);
}

static int abrir_uno(semaforo_t *s, const char *base, const char *suf)
{
    char nom[96];
    nombre_recurso(nom, sizeof(nom), base, suf);
    return semaforo_abrir(s, nom);
}

static void borrar_uno(const char *base, const char *suf)
{
    char nom[96];
    nombre_recurso(nom, sizeof(nom), base, suf);
    semaforo_borrar(nom);
}

int semaforos_crear(semaforos_t *s, const char *nombre, const cabecera_t *cab)
{
    char suf[32];
    int k, err = 0;

    memset(s, 0, sizeof(*s));
    s->n = cab->num_entradas;
    err |= crear_uno(&s->id, nombre, "id", 1);
    err |= crear_uno(&s->reactivo, nombre, "reactivo", 1);
    err |= crear_uno(&s->tablero, nombre, "tablero", 1);
    err |= crear_uno(&s->s_mutex, nombre, "s.mutex", 1);
    err |= crear_uno(&s->s_vacios, nombre, "s.vacios", (unsigned)cab->tam_salida);
    err |= crear_uno(&s->s_llenos, nombre, "s.llenos", 0);
    for (k = 0; k < s->n && !err; k++) {
        snprintf(suf, sizeof(suf), "e%d.mutex", k);
        err |= crear_uno(&s->e_mutex[k], nombre, suf, 1);
        snprintf(suf, sizeof(suf), "e%d.vacios", k);
        err |= crear_uno(&s->e_vacios[k], nombre, suf, (unsigned)cab->tam_entrada);
        snprintf(suf, sizeof(suf), "e%d.llenos", k);
        err |= crear_uno(&s->e_llenos[k], nombre, suf, 0);
    }
    return err ? -1 : 0;
}

int semaforos_abrir(semaforos_t *s, const char *nombre, int n)
{
    char suf[32];
    int k, err = 0;

    memset(s, 0, sizeof(*s));
    s->n = n;
    err |= abrir_uno(&s->id, nombre, "id");
    err |= abrir_uno(&s->reactivo, nombre, "reactivo");
    err |= abrir_uno(&s->tablero, nombre, "tablero");
    err |= abrir_uno(&s->s_mutex, nombre, "s.mutex");
    err |= abrir_uno(&s->s_vacios, nombre, "s.vacios");
    err |= abrir_uno(&s->s_llenos, nombre, "s.llenos");
    for (k = 0; k < n && !err; k++) {
        snprintf(suf, sizeof(suf), "e%d.mutex", k);
        err |= abrir_uno(&s->e_mutex[k], nombre, suf);
        snprintf(suf, sizeof(suf), "e%d.vacios", k);
        err |= abrir_uno(&s->e_vacios[k], nombre, suf);
        snprintf(suf, sizeof(suf), "e%d.llenos", k);
        err |= abrir_uno(&s->e_llenos[k], nombre, suf);
    }
    return err ? -1 : 0;
}

void semaforos_cerrar(semaforos_t *s)
{
    int k;
    semaforo_cerrar(&s->id);
    semaforo_cerrar(&s->reactivo);
    semaforo_cerrar(&s->tablero);
    semaforo_cerrar(&s->s_mutex);
    semaforo_cerrar(&s->s_vacios);
    semaforo_cerrar(&s->s_llenos);
    for (k = 0; k < s->n; k++) {
        semaforo_cerrar(&s->e_mutex[k]);
        semaforo_cerrar(&s->e_vacios[k]);
        semaforo_cerrar(&s->e_llenos[k]);
    }
}

void semaforos_borrar(const char *nombre, int n)
{
    char suf[32];
    int k;
    borrar_uno(nombre, "id");
    borrar_uno(nombre, "reactivo");
    borrar_uno(nombre, "tablero");
    borrar_uno(nombre, "s.mutex");
    borrar_uno(nombre, "s.vacios");
    borrar_uno(nombre, "s.llenos");
    for (k = 0; k < n; k++) {
        snprintf(suf, sizeof(suf), "e%d.mutex", k);
        borrar_uno(nombre, suf);
        snprintf(suf, sizeof(suf), "e%d.vacios", k);
        borrar_uno(nombre, suf);
        snprintf(suf, sizeof(suf), "e%d.llenos", k);
        borrar_uno(nombre, suf);
    }
}

/* ---------- conectarse a un sistema ya iniciado ---------- */

int conectar(sistema_t *s, const char *nombre)
{
    char nom[96];

    memset(s, 0, sizeof(*s));
    snprintf(s->nombre, sizeof(s->nombre), "%s", nombre);
    nombre_segmento(nom, sizeof(nom), nombre);

    if (seg_abrir(&s->seg, nom) < 0)
        return 3;
    s->cab = (cabecera_t *)s->seg.base;
    /* si el evaluador murió sin limpiar, el segmento sigue ahí pero no sirve */
    if (s->cab->magia != MAGIA_EVAL || !s->cab->activo ||
        !proceso_vivo(s->cab->pid_evaluador)) {
        seg_cerrar(&s->seg);
        return 3;
    }
    if (semaforos_abrir(&s->sem, nombre, s->cab->num_entradas) < 0) {
        semaforos_cerrar(&s->sem);
        seg_cerrar(&s->seg);
        return 3;
    }
    return 0;
}

void desconectar(sistema_t *s)
{
    semaforos_cerrar(&s->sem);
    seg_cerrar(&s->seg);
}

/* ---------- utilidades ---------- */

int tipo_desde_letra(char c)
{
    switch (toupper((unsigned char)c)) {
    case 'B': return TIPO_SANGRE;
    case 'D': return TIPO_DETRITOS;
    case 'S': return TIPO_PIEL;
    default:  return -1;
    }
}

char letra_tipo(int tipo)
{
    static const char letras[3] = { 'B', 'D', 'S' };
    return (tipo >= 0 && tipo < 3) ? letras[tipo] : '?';
}

const char *resultado_largo(char r)
{
    if (r == 'P')
        return "positive";
    if (r == 'N')
        return "negative";
    return "-";
}
