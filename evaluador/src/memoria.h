/*
 * memoria.h
 * Estructuras que viven en la memoria compartida (ver doc/diseno.md,
 * sección 4) y funciones para crear, abrir y ubicar cada parte.
 */
#ifndef MEMORIA_H
#define MEMORIA_H

#include <stdint.h>
#include "plataforma.h"

#define MAGIA_EVAL    0x45564131u   /* "EVA1": marca que el segmento es nuestro */
#define MAX_ENTRADAS  32
#define MAX_NOMBRE    16
#define REACTIVO_MAX  100

#define TIPO_SANGRE   0   /* 'B' */
#define TIPO_DETRITOS 1   /* 'D' */
#define TIPO_PIEL     2   /* 'S' */

typedef struct {
    uint64_t id;          /* identificador único */
    int32_t  bandeja;     /* cola de entrada por donde llegó */
    int32_t  tipo;        /* TIPO_SANGRE, TIPO_DETRITOS o TIPO_PIEL */
    int32_t  cantidad;    /* 1..5, también es lo que gasta de reactivo */
    int32_t  t_proceso;   /* segundos; se asigna al empezar a analizarla */
    char     resultado;   /* 'P', 'N', '?' o 0 mientras no hay resultado */
    int64_t  llegada;     /* hora en que entró a la bandeja (epoch, segundos) */
} muestra_t;

typedef struct {
    int32_t  capacidad;   /* ie para las de entrada, oe para la salida */
    int32_t  inicio;      /* posición del próximo elemento a sacar */
    int32_t  fin;         /* posición donde se pone el próximo elemento */
    int32_t  cuenta;      /* elementos ocupados; solo para ctrl y validaciones */
    uint32_t desp_items;  /* offset del arreglo muestra_t[capacidad] */
} cola_t;

typedef struct {
    uint32_t magia;
    volatile int32_t activo;   /* 1 mientras el sistema funciona */
    int32_t  pid_evaluador;    /* proceso de init, para saber si sigue vivo */
    char     nombre[MAX_NOMBRE + 1];

    /* configuración que dio init */
    int32_t  num_entradas;    /* -i  */
    int32_t  tam_entrada;     /* -ie */
    int32_t  tam_salida;      /* -oe */
    int32_t  tam_interna;     /* -q  */
    int32_t  cap_robot;       /* -ee */

    uint64_t siguiente_id;    /* protegido por el semáforo "id" */
    int32_t  reactivo[3];     /* niveles actuales; semáforo "reactivo" */

    uint32_t tam_total;
    uint32_t desp_entradas;   /* cola_t[num_entradas] */
    uint32_t desp_salida;     /* cola_t */
    uint32_t desp_tablero;    /* tablero_t */
} cabecera_t;

/* copia de lo que pasa dentro del evaluador, solo para que ctrl lo muestre */
typedef struct {
    int32_t   en_cola[3];         /* cuántas muestras hay en cada cola interna */
    uint32_t  desp_cola[3];       /* offset de muestra_t[tam_interna] por tipo */
    int32_t   ocupado[3];         /* 1 si el analizador tiene una muestra */
    muestra_t actual[3];          /* la muestra que procesa cada analizador */
    int32_t   esperando_robot[3]; /* 1 si el analizador está parado sin reactivo */
    /* muestra que un despachador ya sacó de su bandeja pero todavía no ha
     * podido meter en la cola interna (porque está llena o no es su turno) */
    int32_t   despachando[MAX_ENTRADAS];
    muestra_t en_mano[MAX_ENTRADAS];
} tablero_t;

/* los semáforos con nombre que usa todo el sistema (sección 5.1) */
typedef struct {
    int        n;                  /* número de bandejas */
    semaforo_t id;
    semaforo_t reactivo;
    semaforo_t tablero;
    semaforo_t s_mutex, s_vacios, s_llenos;
    semaforo_t e_mutex[MAX_ENTRADAS];
    semaforo_t e_vacios[MAX_ENTRADAS];
    semaforo_t e_llenos[MAX_ENTRADAS];
} semaforos_t;

/* lo que tiene abierto cada proceso para hablar con el sistema */
typedef struct {
    segmento_t  seg;
    cabecera_t *cab;
    semaforos_t sem;
    char        nombre[MAX_NOMBRE + 1];
} sistema_t;

/* ubicar cada parte dentro del segmento */
cola_t    *cola_entrada(sistema_t *s, int k);
cola_t    *cola_salida(sistema_t *s);
tablero_t *tablero_de(sistema_t *s);
muestra_t *items_de(sistema_t *s, cola_t *c);
muestra_t *tablero_cola(sistema_t *s, int tipo);

/* llena los desp_* de la cabecera y devuelve el tamaño total */
uint32_t calcular_distribucion(cabecera_t *cab);

/* nombre de la memoria compartida dentro del sistema operativo */
void nombre_segmento(char *buf, size_t n, const char *nombre);

int  semaforos_crear(semaforos_t *s, const char *nombre, const cabecera_t *cab);
int  semaforos_abrir(semaforos_t *s, const char *nombre, int n);
void semaforos_cerrar(semaforos_t *s);
void semaforos_borrar(const char *nombre, int n);

/* abre un sistema ya iniciado. Devuelve 0 si se pudo, 3 si no está iniciado */
int  conectar(sistema_t *s, const char *nombre);
void desconectar(sistema_t *s);

/* utilidades sobre tipos y resultados */
int         tipo_desde_letra(char c);   /* -1 si la letra no es B, D o S */
char        letra_tipo(int tipo);
const char *resultado_largo(char r);    /* positive, negative o - */

#endif
