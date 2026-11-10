/*
 * plataforma.h
 *
 * Todo lo que depende del sistema operativo pasa por aquí.
 * plataforma_posix.c lo implementa para Linux (y macOS) y
 * plataforma_win.c para Windows. El resto del código solo usa
 * estas funciones y no sabe en qué sistema está corriendo.
 */
#ifndef PLATAFORMA_H
#define PLATAFORMA_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600   /* hace falta para CONDITION_VARIABLE */
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

/* ---------- memoria compartida ---------- */

typedef struct {
    void  *base;      /* donde quedó mapeado el segmento en este proceso */
    size_t tam;
#ifdef _WIN32
    HANDLE mapa;
#else
    int    fd;
#endif
} segmento_t;

/* 0 si todo bien, -1 si falló. seg_crear falla si ya existe. */
int  seg_crear(segmento_t *s, const char *nombre, size_t tam);
int  seg_abrir(segmento_t *s, const char *nombre);
void seg_cerrar(segmento_t *s);
void seg_borrar(const char *nombre);

/* ---------- semáforos con nombre ---------- */

typedef struct {
#ifdef _WIN32
    HANDLE h;
#else
    sem_t *s;
#endif
} semaforo_t;

int  semaforo_crear(semaforo_t *s, const char *nombre, unsigned valor);
int  semaforo_abrir(semaforo_t *s, const char *nombre);
void semaforo_esperar(semaforo_t *s);
/* espera como mucho ms milisegundos. 0 = lo consiguió, 1 = se acabó el tiempo */
int  semaforo_esperar_ms(semaforo_t *s, long ms);
void semaforo_avisar(semaforo_t *s);
void semaforo_cerrar(semaforo_t *s);
void semaforo_borrar(const char *nombre);

/* arma el nombre completo de un recurso, p.ej. "/evaluator.e2.mutex"
 * en Linux o "Local\evaluator.e2.mutex" en Windows */
void nombre_recurso(char *buf, size_t n, const char *base, const char *sufijo);

/* ---------- hilos, mutex y variables de condición ---------- */

typedef void *(*funcion_hilo)(void *);

#ifdef _WIN32
typedef HANDLE             hilo_t;
typedef CRITICAL_SECTION   mutex_t;
typedef CONDITION_VARIABLE cond_t;
#else
typedef pthread_t       hilo_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t  cond_t;
#endif

int  hilo_crear(hilo_t *h, funcion_hilo f, void *arg);
void hilo_esperar(hilo_t h);

void mutex_iniciar(mutex_t *m);
void mutex_bloquear(mutex_t *m);
void mutex_soltar(mutex_t *m);
void mutex_destruir(mutex_t *m);

void cond_iniciar(cond_t *c);
void cond_esperar(cond_t *c, mutex_t *m);
void cond_avisar(cond_t *c);
void cond_avisar_todos(cond_t *c);
void cond_destruir(cond_t *c);

/* ---------- varios ---------- */

void    dormir_ms(long ms);
int64_t reloj_ms(void);          /* reloj que solo avanza, para medir esperas */
int     pid_actual(void);
int     proceso_vivo(int pid);
int     entrada_es_terminal(void);

/* llama a f() cuando el usuario presiona Ctrl+C (o llega SIGTERM).
 * f solo debe cambiar una bandera. */
void    al_interrumpir(void (*f)(void));

/* ruta completa del ejecutable actual. 0 si la encontró */
int     ruta_ejecutable(char *buf, size_t n, const char *argv0);
int     existe_archivo(const char *ruta);

/* reemplaza (Linux) o lanza y espera (Windows) el programa real.
 * Devuelve el código de salida; solo regresa en Linux si hubo error. */
int     lanzar_programa(const char *ruta, char **argv);

#ifdef _WIN32
#define SEPARADOR '\\'
#define EXTENSION_EXE ".exe"
#else
#define SEPARADOR '/'
#define EXTENSION_EXE ""
#endif

#endif
