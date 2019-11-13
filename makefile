#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
# 

CC=gcc
CFLAGS=-std=c99 -Wall
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

all: t2fs
	mkdir -p $(BIN_DIR)
	ar crs $(LIB_DIR)/libt2fs.a $(LIB_DIR)/apidisk.o $(LIB_DIR)/bitmap2.o $(BIN_DIR)/t2fs.o

t2fs:
	$(CC) -c $(SRC_DIR)/t2fs.c -o $(BIN_DIR)/t2fs.o $(CFLAGS)

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~ $(BIN_DIR)


