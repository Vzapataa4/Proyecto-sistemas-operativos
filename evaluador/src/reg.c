/*
 * reg.c  (evaluator-reg)
 * Registra muestras en las bandejas de entrada, desde ficheros o desde
 * la entrada estándar (con "-"). Ver doc/diseno.md, sección 3.2.
 *
 * Formato de cada línea:  <bandeja> {B|D|S} <cantidad>
 */
#include "memoria.h"
#include "cola.h"
#include "args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void uso(void)
{
    fprintf(stderr, "uso: evaluator reg [-n <nombre>] { <fichero> ... | - }\n");
}

/* revisa una línea. 1 = muestra válida, 0 = línea vacía o comentario,
 * -1 = error (y deja el motivo en *motivo) */
static int interpretar_linea(char *linea, int num_entradas,
                             int *bandeja, int *tipo, int *cantidad,
                             const char **motivo)
{
    char letra[8], sobra[2];
    long b, q;
    int leidos;
    char *p = linea;

    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#')
        return 0;

    leidos = sscanf(p, "%ld %7s %ld %1s", &b, letra, &q, sobra);
    if (leidos != 3) {
        *motivo = "formato invalido (se espera: <bandeja> {B|D|S} <cantidad>)";
        return -1;
    }
    if (strlen(letra) != 1 || tipo_desde_letra(letra[0]) < 0) {
        *motivo = "tipo invalido (debe ser B, D o S)";
        return -1;
    }
    if (b < 0 || b >= num_entradas) {
        *motivo = "bandeja fuera de rango";
        return -1;
    }
    if (q < 1 || q > 5) {
        *motivo = "cantidad fuera de rango (1 a 5)";
        return -1;
    }
    *bandeja = (int)b;
    *tipo = tipo_desde_letra(letra[0]);
    *cantidad = (int)q;
    return 1;
}

/* deja la muestra en la bandeja. Devuelve el id, o 0 si el sistema se apagó */
static uint64_t registrar(sistema_t *sis, int bandeja, int tipo, int cantidad)
{
    cola_t *c = cola_entrada(sis, bandeja);
    muestra_t m;

    /* si la bandeja está llena, aquí se queda esperando */
    semaforo_esperar(&sis->sem.e_vacios[bandeja]);
    if (!sis->cab->activo)
        return 0;

    memset(&m, 0, sizeof(m));
    semaforo_esperar(&sis->sem.id);
    m.id = sis->cab->siguiente_id++;
    semaforo_avisar(&sis->sem.id);

    m.bandeja = bandeja;
    m.tipo = tipo;
    m.cantidad = cantidad;
    m.llegada = (int64_t)time(NULL);

    semaforo_esperar(&sis->sem.e_mutex[bandeja]);
    cola_meter(c, items_de(sis, c), &m);
    semaforo_avisar(&sis->sem.e_mutex[bandeja]);

    semaforo_avisar(&sis->sem.e_llenos[bandeja]);
    return m.id;
}

/* nombre.txt -> nombre.spl (solo cambia la extensión del archivo) */
static void nombre_spl(const char *original, char *buf, size_t n)
{
    const char *barra1 = strrchr(original, '/');
    const char *barra2 = strrchr(original, '\\');
    const char *nombre = original, *punto;
    size_t largo;

    if (barra1 && barra1 + 1 > nombre)
        nombre = barra1 + 1;
    if (barra2 && barra2 + 1 > nombre)
        nombre = barra2 + 1;
    punto = strrchr(nombre, '.');
    largo = (punto && punto != nombre) ? (size_t)(punto - original) : strlen(original);
    snprintf(buf, n, "%.*s.spl", (int)largo, original);
}

/* procesa un origen de muestras. Devuelve 3 si el sistema se apagó */
static int procesar(sistema_t *sis, FILE *entrada, const char *nombre, FILE *salida)
{
    char linea[512];
    int num = 0, bandeja, tipo, cantidad, r;
    const char *motivo = "";
    uint64_t id;

    while (fgets(linea, sizeof(linea), entrada)) {
        num++;
        r = interpretar_linea(linea, sis->cab->num_entradas, &bandeja, &tipo, &cantidad, &motivo);
        if (r == 0)
            continue;
        if (r < 0) {
            fprintf(stderr, "%s:%d: %s\n", nombre, num, motivo);
            continue;
        }
        id = registrar(sis, bandeja, tipo, cantidad);
        if (id == 0) {
            fprintf(stderr, "reg: el evaluador se cerro\n");
            return 3;
        }
        if (salida) {
            fprintf(salida, "%llu %d %c %d\n", (unsigned long long)id, bandeja,
                    letra_tipo(tipo), cantidad);
            fflush(salida);
        } else {
            printf("%llu\n", (unsigned long long)id);
            fflush(stdout);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    sistema_t sis;
    const char *nombre = nombre_por_omision();
    char spl[4096];
    FILE *f, *out;
    int i, primero = 1, r;

    /* -n (o -s) para el nombre; lo demás son ficheros o "-" */
    while (primero < argc && (strcmp(argv[primero], "-n") == 0 || strcmp(argv[primero], "-s") == 0)) {
        if (primero + 1 >= argc) {
            uso();
            return 2;
        }
        nombre = argv[primero + 1];
        primero += 2;
    }
    if (primero >= argc) {
        uso();
        return 2;
    }
    if (!nombre_valido(nombre)) {
        fprintf(stderr, "reg: nombre invalido '%s'\n", nombre);
        return 2;
    }

    if (conectar(&sis, nombre) != 0) {
        fprintf(stderr, "reg: el sistema '%s' no esta iniciado (use evaluator init)\n", nombre);
        return 3;
    }

    for (i = primero; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            r = procesar(&sis, stdin, "stdin", NULL);
        } else {
            f = fopen(argv[i], "r");
            if (!f)
                continue;   /* el enunciado pide ignorarlo sin mensaje */
            nombre_spl(argv[i], spl, sizeof(spl));
            out = fopen(spl, "w");
            if (!out)
                fprintf(stderr, "reg: no se pudo crear %s, los ids se muestran aqui\n", spl);
            r = procesar(&sis, f, argv[i], out);
            fclose(f);
            if (out)
                fclose(out);
        }
        if (r != 0) {
            desconectar(&sis);
            return r;
        }
    }

    desconectar(&sis);
    return 0;
}
