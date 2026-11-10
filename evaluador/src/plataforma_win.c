/*
 * plataforma_win.c
 * Versión de la capa de plataforma para Windows.
 *
 * Diferencias importantes con Linux:
 *  - los objetos con nombre (memoria y semáforos) desaparecen solos cuando
 *    se cierra el último handle, así que "borrar" no hace nada.
 *  - no existe exec, entonces el envoltorio lanza al hijo y lo espera.
 */
#include "plataforma.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- memoria compartida ---------- */

int seg_crear(segmento_t *s, const char *nombre, size_t tam)
{
    unsigned long long t = (unsigned long long)tam;

    s->mapa = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 (DWORD)(t >> 32), (DWORD)(t & 0xFFFFFFFFu), nombre);
    if (s->mapa == NULL)
        return -1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(s->mapa);
        s->mapa = NULL;
        return -1;
    }
    s->base = MapViewOfFile(s->mapa, FILE_MAP_ALL_ACCESS, 0, 0, tam);
    if (s->base == NULL) {
        CloseHandle(s->mapa);
        s->mapa = NULL;
        return -1;
    }
    s->tam = tam;
    memset(s->base, 0, tam);
    return 0;
}

int seg_abrir(segmento_t *s, const char *nombre)
{
    MEMORY_BASIC_INFORMATION info;

    s->mapa = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, nombre);
    if (s->mapa == NULL)
        return -1;
    s->base = MapViewOfFile(s->mapa, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (s->base == NULL) {
        CloseHandle(s->mapa);
        s->mapa = NULL;
        return -1;
    }
    /* Windows no dice el tamaño exacto, solo el de la región (redondeado
     * a páginas). Igual el tamaño real se lee de la cabecera. */
    if (VirtualQuery(s->base, &info, sizeof(info)) != 0)
        s->tam = info.RegionSize;
    else
        s->tam = 0;
    return 0;
}

void seg_cerrar(segmento_t *s)
{
    if (s->base)
        UnmapViewOfFile(s->base);
    if (s->mapa)
        CloseHandle(s->mapa);
    s->base = NULL;
    s->mapa = NULL;
}

void seg_borrar(const char *nombre)
{
    (void)nombre;   /* se libera solo al cerrar el último handle */
}

/* ---------- semáforos con nombre ---------- */

int semaforo_crear(semaforo_t *s, const char *nombre, unsigned valor)
{
    s->h = CreateSemaphoreA(NULL, (LONG)valor, 0x7FFFFFFF, nombre);
    if (s->h == NULL)
        return -1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(s->h);
        s->h = NULL;
        return -1;
    }
    return 0;
}

int semaforo_abrir(semaforo_t *s, const char *nombre)
{
    s->h = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, nombre);
    return s->h == NULL ? -1 : 0;
}

void semaforo_esperar(semaforo_t *s)
{
    WaitForSingleObject(s->h, INFINITE);
}

int semaforo_esperar_ms(semaforo_t *s, long ms)
{
    if (ms < 0)
        ms = 0;
    return WaitForSingleObject(s->h, (DWORD)ms) == WAIT_OBJECT_0 ? 0 : 1;
}

void semaforo_avisar(semaforo_t *s)
{
    ReleaseSemaphore(s->h, 1, NULL);
}

void semaforo_cerrar(semaforo_t *s)
{
    if (s->h)
        CloseHandle(s->h);
    s->h = NULL;
}

void semaforo_borrar(const char *nombre)
{
    (void)nombre;
}

void nombre_recurso(char *buf, size_t n, const char *base, const char *sufijo)
{
    snprintf(buf, n, "Local\\%s.%s", base, sufijo);
}

/* ---------- hilos ---------- */

/* Windows quiere otra firma para la función del hilo, así que se usa
 * una funcioncita intermedia que llama a la nuestra. */
typedef struct {
    funcion_hilo f;
    void *arg;
} paquete_hilo;

static DWORD WINAPI arranque_hilo(LPVOID p)
{
    paquete_hilo paq = *(paquete_hilo *)p;
    free(p);
    paq.f(paq.arg);
    return 0;
}

int hilo_crear(hilo_t *h, funcion_hilo f, void *arg)
{
    paquete_hilo *paq = malloc(sizeof(*paq));
    if (!paq)
        return -1;
    paq->f = f;
    paq->arg = arg;
    *h = CreateThread(NULL, 0, arranque_hilo, paq, 0, NULL);
    if (*h == NULL) {
        free(paq);
        return -1;
    }
    return 0;
}

void hilo_esperar(hilo_t h)
{
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
}

void mutex_iniciar(mutex_t *m)   { InitializeCriticalSection(m); }
void mutex_bloquear(mutex_t *m)  { EnterCriticalSection(m); }
void mutex_soltar(mutex_t *m)    { LeaveCriticalSection(m); }
void mutex_destruir(mutex_t *m)  { DeleteCriticalSection(m); }

void cond_iniciar(cond_t *c)               { InitializeConditionVariable(c); }
void cond_esperar(cond_t *c, mutex_t *m)   { SleepConditionVariableCS(c, m, INFINITE); }
void cond_avisar(cond_t *c)                { WakeConditionVariable(c); }
void cond_avisar_todos(cond_t *c)          { WakeAllConditionVariable(c); }
void cond_destruir(cond_t *c)              { (void)c; }

/* ---------- varios ---------- */

void dormir_ms(long ms)
{
    if (ms > 0)
        Sleep((DWORD)ms);
}

int64_t reloj_ms(void)
{
    return (int64_t)GetTickCount64();
}

int pid_actual(void)
{
    return (int)GetCurrentProcessId();
}

int proceso_vivo(int pid)
{
    HANDLE p;
    DWORD codigo = 0;
    int vivo = 0;

    if (pid <= 0)
        return 0;
    p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (p == NULL)
        return 0;
    if (GetExitCodeProcess(p, &codigo) && codigo == STILL_ACTIVE)
        vivo = 1;
    CloseHandle(p);
    return vivo;
}

int entrada_es_terminal(void)
{
    return _isatty(_fileno(stdin));
}

static void (*funcion_interrupcion)(void) = NULL;

static BOOL WINAPI manejador_consola(DWORD tipo)
{
    if (tipo == CTRL_C_EVENT || tipo == CTRL_BREAK_EVENT || tipo == CTRL_CLOSE_EVENT) {
        if (funcion_interrupcion)
            funcion_interrupcion();
        return TRUE;
    }
    return FALSE;
}

void al_interrumpir(void (*f)(void))
{
    funcion_interrupcion = f;
    SetConsoleCtrlHandler(manejador_consola, TRUE);
}

int ruta_ejecutable(char *buf, size_t n, const char *argv0)
{
    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)n);
    (void)argv0;
    return (len > 0 && len < n) ? 0 : -1;
}

int existe_archivo(const char *ruta)
{
    DWORD a = GetFileAttributesA(ruta);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* agrega un argumento a la línea de comandos con las comillas que
 * Windows espera (las comillas internas se escapan con \) */
static void agregar_argumento(char *linea, size_t n, const char *arg)
{
    size_t len = strlen(linea);
    const char *c;
    int barras;

    if (len && len < n - 1)
        linea[len++] = ' ';
    if (len < n - 1)
        linea[len++] = '"';
    for (c = arg; *c && len < n - 4; c++) {
        barras = 0;
        while (*c == '\\') {
            barras++;
            c++;
        }
        if (*c == '\0' || *c == '"') {
            /* las barras antes de una comilla (o del final) se duplican */
            while (barras-- > 0 && len < n - 4) {
                linea[len++] = '\\';
                linea[len++] = '\\';
            }
            if (*c == '"') {
                linea[len++] = '\\';
                linea[len++] = '"';
            } else {
                break;
            }
        } else {
            while (barras-- > 0 && len < n - 4)
                linea[len++] = '\\';
            linea[len++] = *c;
        }
    }
    if (len < n - 1)
        linea[len++] = '"';
    linea[len] = '\0';
}

/* Ctrl+C en el envoltorio: no hacemos nada, el hijo comparte la consola
 * y se encarga él. Nosotros solo esperamos a que termine. */
static BOOL WINAPI ignorar_ctrl_c(DWORD tipo)
{
    return tipo == CTRL_C_EVENT || tipo == CTRL_BREAK_EVENT;
}

int lanzar_programa(const char *ruta, char **argv)
{
    char linea[32768];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD codigo = 1;
    int i;

    linea[0] = '\0';
    for (i = 0; argv[i]; i++)
        agregar_argumento(linea, sizeof(linea), argv[i]);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    SetConsoleCtrlHandler(ignorar_ctrl_c, TRUE);
    if (!CreateProcessA(ruta, linea, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
        return 127;

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &codigo);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)codigo;
}
