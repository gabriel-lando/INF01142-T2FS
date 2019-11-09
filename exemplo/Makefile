#
# Makefile para programas exemplo
# 

CC=gcc
LIB_DIR=../lib

all: main t2shell

main: main.c $(LIB_DIR)/libt2fs.a
	$(CC) -o main main.c -L$(LIB_DIR) -lt2fs -Wall

t2shell: t2shell.c $(LIB_DIR)/libt2fs.a
	$(CC) -o t2shell t2shell.c -L$(LIB_DIR) -lt2fs -Wall

clean:
	rm -rf main t2shell *.o *~