/*
 * args.h
 * Pequeñas ayudas para leer la línea de comandos y las variables de ambiente.
 */
#ifndef ARGS_H
#define ARGS_H

/* convierte texto a entero y revisa que esté en [min, max]. 0 si está bien */
int leer_entero(const char *txt, long min, long max, long *salida);

/* letras, números, '_' y '-', máximo MAX_NOMBRE caracteres */
int nombre_valido(const char *nombre);

/* EVALUATOR_NAME o "evaluator" */
const char *nombre_por_omision(void);

/* EVALUATOR_SCALE (entero >= 1) o 1 */
long escala_tiempo(void);

/* EVALUATOR_SEED o la hora actual */
unsigned semilla_inicial(void);

/* generador de números al azar sencillo; cada hilo tiene su propio estado
 * (rand_r no existe en Windows) */
unsigned azar(unsigned *estado);
int      azar_entre(unsigned *estado, int min, int max);

#endif
