# Makefile para Análisis de Temperaturas - HPC 2019

CC = gcc
MPI_INCLUDES = -I"C:/Program Files (x86)/Microsoft SDKs/MPI/Include"
MPI_LIBS = -L"C:/Program Files (x86)/Microsoft SDKs/MPI/Lib/x64" -lmsmpi
CFLAGS = -Wall -O3 -std=c11
LDFLAGS = -lm
PTHREAD_FLAGS = -pthread

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Ejecutables
EXECS = $(BIN_DIR)/seq $(BIN_DIR)/pth $(BIN_DIR)/mpi $(BIN_DIR)/mr

all: directories $(EXECS)

directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Reglas de compilación de objetos
$(OBJ_DIR)/common.o: $(SRC_DIR)/common.h
	@echo "common.h no necesita compilación (header)"

$(OBJ_DIR)/seq.o: $(SRC_DIR)/seq.c $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/pth.o: $(SRC_DIR)/pth.c $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(PTHREAD_FLAGS) -c $< -o $@

$(OBJ_DIR)/mpi.o: $(SRC_DIR)/mpi.c $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(MPI_INCLUDES) -c $< -o $@

$(OBJ_DIR)/mr.o: $(SRC_DIR)/mr.c $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(PTHREAD_FLAGS) -c $< -o $@

# Reglas de linkeo de ejecutables
$(BIN_DIR)/seq: $(OBJ_DIR)/seq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/pth: $(OBJ_DIR)/pth.o
	$(CC) $(CFLAGS) $(PTHREAD_FLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/mpi: $(OBJ_DIR)/mpi.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(MPI_LIBS)

$(BIN_DIR)/mr: $(OBJ_DIR)/mr.o
	$(CC) $(CFLAGS) $(PTHREAD_FLAGS) $^ -o $@ $(LDFLAGS)

clean:
	@echo "Limpiando..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "✓ Limpieza completada"

.PHONY: all directories clean
