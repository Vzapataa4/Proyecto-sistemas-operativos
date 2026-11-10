/*
 * plataforma_posix.c
 * Versión de la capa de plataforma para Linux (también compila en macOS,
 * que es donde a veces probamos).
 */
#define _GNU_SOURCE
#include "plataforma.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

/* ---------- memoria compartida ---------- */

int seg_crear(segmento_t *s, const char *nombre, size_t tam)
{
    s->fd = shm_open(nombre, O_CREAT | O_EXCL | O_RDWR, 0660);
    if (s->fd < 0)
        return -1;
    if (ftruncate(s->fd, (off_t)tam) < 0) {
        close(s->fd);
        shm_unlink(nombre);
        return -1;
    }
    s->base = mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, s->fd, 0);
    if (s->base == MAP_FAILED) {
        close(s->fd);
        shm_unlink(nombre);
        return -1;
    }
    s->tam = tam;
    memset(s->base, 0, tam);
    return 0;
}

int seg_abrir(segmento_t *s, const char *nombre)
{
    struct stat st;

    s->fd = shm_open(nombre, O_RDWR, 0);
    if (s->fd < 0)
        return -1;
    if (fstat(s->fd, &st) < 0 || st.st_size <= 0) {
        close(s->fd);
        return -1;
    }
    s->tam = (size_t)st.st_size;
    s->base = mmap(NULL, s->tam, PROT_READ | PROT_WRITE, MAP_SHARED, s->fd, 0);
    if (s->base == MAP_FAILED) {
        close(s->fd);
        return -1;
    }
    return 0;
}

void seg_cerrar(segmento_t *s)
{
    if (s->base && s->base != MAP_FAILED)
        munmap(s->base, s->tam);
    if (s->fd >= 0)
        close(s->fd);
    s->base = NULL;
    s->fd = -1;
}

void seg_borrar(const char *nombre)
{
    shm_unlink(nombre);
}

/* ---------- semáforos con nombre ---------- */

int semaforo_crear(semaforo_t *s, const char *nombre, unsigned valor)
{
    s->s = sem_open(nombre, O_CREAT | O_EXCL, 0660, valor);
    return s->s == SEM_FAILED ? -1 : 0;
}

int semaforo_abrir(semaforo_t *s, const char *nombre)
{
    s->s = sem_open(nombre, 0);
    return s->s == SEM_FAILED ? -1 : 0;
}

void semaforo_esperar(semaforo_t *s)
{
    /* si una señal interrumpe la espera, se vuelve a intentar */
    while (sem_wait(s->s) < 0 && errno == EINTR)
        ;
}

int semaforo_esperar_ms(semaforo_t *s, long ms)
{
#ifdef __APPLE__
    /* macOS no tiene sem_timedwait, así que se prueba cada 10 ms */
    int64_t fin = reloj_ms() + ms;
    while (sem_trywait(s->s) < 0) {
        if (reloj_ms() >= fin)
            return 1;
        dormir_ms(10);
    }
    return 0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    while (sem_timedwait(s->s, &ts) < 0) {
        if (errno == ETIMEDOUT)
            return 1;
        if (errno != EINTR)
            return 1;
    }
    return 0;
#endif
}

void semaforo_avisar(semaforo_t *s)
{
    sem_post(s->s);
}

void semaforo_cerrar(semaforo_t *s)
{
    if (s->s && s->s != SEM_FAILED)
        sem_close(s->s);
    s->s = NULL;
}

void semaforo_borrar(const char *nombre)
{
    sem_unlink(nombre);
}

void nombre_recurso(char *buf, size_t n, const char *base, const char *sufijo)
{
    snprintf(buf, n, "/%s.%s", base, sufijo);
}

/* ---------- hilos ---------- */

int hilo_crear(hilo_t *h, funcion_hilo f, void *arg)
{
    return pthread_create(h, NULL, f, arg) == 0 ? 0 : -1;
}

void hilo_esperar(hilo_t h)
{
    pthread_join(h, NULL);
}

void mutex_iniciar(mutex_t *m)   { pthread_mutex_init(m, NULL); }
void mutex_bloquear(mutex_t *m)  { pthread_mutex_lock(m); }
void mutex_soltar(mutex_t *m)    { pthread_mutex_unlock(m); }
void mutex_destruir(mutex_t *m)  { pthread_mutex_destroy(m); }

void cond_iniciar(cond_t *c)                 { pthread_cond_init(c, NULL); }
void cond_esperar(cond_t *c, mutex_t *m)     { pthread_cond_wait(c, m); }
void cond_avisar(cond_t *c)                  { pthread_cond_signal(c); }
void cond_avisar_todos(cond_t *c)            { pthread_cond_broadcast(c); }
void cond_destruir(cond_t *c)                { pthread_cond_destroy(c); }

/* ---------- varios ---------- */

void dormir_ms(long ms)
{
    struct timespec ts;
    if (ms <= 0)
        return;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
        ;
}

int64_t reloj_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int pid_actual(void)
{
    return (int)getpid();
}

int proceso_vivo(int pid)
{
    if (pid <= 0)
        return 0;
    /* la señal 0 no hace nada, solo sirve para preguntar si existe */
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
}

int entrada_es_terminal(void)
{
    return isatty(STDIN_FILENO);
}

static void (*funcion_interrupcion)(void) = NULL;

static void manejador_senal(int sig)
{
    (void)sig;
    if (funcion_interrupcion)
        funcion_interrupcion();
}

void al_interrumpir(void (*f)(void))
{
    struct sigaction sa;
    funcion_interrupcion = f;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = manejador_senal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int ruta_ejecutable(char *buf, size_t n, const char *argv0)
{
    (void)n;   /* en macOS no se usa */
#if defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, n - 1);
    if (len > 0) {
        buf[len] = '\0';
        return 0;
    }
#elif defined(__APPLE__)
    char tmp[PATH_MAX];
    uint32_t tam = sizeof(tmp);
    if (_NSGetExecutablePath(tmp, &tam) == 0 && realpath(tmp, buf))
        return 0;
#endif
    /* si no se pudo, toca confiar en argv[0] */
    if (argv0 && realpath(argv0, buf))
        return 0;
    return -1;
}

int existe_archivo(const char *ruta)
{
    return access(ruta, X_OK) == 0;
}

int lanzar_programa(const char *ruta, char **argv)
{
    execv(ruta, argv);
    /* si llegamos aquí es porque execv falló */
    return 127;
}
