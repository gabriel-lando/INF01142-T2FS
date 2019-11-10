/*
Retornos:
		+0: Sucesso
		-1: Parametros invalidos
		-2: Erro na leitura do setor zero do disco
		-3: Numero da particao invalido
		-4: Setores por bloco nao for divisor da qtde de setores da particao
		-5: Erro na escrita no disco
		-6: Checksum invalido
		-7: Erro em operacoes com funcoes de bitmap
*/

#ifndef __LIBT2FS___
#define __LIBT2FS___

#include "t2disk.h"
#include "apidisk.h"
#include "bitmap2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef int FILE2;

typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned int DWORD;

#pragma pack(push, 1)

/** Registro com as informacoes da entrada de diretorio, lida com readdir2 */
#define MAX_FILE_NAME_SIZE 255
typedef struct {
	char    name[MAX_FILE_NAME_SIZE + 1]; /* Nome do arquivo cuja entrada foi lida do disco      */
	BYTE    fileType;                   /* Tipo do arquivo: regular (0x01) ou diretorio (0x02) */
	DWORD   fileSize;                   /* Numero de bytes do arquivo                          */
} DIRENT2;

#pragma pack(pop)


/*-----------------------------------------------------------------------------
Funcao: Usada para identificar os desenvolvedores do T2FS.
	Essa funcao copia um string de identificacao para o ponteiro indicado por "name".
	Essa copia nao pode exceder o tamanho do buffer, informado pelo parametro "size".
	O string deve ser formado apenas por caracteres ASCII (Valores entre 0x20 e 0x7A) e terminado por '\0'.
	O string deve conter o nome e numero do cartao dos participantes do grupo.

Entra:	name -> buffer onde colocar o string de identificacao.
	size -> tamanho do buffer "name" (numero maximo de bytes a serem copiados).

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
	Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int identify2(char* name, int size);


/*-----------------------------------------------------------------------------
Funcao:	Formata uma particao do disco virtual.
		Uma particao deve ser montada, antes de poder ser montada para uso.

Entra:	partition -> numero da particao a ser formatada
		sectors_per_block -> numero de setores que formam um bloco, para uso na formatacao da particao

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
		Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int format2(int partition, int sectors_per_block);


/*-----------------------------------------------------------------------------
Funcao:	Monta a particao indicada por "partition" no diretorio raiz

Entra:	partition -> numero da particao a ser montada

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
		Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int mount(int partition);


/*-----------------------------------------------------------------------------
Funcao:	Desmonta a particao atualmente montada, liberando o ponto de montagem.

Entra:	-

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
		Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int umount(void);


/*-----------------------------------------------------------------------------
Funcao: Criar um novo arquivo.
	O nome desse novo arquivo eh aquele informado pelo parametro "filename".
	O contador de posicao do arquivo (current pointer) deve ser colocado na posicao zero.
	Caso ja exista um arquivo com o mesmo nome, a funcao devera retornar um erro de criacao.
	A funcao deve retornar o identificador (handle) do arquivo.
	Esse handle sera usado em chamadas posteriores do sistema de arquivo para fins de manipulacao do arquivo criado.

Entra:	filename -> nome do arquivo a ser criado.

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna o handle do arquivo (numero positivo).
	Em caso de erro, deve ser retornado um valor negativo.
-----------------------------------------------------------------------------*/
FILE2 create2(char* filename);


/*-----------------------------------------------------------------------------
Funcao:	Apagar um arquivo do disco.
	O nome do arquivo a ser apagado eh aquele informado pelo parametro "filename".

Entra:	filename -> nome do arquivo a ser apagado.

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
	Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int delete2(char* filename);


/*-----------------------------------------------------------------------------
Funcao:	Abre um arquivo existente no disco.
	O nome desse novo arquivo eh aquele informado pelo parametro "filename".
	Ao abrir um arquivo, o contador de posicao do arquivo (current pointer) deve ser colocado na posicao zero.
	A funcao deve retornar o identificador (handle) do arquivo.
	Esse handle sera usado em chamadas posteriores do sistema de arquivo para fins de manipulacao do arquivo criado.
	Todos os arquivos abertos por esta chamada sao abertos em leitura e em escrita.
	O ponto em que a leitura, ou escrita, sera realizada eh fornecido pelo valor current_pointer (ver funcao seek2).

Entra:	filename -> nome do arquivo a ser apagado.

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna o handle do arquivo (numero positivo)
	Em caso de erro, deve ser retornado um valor negativo
-----------------------------------------------------------------------------*/
FILE2 open2(char* filename);


/*-----------------------------------------------------------------------------
Funcao:	Fecha o arquivo identificado pelo parametro "handle".

Entra:	handle -> identificador do arquivo a ser fechado

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
	Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int close2(FILE2 handle);


/*-----------------------------------------------------------------------------
Funcao:	Realiza a leitura de "size" bytes do arquivo identificado por "handle".
	Os bytes lidos sao colocados na area apontada por "buffer".
	Apos a leitura, o contador de posicao (current pointer) deve ser ajustado para o byte seguinte ao ultimo lido.

Entra:	handle -> identificador do arquivo a ser lido
	buffer -> buffer onde colocar os bytes lidos do arquivo
	size -> numero de bytes a serem lidos

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna o numero de bytes lidos.
	Se o valor retornado for menor do que "size", entao o contador de posicao atingiu o final do arquivo.
	Em caso de erro, sera retornado um valor negativo.
-----------------------------------------------------------------------------*/
int read2(FILE2 handle, char* buffer, int size);


/*-----------------------------------------------------------------------------
Funcao:	Realiza a escrita de "size" bytes no arquivo identificado por "handle".
	Os bytes a serem escritos estao na area apontada por "buffer".
	Apos a escrita, o contador de posicao (current pointer) deve ser ajustado para o byte seguinte ao ultimo escrito.

Entra:	handle -> identificador do arquivo a ser escrito
	buffer -> buffer de onde pegar os bytes a serem escritos no arquivo
	size -> numero de bytes a serem escritos

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna o numero de bytes efetivamente escritos.
	Em caso de erro, sera retornado um valor negativo.
-----------------------------------------------------------------------------*/
int write2(FILE2 handle, char* buffer, int size);


/*-----------------------------------------------------------------------------
Funcao:	Abre o diretorio raiz da particao ativa.
		Se a operacao foi realizada com sucesso,
		a funcao deve posicionar o ponteiro de entradas (current entry) na primeira posicao valida do diretorio.

Entra:	-

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
		Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int opendir2(void);


/*-----------------------------------------------------------------------------
Funcao:	Realiza a leitura das entradas do diretorio aberto
		A cada chamada da funcao eh lida a entrada seguinte do diretorio
		Algumas das informacoes dessas entradas devem ser colocadas no parametro "dentry".
		Apas realizada a leitura de uma entrada, o ponteiro de entradas (current entry) sera ajustado para a  entrada valida seguinte.
		Sao considerados erros:
			(a) qualquer situacao que impeca a realizacao da operacao
			(b) termino das entradas validas do diretorio aberto.

Entra:	dentry -> estrutura de dados onde a funcao coloca as informacoes da entrada lida.

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
		Em caso de erro, sera retornado um valor diferente de zero ( e "dentry" nao sera valido)
-----------------------------------------------------------------------------*/
int readdir2(DIRENT2* dentry);


/*-----------------------------------------------------------------------------
Funcao:	Fecha o diretorio identificado pelo parametro "handle".

Entra:	-

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
		Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int closedir2(void);


/*-----------------------------------------------------------------------------
Funcao:	Cria um link simbolico (soft link)

Entra:	linkname -> nome do link
		filename -> nome do arquivo apontado pelo link

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
	Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int sln2(char* linkname, char* filename);


/*-----------------------------------------------------------------------------
Funcao:	Cria um link estrito (hard link)

Entra:	linkname -> nome do link
		filename -> nome do arquivo apontado pelo link

Saida:	Se a operacao foi realizada com sucesso, a funcao retorna "0" (zero).
	Em caso de erro, sera retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int hln2(char* linkname, char* filename);




#endif

