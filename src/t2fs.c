
/**
*/
#include "../include/t2fs.h"


#define IS_DEBUG

/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
	strncpy(name, "Gabriel Lando - 00291399\nLeonardo Lauryel - 00275616\nThayná Minuzzo - 00262525", size);
	return 0;
}


/*-----------------------------------------------------------------------------
Função extras essenciais para o funcionamento do programa
-----------------------------------------------------------------------------*/
static void DEBUG(char *format, ...);
static DWORD strToInt(unsigned char *str, int size);
static DWORD Checksum(void *data, int qty);



/*-----------------------------------------------------------------------------
Função:	Formata logicamente uma partição do disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho 
		corresponde a um múltiplo de setores dados por sectors_per_block.

Retorno:
		 0: Sucesso
		-1: Parâmetros inválidos
		-2: Erro na leitura do setor zero do disco
		-3: Numero da partição inválido
		-4: Setores por bloco não for divisor da qtde de setores da partição
		-5: Erro na escrita no disco
-----------------------------------------------------------------------------*/
int format2(int partition, int sectors_per_block) {
	if (partition < 0 || sectors_per_block <= 0)	{
		DEBUG("#ERRO format2: parâmetros inválidos\n");
		return -1;
	}

	// Testar se existe a partição
	unsigned char buffer[SECTOR_SIZE];

	if (read_sector(0, buffer)){
		DEBUG("#ERRO format2: erro na leitura do setor 0\n");
		return -2;
	}

	int qtd_partitions = strToInt(&buffer[6], 2);
	//DEBUG("Qtde de particoes: %d\n", qtd_partitions);

	if (partition >= qtd_partitions) {
		DEBUG("#ERRO format2: particao invalida\n");
		return -3;
	}

	int byte_inicial = strToInt(&buffer[4], 2) + 32 * partition;
	//DEBUG("Byte inicial: %d\n", byte_inicial);

	DWORD setor_inicial = strToInt(&buffer[byte_inicial], 4);
	DWORD setor_final = strToInt(&buffer[byte_inicial + 4], 4);
	DWORD qtde_setores = setor_final - setor_inicial + 1;
	DWORD qtde_blocos = qtde_setores / sectors_per_block;

	WORD freeInodeBitmapSize = 1 + (WORD)((double)(qtde_blocos) / (SECTOR_SIZE * 8));
	WORD freeBlocksBitmapSize = 1 + (WORD)((double)(qtde_blocos) / (SECTOR_SIZE * 8));
	WORD inodeAreaSize = 1 + (WORD)((double)(qtde_blocos) * sizeof(struct t2fs_inode) / SECTOR_SIZE);

	DWORD minQtdBlocos = 2 + freeBlocksBitmapSize + freeInodeBitmapSize + inodeAreaSize;

	//DEBUG("freeBlocksBitmapSize: %d   freeInodeBitmapSize: %d   inodeAreaSize: %d   minQtdBlocos: %d   Size inode: %d\n", freeBlocksBitmapSize, freeInodeBitmapSize, inodeAreaSize, minQtdBlocos, sizeof(struct t2fs_inode));

	if (qtde_blocos < minQtdBlocos)
	{
		DEBUG("#ERRO format2: ha poucos blocos (diminuir qtde de setores por bloco)\n");
		return -4;
	}

	//DEBUG("Setor inicial: %d   Setor final: %d   Qtde de setores: %d  Qtde de blocos: %d\n", setor_inicial, setor_final, qtde_setores, qtde_blocos);

	// Se está dentro do intervalo de partiçoes,
	// cria novo superbloco e limpar bitmap de blocos e inodes

	struct t2fs_superbloco newSuperbloco = {
		.id = {'T', '2', 'F', 'S'},
		.version = 0x7E32,
		.superblockSize = 1,
		.freeBlocksBitmapSize = freeBlocksBitmapSize, /** Número de blocos do bitmap de blocos de dados */
		.freeInodeBitmapSize = freeInodeBitmapSize,   /** Número de blocos do bitmap de i-nodes */
		.inodeAreaSize = inodeAreaSize,				  /** Número de blocos reservados para os i-nodes */
		.blockSize = (WORD)sectors_per_block,		  /** Número de setores que formam um bloco */
		.diskSize = qtde_blocos,					  /** Número total de blocos da partição */
		.Checksum = 0								  /** Soma dos 5 primeiros inteiros de 32 bits do superbloco */
	};

	// Calculando Checksum
	newSuperbloco.Checksum = Checksum((void *)&newSuperbloco, 5);

	// ESCREVER DADOS NA PARTICAO
	// Gravar super bloco na partição formatada
	unsigned char *superblocoArea = (unsigned char *)calloc((size_t)(SECTOR_SIZE * sectors_per_block), sizeof(unsigned char));
	memcpy(superblocoArea, &newSuperbloco, sizeof(struct t2fs_superbloco));

	for (DWORD i = 0; i < sectors_per_block; i++)
		if (write_sector(setor_inicial + i, &superblocoArea[i * SECTOR_SIZE]))
		{
			DEBUG("#ERRO format2: erro na escrita do superbloco\n");
			return -5;
		}

	// Alocar e zerar area de memoria
	unsigned char *emptySector = (unsigned char *)calloc(SECTOR_SIZE, sizeof(unsigned char));

	// Zera o restante da partição
	for (DWORD i = 0; i < qtde_setores - sectors_per_block; i++)
		if (write_sector(setor_inicial + sectors_per_block + i, emptySector))
		{
			DEBUG("#ERRO format2: erro ao apagar dados da particao\n");
			return -5;
		}

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Monta a partição indicada por "partition" no diretório raiz
-----------------------------------------------------------------------------*/
int mount(int partition) {

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Desmonta a partição atualmente montada, liberando o ponto de montagem.
-----------------------------------------------------------------------------*/
int umount(void) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um 
		arquivo já existente, o mesmo terá seu conteúdo removido e 
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco. 
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
int opendir2(void) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2(DIRENT2 *dentry) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2(void) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink)
-----------------------------------------------------------------------------*/
int sln2(char *linkname, char *filename) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (hardlink)
-----------------------------------------------------------------------------*/
int hln2(char *linkname, char *filename) {
	return -1;
}





/*-----------------------------------------------------------------------------
Função:	calcula Checksum

Entrada:
		data: ponteiro void* para uma estrutura
		qty:  quantidade de bytes para calcular
Retorno: valor do checksum
-----------------------------------------------------------------------------*/
static DWORD Checksum(void *data, int qty) {

	DWORD *calcChecksum = (DWORD *)data;
	unsigned long long int tmpChecksum = 0;
	for (int i = 0; i < qty; i++)
		tmpChecksum += calcChecksum[i];
	DWORD checksum = (DWORD)tmpChecksum + (DWORD)(tmpChecksum >> 32);

	return ~checksum;
}



/*-----------------------------------------------------------------------------
Função:	converte string to int using little-endian
-----------------------------------------------------------------------------*/
static DWORD strToInt(unsigned char *str, int size) {
	int ret = 0;

	for (int i = size - 1; i >= 0; i--)
		ret += str[i] * (1 << (8*i));

	return ret;
}

/*-----------------------------------------------------------------------------
Função para debug.

Equivalente a um printf(), porem verifica se esta em modo Debug ou nao.

Se definida a variavel IS_DEBUG, os comandos de debug serao impressos.
-----------------------------------------------------------------------------*/
static void DEBUG(char *format, ...){

#ifdef IS_DEBUG
	va_list args;
	va_start(args, format);

	vprintf(format, args);

	va_end(args);
#endif

}