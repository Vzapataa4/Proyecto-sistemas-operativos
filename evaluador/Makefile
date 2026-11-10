# Makefile del evaluador de examenes medicos
#
#   make            compila todo y deja los programas en bin/
#   make clean      borra lo compilado
#   make install    copia los programas a $(PREFIX)/bin  (ver Install.md)
#
# Detecta solo si esta en Linux, macOS o Windows (MSYS2 / MinGW).

ifeq ($(origin CC),default)
    CC = gcc
endif
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra
PREFIX  ?= /usr/local

SRC = src
BIN = bin

ifeq ($(OS),Windows_NT)
    PLATAFORMA = $(SRC)/plataforma_win.c
    EXE        = .exe
    LIBS       =
else
    PLATAFORMA = $(SRC)/plataforma_posix.c
    EXE        =
    CFLAGS    += -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
    ifeq ($(shell uname -s),Darwin)
        LIBS   = -pthread
    else
        LIBS   = -pthread -lrt
    endif
endif

# lo que usan todos los programas
COMUN = $(SRC)/memoria.c $(SRC)/cola.c $(SRC)/args.c $(PLATAFORMA)
HEADERS = $(wildcard $(SRC)/*.h)

PROGRAMAS = $(BIN)/evaluator$(EXE) \
            $(BIN)/evaluator-init$(EXE) \
            $(BIN)/evaluator-reg$(EXE) \
            $(BIN)/evaluator-ctrl$(EXE) \
            $(BIN)/evaluator-rep$(EXE)

all: $(PROGRAMAS)

$(BIN)/evaluator$(EXE): $(SRC)/evaluator.c $(PLATAFORMA) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/evaluator.c $(PLATAFORMA) $(LIBS)

$(BIN)/evaluator-init$(EXE): $(SRC)/init.c $(SRC)/despachador.c $(SRC)/analizador.c $(SRC)/robot.c $(COMUN) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/init.c $(SRC)/despachador.c $(SRC)/analizador.c $(SRC)/robot.c $(COMUN) $(LIBS)

$(BIN)/evaluator-reg$(EXE): $(SRC)/reg.c $(COMUN) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/reg.c $(COMUN) $(LIBS)

$(BIN)/evaluator-ctrl$(EXE): $(SRC)/ctrl.c $(COMUN) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/ctrl.c $(COMUN) $(LIBS)

$(BIN)/evaluator-rep$(EXE): $(SRC)/rep.c $(COMUN) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC)/rep.c $(COMUN) $(LIBS)

clean:
	rm -f $(PROGRAMAS)

install: all
	mkdir -p $(PREFIX)/bin
	cp $(PROGRAMAS) $(PREFIX)/bin/

.PHONY: all clean install
