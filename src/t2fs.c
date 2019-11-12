/*
Trabalho Pratico II
Implementacao de um Sistema de Arquivos T2FS (revisao 21.10.19)

O objetivo deste trabalho eh a aplicacao dos conceitos de sistemas operacionais
na implementacao de um sistema de arquivos que sera chamado, daqui para diante,
de T2FS (Task 2 – File System – Versao 2019.2) e devera ser implementado,
OBRIGATORIAMENTE, na linguagem “C”, sem o uso de outras bibliotecas, com excecao da libc.
Alem disso, a implementacao devera executar na maquina virtual fornecida no Moodle.

Alunos: 
	Gabriel Lando
	Leonardo Lauryel
	Thayna Minuzzo
*/

#include "../include/t2fs.h"

/*-----------------------------------------------------------------------------
-> Habilitar o debug: linha abaixo descomentada.
-> Desabilitar o debug: linha abaixo comentada.
-----------------------------------------------------------------------------*/
#define IS_DEBUG

/*-----------------------------------------------------------------------------
Variaveis globais
-----------------------------------------------------------------------------*/
#define MAX_OPENED_FILES	10
#define MAX_FILENAME		50

int partitionMounted = -1;
int fileCounter = 0;
struct t2fs_record openedFiles[MAX_OPENED_FILES] = { 0 };

/*-----------------------------------------------------------------------------
Funcao:	Informa a identificacao dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2(char* name, int size) {
	strncpy(name, "Gabriel Lando - 00291399\nLeonardo Lauryel - 00275616\nThayna Minuzzo - 00262525", size);
	return 0;
}


/*-----------------------------------------------------------------------------
Funcao extras essenciais para o funcionamento do programa
-----------------------------------------------------------------------------*/
static void DEBUG(char* format, ...);
static DWORD strToInt(unsigned char* str, int size);
static DWORD Checksum(void* data, int qty);
static int validateFilename(int len, char* filename);
static int isPartition(int partition);
static void partitionSectors(int partition, DWORD* setor_inicial, DWORD* setor_final);
static int allocBlockOrInode(int isBlock, int partition);
static int readSuperblock(int partition, struct t2fs_superbloco* superbloco);
static int writeInode(int index, struct t2fs_inode inode, int partition);
static int readInode(int index, struct t2fs_inode* inode, int partition);
static int readBlockFromInode(int index, struct t2fs_inode inode, int sectors_per_block, int partition, unsigned char* buffer);
static int readDirEntry(int index, struct t2fs_record* record);
static int findFileByName(char* filename, struct t2fs_record* record);
static int addBlockOnInode(struct t2fs_inode* inode, int sectors_per_block, DWORD blockID);
static int createNewFile(char* filename, struct t2fs_record* record, int type);
static int writeDirEntry(struct t2fs_record record);
static int disallocBlockOrInode(int isBlock, int partition, int index);
static int clearInodeBlocks(struct t2fs_inode* inode, int sectors_per_block);



/*-----------------------------------------------------------------------------
Funcao:	Formata logicamente uma particao do disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho
		corresponde a um multiplo de setores dados por sectors_per_block.

Retorno:
		 0: Sucesso
		-1: Parametros invalidos
		-2: Erro na leitura do setor zero do disco
		-3: Numero da particao invalido
		-4: Setores por bloco nao for divisor da qtde de setores da particao
		-5: Erro na escrita no disco
-----------------------------------------------------------------------------*/
int format2(int partition, int sectors_per_block) {
	if (partition < 0 || sectors_per_block <= 0) {
		DEBUG("#ERRO format2: parametros invalidos\n");
		return -1;
	}

	// Testar se existe a particao
	int ret = 0;
	if ((ret = isPartition(partition)))
		return ret;

	DWORD setor_inicial = 0;
	DWORD setor_final = 0; 
	partitionSectors(partition, &setor_inicial, &setor_final);

	DWORD qtde_setores = setor_final - setor_inicial + 1;
	DWORD qtde_blocos = qtde_setores / sectors_per_block;

	double val = ((double)(qtde_blocos) / (SECTOR_SIZE * 8));
	WORD freeBlocksBitmapSize = ((WORD)val == val) ? ((WORD)val) : ((DWORD)(val + 1));
	val = (double)qtde_blocos / 10;
	WORD inodeAreaSize = ((DWORD)val == val) ? ((DWORD)val) : ((DWORD)(val + 1)); // round(10% da qtde de blocos)
	val = ((double)inodeAreaSize * sectors_per_block) / (sizeof(struct t2fs_inode) * 8);
	WORD freeInodeBitmapSize = ((DWORD)val == val) ? ((DWORD)val) : ((DWORD)(val + 1));

	DWORD minQtdBlocos = 2 + freeBlocksBitmapSize + freeInodeBitmapSize + inodeAreaSize;

	////DEBUG("#INFO format2: freeBlocksBitmapSize: %d   freeInodeBitmapSize: %d   inodeAreaSize: %d   minQtdBlocos: %d   Size inode: %d\n", freeBlocksBitmapSize, freeInodeBitmapSize, inodeAreaSize, minQtdBlocos, sizeof(struct t2fs_inode));

	if (qtde_blocos < minQtdBlocos) {
		DEBUG("#ERRO format2: ha poucos blocos (diminuir qtde de setores por bloco)\n");
		return -4;
	}

	// Se esta dentro do intervalo de particoes,
	// cria novo superbloco e limpa bitmap de blocos e inodes
	struct t2fs_superbloco newSuperbloco = {
		.id = {'T', '2', 'F', 'S'},
		.version = 0x7E32,
		.superblockSize = 1,
		.freeBlocksBitmapSize = freeBlocksBitmapSize, /** Numero de blocos do bitmap de blocos de dados */
		.freeInodeBitmapSize = freeInodeBitmapSize,   /** Numero de blocos do bitmap de i-nodes */
		.inodeAreaSize = inodeAreaSize,				  /** Numero de blocos reservados para os i-nodes */
		.blockSize = (WORD)sectors_per_block,		  /** Numero de setores que formam um bloco */
		.diskSize = qtde_blocos,					  /** Numero total de blocos da particao */
		.Checksum = 0								  /** Soma dos 5 primeiros inteiros de 32 bits do superbloco */
	};

	// Calculando Checksum
	newSuperbloco.Checksum = Checksum((void*)&newSuperbloco, 5);

	// ESCREVER DADOS NA PARTICAO
	// Gravar super bloco na particao formatada
	unsigned char* superblocoArea = (unsigned char*)calloc((size_t)(SECTOR_SIZE * sectors_per_block), sizeof(unsigned char));
	memcpy(superblocoArea, &newSuperbloco, sizeof(struct t2fs_superbloco));

	for (DWORD i = 0; i < sectors_per_block; i++)
		if (write_sector(setor_inicial + i, &superblocoArea[i * SECTOR_SIZE])) {
			DEBUG("#ERRO format2: erro na escrita do superbloco\n");
			return -5;
		}

	free(superblocoArea);

	// Alocar e zerar area de memoria
	unsigned char* emptySector = (unsigned char*)calloc(SECTOR_SIZE, sizeof(unsigned char));

	// Zera o restante da particao
	for (DWORD i = 0; i < qtde_setores - sectors_per_block; i++)
		if (write_sector(setor_inicial + sectors_per_block + i, emptySector)) {
			DEBUG("#ERRO format2: erro ao apagar dados da particao\n");
			return -5;
		}

	free(emptySector);
	// Criar o Diretorio raiz

	// Alocar 1 inode pra salvar o diretorio raiz
	int inodeIndex = allocBlockOrInode(0, partition);
	
	// Gera a estrutura do inode e escreve no disco
	struct t2fs_inode inodeRoot = {
		.blocksFileSize = 0,
		.bytesFileSize = 0,
		.dataPtr = { 0 },
		.singleIndPtr = 0,
		.doubleIndPtr = 0,
		.RefCounter = 0
	};

	if ((ret = writeInode(inodeIndex, inodeRoot, partition)))
		return ret;

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Monta a particao indicada por "partition" no diretorio raiz

Retorno:
		 0: Sucesso
		-2: Erro na leitura do setor zero do disco
		-3: Numero da particao invalido
		-6: Checksum invalido
-----------------------------------------------------------------------------*/
int mount(int partition) {

	int ret = 0;
	if((ret = readSuperblock(partition, NULL)))
		return ret;

	partitionMounted = partition;

	for (int i = 0; i < MAX_OPENED_FILES; i++)
		openedFiles[i].TypeVal = 0;

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Desmonta a particao atualmente montada, liberando o ponto de montagem.
-----------------------------------------------------------------------------*/
int umount(void) {
	partitionMounted = -1;

	/*CLOSE ALL FILES*/

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para criar um novo arquivo no disco e abri-lo,
		sendo, nesse ultimo aspecto, equivalente a funcao open2.
		No entanto, diferentemente da open2, se filename referenciar um
		arquivo ja existente, o mesmo tera seu conteudo removido e
		assumira um tamanho de zero bytes.

Retorno:
		-11: Filename muito longo
-----------------------------------------------------------------------------*/
FILE2 create2(char* filename) {
	if (partitionMounted == -1) {
		DEBUG("#ERRO create2: particao nao montada\n");
		return -3;
	}

	if(fileCounter >= MAX_OPENED_FILES) {
		DEBUG("#ERRO create2: limite de arquivos excedido\n");
		return -13;
	}

	int ret = 0;
	if ((ret = validateFilename(strlen(filename), filename))) {
		DEBUG("#ERRO create2: filename invalido\n");
		return ret;
	}

	struct t2fs_record record;
	if ((ret = findFileByName(filename, &record)) == 0) {
		// criar novo arquivo
		if ((ret = createNewFile(filename, &record, 1))) {
			DEBUG("#ERRO create2: erro ao criar novo arquivo (%d)\n", ret);
			return ret;
		}
	}
	else if (ret < 0) {
		DEBUG("#ERRO create2: erro ao encontrar arquivo (%d)\n", ret);
		return ret;
	}
	else {
		struct t2fs_inode inode;
		readInode(record.inodeNumber, &inode, partitionMounted);

		struct t2fs_superbloco superbloco;
		readSuperblock(partitionMounted, &superbloco);
		clearInodeBlocks(&inode, superbloco.blockSize);
		
		writeInode(record.inodeNumber, inode, partitionMounted);
	}

	FILE2 freeHandle = 0;
	for (freeHandle = 0; freeHandle < MAX_OPENED_FILES; freeHandle++) {
		if (openedFiles[freeHandle].TypeVal == 0) {
			openedFiles[freeHandle] = record;
			fileCounter++;
			break;
		}
	}

	return freeHandle;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para remover (apagar) um arquivo do disco.
-----------------------------------------------------------------------------*/
int delete2(char* filename) {
	return -1;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2(char* filename) {
	if (partitionMounted == -1) {
		DEBUG("#ERRO open2: particao nao montada\n");
		return -3;
	}

	if (fileCounter >= MAX_OPENED_FILES) {
		DEBUG("#ERRO open2: limite de arquivos excedido\n");
		return -13;
	}

	int ret = 0;
	if ((ret = validateFilename(strlen(filename), filename))) {
		DEBUG("#ERRO open2: filename invalido\n");
		return ret;
	}

	struct t2fs_record record;
	if (findFileByName(filename, &record) <= 0) {
		DEBUG("#ERRO open2: arquivo nao existe\n");
		return -10;
	}

	FILE2 freeHandle = 0;
	for (freeHandle = 0; freeHandle < MAX_OPENED_FILES; freeHandle++) {
		if (openedFiles[freeHandle].TypeVal == 0) {
			openedFiles[freeHandle] = record;
			fileCounter++;
			break;
		}
	}

	return freeHandle;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2(FILE2 handle) {
	if (partitionMounted == -1) {
		DEBUG("#ERRO close2: particao nao montada\n");
		return -3;
	}

	if (handle < 0 || handle >= MAX_OPENED_FILES || openedFiles[handle].TypeVal == 0) {
		DEBUG("#ERRO close2: handle invalido\n");
		return -14;
	}

	fileCounter--;		
	openedFiles[handle].TypeVal = 0;

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2(FILE2 handle, char* buffer, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2(FILE2 handle, char* buffer, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao que abre um diretorio existente no disco.
-----------------------------------------------------------------------------*/
int opendir2(void) {
	return -1;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para ler as entradas de um diretorio.
-----------------------------------------------------------------------------*/
int readdir2(DIRENT2* dentry) {
	return -1;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para fechar um diretorio.
-----------------------------------------------------------------------------*/
int closedir2(void) {
	return -1;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para criar um caminho alternativo (softlink)
-----------------------------------------------------------------------------*/
int sln2(char* linkname, char* filename) {
	return -1;
}

/*-----------------------------------------------------------------------------
Funcao:	Funcao usada para criar um caminho alternativo (hardlink)
-----------------------------------------------------------------------------*/
int hln2(char* linkname, char* filename) {
	return -1;
}




/*-----------------------------------------------------------------------------
Funcao extras essenciais para o funcionamento do programa
-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
Funcao:	Cria um novo arquivo e retorna a struct dele

Retorno:
		 #: Identificador do arquivo somado em 1
		 0: Arquivo nao encontrado
		-3: Numero da particao invalido
-----------------------------------------------------------------------------*/
static int createNewFile(char* filename, struct t2fs_record* record, int type) {

	if (partitionMounted == -1) {
		DEBUG("#ERRO createNewFile: particao nao montada\n");
		return -3;
	}

	int indexInode = allocBlockOrInode(0, partitionMounted);
	if(indexInode < 0) {
		DEBUG("#ERRO createNewFile: erro no inode\n");
		return indexInode;
	}
	
	struct t2fs_record newRecord = {
		.TypeVal = type,
		.inodeNumber = indexInode
	};
	
	strcpy(newRecord.name, filename);

	struct t2fs_inode newInode = {
		.blocksFileSize = 0,
		.bytesFileSize = 0,
		.dataPtr = { 0 },
		.singleIndPtr = 0,
		.doubleIndPtr = 0,
		.RefCounter = 0
	};

	int ret = 0;
	if ((ret = writeInode(indexInode, newInode, partitionMounted)))
		return ret;

	if ((ret = writeDirEntry(newRecord)))
		return ret;

	//DEBUG("#ERRO findFileByName: arquivo nao encontrado\n");
	*record = newRecord;

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Encontra um arquivo no disco

Retorno:
		 #: Identificador do arquivo somado em um 
		 0: Sucesso - Arquivo nao encontrado
-----------------------------------------------------------------------------*/
static int findFileByName(char* filename, struct t2fs_record* record) {
	if (partitionMounted == -1) {
		DEBUG("#ERRO findFileByName: particao nao montada\n");
		return -3;
	}

	int ret = 0;
	struct t2fs_superbloco superbloco;
	if ((ret = readSuperblock(partitionMounted, &superbloco))) {
		DEBUG("#ERRO findFileByName: erro na leitura do superbloco\n");
		return ret;
	}

	struct t2fs_inode inode;
	if ((ret = readInode(0, &inode, partitionMounted))) {
		DEBUG("#ERRO findFileByName: erro na leitura do inode 0\n");
		return ret;
	}

	DWORD qtyFiles = inode.bytesFileSize / sizeof(struct t2fs_record);

	////DEBUG("#INFO findFileByName: qtyFiles: %u  bytesFileSize: %u\n", qtyFiles, inode.bytesFileSize);

	for (DWORD i = 0; i < qtyFiles; i++) {
		if ((ret = readDirEntry(i, record)) < 0)
			return ret;
		
		////DEBUG("#INFO findFileByName: Filename (%d): %s  record name (%d): %s\n", strlen(filename), filename, strlen(record->name), record->name);

		if (!strcmp(filename, record->name))
			return i + 1;
	}

	//DEBUG("#ERRO findFileByName: arquivo nao encontrado\n");
	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Encontra um arquivo no disco

Retorno:
		 0: Sucesso
-----------------------------------------------------------------------------*/
static int readDirEntry(int index, struct t2fs_record* record) {
	if (partitionMounted == -1) {
		DEBUG("#ERRO readDirEntry: particao nao montada\n");
		return -3;
	}

	int ret = 0;
	struct t2fs_superbloco superbloco;
	if ((ret = readSuperblock(partitionMounted, &superbloco))) {
		DEBUG("#ERRO readDirEntry: erro na leitura do superbloco\n");
		return ret;
	}

	struct t2fs_inode inode;
	if ((ret = readInode(0, &inode, partitionMounted))) {
		DEBUG("#ERRO readDirEntry: erro na leitura do inode 0\n");
		return ret;
	}
	 
	if (((index + 1) * sizeof(struct t2fs_record)) > (inode.bytesFileSize)) {
		DEBUG("#ERRO readDirEntry: indice nao se encontra na entrada de diretorio\n");
		return -8;
	}

	int indexBlock = index * sizeof(struct t2fs_record) / (SECTOR_SIZE * superbloco.blockSize);
	int offsetBlock = index % ((SECTOR_SIZE * superbloco.blockSize) / sizeof(struct t2fs_record));

	////DEBUG("#INFO readDirEntry: indexBlock: %u  offsetBlock: %u\n", indexBlock, offsetBlock);

	unsigned char* buffer = (unsigned char*)malloc(SECTOR_SIZE * superbloco.blockSize);
	readBlockFromInode(indexBlock, inode, superbloco.blockSize, partitionMounted, buffer);
	
	struct t2fs_record* pRecord = (struct t2fs_record*)buffer;
	*record = pRecord[offsetBlock];

	free(buffer);

	////DEBUG("#INFO readDirEntry: %s\n", record->name);

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Escreve um novo arquivo no disco

Retorno:
		 0: Sucesso
-----------------------------------------------------------------------------*/
static int writeDirEntry(struct t2fs_record record) {
	if (partitionMounted == -1) {
		DEBUG("#ERRO writeDirEntry: particao nao montada\n");
		return -3;
	}

	int ret = 0;
	struct t2fs_superbloco superbloco;
	if ((ret = readSuperblock(partitionMounted, &superbloco))) {
		DEBUG("#ERRO writeDirEntry: erro na leitura do superbloco\n");
		return ret;
	}

	struct t2fs_inode inode;
	if ((ret = readInode(0, &inode, partitionMounted))) {
		DEBUG("#ERRO writeDirEntry: erro na leitura do inode 0\n");
		return ret;
	}

	////DEBUG("#INFO writeDirEntry: bytesFileSize: %u  %u\n", inode.bytesFileSize, inode.blocksFileSize * SECTOR_SIZE / superbloco.blockSize);

	if (!inode.blocksFileSize || !(inode.bytesFileSize % (inode.blocksFileSize * SECTOR_SIZE / superbloco.blockSize))) {
		// Alocar novo bloco
		int indexBlk = allocBlockOrInode(1, partitionMounted);
		if (indexBlk < 0) {
			DEBUG("#ERRO writeDirEntry: erro ao alocar novo bloco\n");
			return indexBlk;
		}

		//DEBUG("#INFO: indexBlk %u\n", indexBlk);
		
		if ((ret = addBlockOnInode(&inode, superbloco.blockSize, indexBlk))) {
			DEBUG("#ERRO writeDirEntry: erro ao adicionar bloco no inode\n");
			return ret;
		}
	}

	//DWORD indiceDir = (inode.bytesFileSize % (inode.blocksFileSize * SECTOR_SIZE / superbloco.blockSize)) / sizeof(struct t2fs_record);
	DWORD indiceDir = (inode.bytesFileSize - ((inode.blocksFileSize - 1) * SECTOR_SIZE * superbloco.blockSize)) / sizeof(struct t2fs_record);

	//DEBUG("#INFO writeDirEntry: indiceDir: %u  blockSize: %u\n", indiceDir, superbloco.blockSize);

	unsigned char* buffer = (unsigned char*)malloc(SECTOR_SIZE * superbloco.blockSize);

	int index = 0;
	if ((index = readBlockFromInode(inode.blocksFileSize - 1, inode, superbloco.blockSize, partitionMounted, buffer)) < 0) {
		DEBUG("#ERRO writeDirEntry: erro ao ler bloco do inode\n");
		return index;
	}

	struct t2fs_record* tmpArray = (struct t2fs_record*)buffer;
	tmpArray[indiceDir] = record;
	inode.bytesFileSize += sizeof(struct t2fs_record);

	////DEBUG("#INFO writeDirEntry: indiceDir: %u  sector to write: %u\n", indiceDir, ret);
	/***************************************************************************************************************/
	DWORD setor_inicial = 0;
	partitionSectors(partitionMounted, &setor_inicial, NULL);
	DWORD writeIndex = setor_inicial + index * superbloco.blockSize;

	//DEBUG("#INFO: Indice: %u  writeIndex %u\n", index, writeIndex);
	for (int i = 0; i < superbloco.blockSize; i++)
		write_sector(writeIndex + i, &buffer[i * SECTOR_SIZE]);
	/***************************************************************************************************************/
	if ((ret = writeInode(0, inode, partitionMounted))) {
		DEBUG("#ERRO writeDirEntry: erro na gravacao do inode 0\n");
		return ret;
	}

	free(buffer);

	return 0;
}


/*-----------------------------------------------------------------------------
Funcao:	Retorna um block do disco apontado pelo inode
Entrada:
		index: indice do bloco a ser lido
		inode: inode que aponta para os blocos
		sectors_per_block: valor de superbloco.blockSize
		buffer: ponteiro para o buffer que irá receber os dados, deve ser sectors_per_block * SECTOR_SIZE

Retorno:
		 #: Endereço do bloco lido
-----------------------------------------------------------------------------*/
static int readBlockFromInode(int index, struct t2fs_inode inode, int sectors_per_block, int partition, unsigned char* buffer) {
	unsigned long long int maxIndirSimples = sectors_per_block * SECTOR_SIZE / sizeof(DWORD);

	if (index >= inode.blocksFileSize || index < 0) {
		DEBUG("#ERRO readBlockFromInode: inode nao contem esse indice\n");
		return -9;
	}

	DWORD setor_inicial = 0;
	partitionSectors(partition, &setor_inicial, NULL);

	if (index < 2) {
		DWORD readIndex = setor_inicial + inode.dataPtr[index] * sectors_per_block;
		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);
		return inode.dataPtr[index];
	}
	else if ((index - 2) < maxIndirSimples) {
		index -= 2;

		DWORD readIndex = setor_inicial + inode.singleIndPtr * sectors_per_block;

		unsigned char* indSimp = (unsigned char*)malloc(SECTOR_SIZE * sectors_per_block);
		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &indSimp[i * SECTOR_SIZE]);

		DWORD* pIndirSimples = (DWORD*)indSimp;
		readIndex = setor_inicial + pIndirSimples[index] * sectors_per_block;

		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

		DWORD indexRet = pIndirSimples[index];
		free(indSimp);

		return indexRet;
	}
	else if ((index - maxIndirSimples - 2) < (maxIndirSimples * maxIndirSimples)) {
		index -= (2 + maxIndirSimples);
		DWORD indexIndir1 = index / maxIndirSimples;
		DWORD indexIndir2 = index % maxIndirSimples;

		DWORD readIndex = setor_inicial + inode.doubleIndPtr * sectors_per_block;

		unsigned char* indDupla = (unsigned char*)malloc(SECTOR_SIZE * sectors_per_block);
		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &indDupla[i * SECTOR_SIZE]);
		DWORD* pIndirDupla = (DWORD*)indDupla;
		DWORD indDupla1 = pIndirDupla[indexIndir1];
		
		readIndex = setor_inicial + indDupla1 * sectors_per_block;

		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &indDupla[i * SECTOR_SIZE]);

		pIndirDupla = (DWORD*)indDupla;
		indDupla1 = pIndirDupla[indexIndir2];
		readIndex = setor_inicial + indDupla1 * sectors_per_block;

		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

		DWORD indexRet = indDupla1;
		free(indDupla);

		return indexRet;
	}
	else {
		DEBUG("#ERRO readBlockFromInode: indice invalido para esse inode\n");
		return -9;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Remove todos os blocos de um inode
Entrada:
		inode: inode que aponta para os blocos
		sectors_per_block: valor de superbloco.blockSize

Retorno:
		 0: Sucesso
-----------------------------------------------------------------------------*/
static int clearInodeBlocks(struct t2fs_inode* inode, int sectors_per_block) {
	unsigned long long int maxIndirSimples = sectors_per_block * SECTOR_SIZE / sizeof(DWORD);

	DWORD setor_inicial = 0;
	partitionSectors(partitionMounted, &setor_inicial, NULL);

	DWORD index = inode->blocksFileSize;

	for (int i = index - 1; i >= 0; i--) {
		if (index < 2)
			disallocBlockOrInode(1, partitionMounted, inode->dataPtr[index]);
		else if ((index - 2) < maxIndirSimples) {
			index -= 2;

			DWORD readIndex = setor_inicial + inode->singleIndPtr * sectors_per_block;
			unsigned char* buffer = (unsigned char*)malloc(SECTOR_SIZE * sectors_per_block);
			for (int i = 0; i < sectors_per_block; i++)
				read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

			DWORD* pIndirSimples = (DWORD*)buffer;
			disallocBlockOrInode(1, partitionMounted, pIndirSimples[index]);

			for (int i = 0; i < sectors_per_block; i++)
				write_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

			free(buffer);

			if(!index)
				disallocBlockOrInode(1, partitionMounted, inode->singleIndPtr);

		}
		else if ((index - maxIndirSimples - 2) < (maxIndirSimples * maxIndirSimples)) {
			index -= (2 + maxIndirSimples);

			DWORD indexIndir1 = index / maxIndirSimples;
			DWORD indexIndir2 = index % maxIndirSimples;

			DWORD readIndex = setor_inicial + inode->doubleIndPtr * sectors_per_block;
			unsigned char* buffer = (unsigned char*)malloc(SECTOR_SIZE * sectors_per_block);
			for (int i = 0; i < sectors_per_block; i++)
				read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

			DWORD* pIndirDupla1 = (DWORD*)buffer;
			
			readIndex = setor_inicial + pIndirDupla1[indexIndir1] * sectors_per_block;
			for (int i = 0; i < sectors_per_block; i++)
				read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

			DWORD* pIndirDupla2 = (DWORD*)buffer;
			disallocBlockOrInode(1, partitionMounted, pIndirDupla2[indexIndir2]);

			for (int i = 0; i < sectors_per_block; i++)
				write_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

			free(buffer);

			if (pIndirDupla1[indexIndir1] == 0)
				disallocBlockOrInode(1, partitionMounted, pIndirDupla1[indexIndir1]);

			if (!index)
				disallocBlockOrInode(1, partitionMounted, inode->doubleIndPtr);

		}
	}
	inode->blocksFileSize = 0;
	inode->bytesFileSize = 0;
	inode->dataPtr[0] = 0;
	inode->dataPtr[1] = 0;
	inode->singleIndPtr = 0;
	inode->doubleIndPtr = 0;

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Adiciona um bloco de dados no inode
Entrada:
		inode: inode que aponta para os blocos
		sectors_per_block: valor de superbloco.blockSize
		blockID: ID do bloco pra ser adicionado

Retorno:
		 0: Sucesso
-----------------------------------------------------------------------------*/
static int addBlockOnInode(struct t2fs_inode *inode, int sectors_per_block, DWORD blockID) {
	unsigned long long int maxIndirSimples = sectors_per_block * SECTOR_SIZE / sizeof(DWORD);

	DWORD setor_inicial = 0;
	partitionSectors(partitionMounted, &setor_inicial, NULL);

	DWORD index = inode->blocksFileSize;

	if (index < 2)
		inode->dataPtr[index] = blockID;
	else if ((index - 2) < maxIndirSimples) {
		index -= 2;

		if (inode->singleIndPtr == 0) {
			DWORD indexBlk = allocBlockOrInode(1, partitionMounted);
			if (indexBlk < 0) {
				DEBUG("#ERRO addBlockOnInode: erro ao alocar novo bloco\n");
				return indexBlk;
			}
			inode->singleIndPtr = indexBlk;
		}

		DWORD readIndex = setor_inicial + inode->singleIndPtr * sectors_per_block;
		unsigned char* buffer = (unsigned char*)malloc(SECTOR_SIZE * sectors_per_block);
		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);
		DWORD* pIndirSimples = (DWORD*)buffer;
		pIndirSimples[index] = blockID;
		for (int i = 0; i < sectors_per_block; i++)
			write_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

		free(buffer);
	}
	else if ((index - maxIndirSimples - 2) < (maxIndirSimples * maxIndirSimples)) {
		index -= (2 + maxIndirSimples);

		if (inode->doubleIndPtr == 0) {
			DWORD indexBlk = allocBlockOrInode(1, partitionMounted);
			if (indexBlk < 0) {
				DEBUG("#ERRO addBlockOnInode: erro ao alocar novo bloco\n");
				return indexBlk;
			}
			inode->doubleIndPtr = indexBlk;
		}

		DWORD indexIndir1 = index / maxIndirSimples;
		DWORD indexIndir2 = index % maxIndirSimples;

		DWORD readIndex = setor_inicial + inode->doubleIndPtr * sectors_per_block;
		unsigned char* buffer = (unsigned char*)malloc(SECTOR_SIZE * sectors_per_block);
		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

		DWORD* pIndirDupla1 = (DWORD*)buffer;
		if (pIndirDupla1[indexIndir1] == 0) {
			DWORD indexBlk = allocBlockOrInode(1, partitionMounted);
			if (indexBlk < 0) {
				DEBUG("#ERRO addBlockOnInode: erro ao alocar novo bloco\n");
				return indexBlk;
			}

			////DEBUG("#INFO addBlockOnInode: indexBlk = %u\n", indexBlk);

			pIndirDupla1[indexIndir1] = indexBlk;
			for (int i = 0; i < sectors_per_block; i++)
				write_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);
		}

		readIndex = setor_inicial + pIndirDupla1[indexIndir1] * sectors_per_block;
		for (int i = 0; i < sectors_per_block; i++)
			read_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);
		DWORD* pIndirDupla2 = (DWORD*)buffer;
		pIndirDupla2[indexIndir2] = blockID;
		for (int i = 0; i < sectors_per_block; i++)
			write_sector(readIndex + i, &buffer[i * SECTOR_SIZE]);

		free(buffer);
	}
	else {
		DEBUG("#ERRO addBlockOnInode: inode excede o limite de blocos\n");
		return -12;
	}

	inode->blocksFileSize++;

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Desaloca um bloco ou inode

Entrada:
		isBlock:	TRUE: alocar 1 bloco
					FALSE: alocar 1 inode
		index:		Indice a ser desalocado
Retorno:
		0: Sucesso
-----------------------------------------------------------------------------*/
static int disallocBlockOrInode(int isBlock, int partition, int index) {
	DWORD setor_inicial = 0;
	partitionSectors(partition, &setor_inicial, NULL);

	if (openBitmap2(setor_inicial)) {
		DEBUG("#ERRO allocBlockOrInode: erro ao abrir bitmap\n");
		return -7;
	}

	int ret = 0;
	struct t2fs_superbloco superbloco;
	if ((ret = readSuperblock(partition, &superbloco)))
		return ret;

	int indexToRemove = index;

	if (isBlock)
		indexToRemove -= superbloco.superblockSize + superbloco.freeBlocksBitmapSize + superbloco.freeInodeBitmapSize + superbloco.inodeAreaSize;

	if (setBitmap2(isBlock, indexToRemove, 0)) {
		DEBUG("#ERRO allocBlockOrInode: erro ao alterar bitmap\n");
		return -7;
	}
	
	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Aloca um bloco ou inode e retorna o indice dele

Entrada:
		isBlock:	TRUE: alocar 1 bloco
					FALSE: alocar 1 inode
Retorno:
		 #: Identificador do bloco/inode alocado
		-3: Numero da particao invalido
		-7: Erro em operacoes com funcoes de bitmap
-----------------------------------------------------------------------------*/
static int allocBlockOrInode(int isBlock, int partition) {
	DWORD setor_inicial = 0;
	partitionSectors(partition, &setor_inicial, NULL);

	if (openBitmap2(setor_inicial)) {
		DEBUG("#ERRO allocBlockOrInode: erro ao abrir bitmap\n");
		return -7;
	}

	int ret = 0;
	struct t2fs_superbloco superbloco;
	if ((ret = readSuperblock(partition, &superbloco)))
		return ret;

	DWORD numMax = 0;

	if(isBlock)
		numMax = superbloco.diskSize - (superbloco.superblockSize + superbloco.freeBlocksBitmapSize + superbloco.freeInodeBitmapSize + superbloco.inodeAreaSize);
	else
		numMax = superbloco.inodeAreaSize * superbloco.blockSize * (SECTOR_SIZE / sizeof(struct t2fs_inode));

	int index = 0;

	while (index < numMax && (ret = getBitmap2(isBlock, index++)) == 1);

	if (index >= numMax || ret != 0) {
		DEBUG("#ERRO allocBlockOrInode: erro ao buscar bitmap\n");
		return -7;
	}

	if (setBitmap2(isBlock, --index, 1)) {
		DEBUG("#ERRO allocBlockOrInode: erro ao alterar bitmap\n");
		return -7;
	}

	if (closeBitmap2()) {
		DEBUG("#ERRO allocBlockOrInode: erro ao fechar bitmap\n");
		return -7;
	}

	// Se for um bloco, limpar o conteudo dele
	if (isBlock) {
		index += superbloco.superblockSize + superbloco.freeBlocksBitmapSize + superbloco.freeInodeBitmapSize + superbloco.inodeAreaSize;

		DWORD writeIndex = setor_inicial + index * superbloco.blockSize;

		unsigned char* buffer = (unsigned char*)calloc(SECTOR_SIZE, sizeof(unsigned char));
		for (int i = 0; i < superbloco.blockSize; i++)
			write_sector(writeIndex + i, buffer);
		free(buffer);
	}
	
	return index;
}

/*-----------------------------------------------------------------------------
Funcao:	Le um inode na area reservada para inodes
-----------------------------------------------------------------------------*/
static int readInode(int index, struct t2fs_inode *inode, int partition) {

	struct t2fs_superbloco superbloco;

	int ret;
	if ((ret = readSuperblock(partition, &superbloco)))
		return ret;

	DWORD setor_inicial = 0;
	partitionSectors(partition, &setor_inicial, NULL);

	DWORD sectorToRead = (superbloco.superblockSize + superbloco.freeBlocksBitmapSize + superbloco.freeInodeBitmapSize) * superbloco.blockSize + (index * sizeof(struct t2fs_inode) / SECTOR_SIZE);

	unsigned char buffer[SECTOR_SIZE];
	read_sector(setor_inicial + sectorToRead, buffer);

	struct t2fs_inode* inodePointer = (struct t2fs_inode*)buffer;
	*inode = inodePointer[index % (SECTOR_SIZE / sizeof(struct t2fs_inode))];

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Escreve um inode na area reservada para inodes
-----------------------------------------------------------------------------*/
static int writeInode(int index, struct t2fs_inode inode, int partition) {

	struct t2fs_superbloco superbloco;

	int ret;
	if ((ret = readSuperblock(partition, &superbloco)))
		return ret;

	DWORD setor_inicial = 0;
	partitionSectors(partition, &setor_inicial, NULL);

	int sectorToWrite = (superbloco.superblockSize + superbloco.freeBlocksBitmapSize + superbloco.freeInodeBitmapSize) * superbloco.blockSize + (index * sizeof(struct t2fs_inode) / SECTOR_SIZE);

	unsigned char buffer[SECTOR_SIZE];
	read_sector(setor_inicial + sectorToWrite, buffer);

	struct t2fs_inode* inodePointer = (struct t2fs_inode*)buffer;
	inodePointer[index % (SECTOR_SIZE / sizeof(struct t2fs_inode))] = inode;

	write_sector(setor_inicial + sectorToWrite, buffer);

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Retorna o superbloco da particao.
		Pode ser usada para testar se a particao eh valida
-----------------------------------------------------------------------------*/
static int readSuperblock(int partition, struct t2fs_superbloco* superbloco) {
	
	int ret;
	if ((ret = isPartition(partition)))
		return ret;

	DWORD setor_inicial = 0;
	partitionSectors(partition, &setor_inicial, NULL);

	unsigned char buffer[SECTOR_SIZE];
	read_sector(setor_inicial, buffer);

	// Calculando Checksum
	if (Checksum((void*)buffer, 6)) {
		DEBUG("#ERRO readSuperblock: erro ao verificar o checksum (particao corrompida)\n");
		return -6;
	}

	if(superbloco)
		memcpy(superbloco, buffer, sizeof(struct t2fs_superbloco));

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	Retorna o primeiro e ultimo setor da particao como referencia
-----------------------------------------------------------------------------*/
static void partitionSectors(int partition, DWORD* setor_inicial, DWORD* setor_final) {
	// Testar se existe a particao
	unsigned char buffer[SECTOR_SIZE];
	read_sector(0, buffer);

	int byte_inicial = strToInt(&buffer[4], 2) + 32 * partition;

	if (setor_inicial)
		*setor_inicial = strToInt(&buffer[byte_inicial], 4);

	if (setor_final)
		*setor_final = strToInt(&buffer[byte_inicial + 4], 4);
}

/*-----------------------------------------------------------------------------
Funcao:	verifica se a particao existe

Retorno:
		 0: Sucesso
		-2: Erro na leitura do setor zero do disco
		-3: Numero da particao invalido
-----------------------------------------------------------------------------*/
static int isPartition(int partition) {
	// Testar se existe a particao
	unsigned char buffer[SECTOR_SIZE];

	if (read_sector(0, buffer)) {
		DEBUG("#ERRO isPartition: erro na leitura do setor 0\n");
		return -2;
	}

	int qtd_partitions = strToInt(&buffer[6], 2);

	if (partition >= qtd_partitions || partition < 0) {
		DEBUG("#ERRO isPartition: particao invalida\n");
		return -3;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	converte string to int using little-endian
-----------------------------------------------------------------------------*/
static DWORD strToInt(unsigned char* str, int size) {
	int ret = 0;

	for (int i = size - 1; i >= 0; i--)
		ret += str[i] * (1 << (8 * i));

	return ret;
}


/*-----------------------------------------------------------------------------
Funcao:	valida o nome do arquivo, retornando uma nova string apenas com
		caracteres permitidos

Entrada:
		len: tamanho da string
		filename: string com o nome do arquivo
Retorno:
		  0: Sucesso
		-11: Filename incorreto
-----------------------------------------------------------------------------*/
static int validateFilename(int len, char* filename) {
	int newSize = 0;

	for (int i = 0; i < len; i++) {
		if (!(!newSize && filename[i] == ' ')) {
			char c = filename[i];
			if (c == '-' || c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.')
				filename[newSize++] = c;
		}
	}
	if (newSize <= MAX_FILENAME)
		filename[newSize] = '\0';
	else
		filename[MAX_FILENAME] = '\0';

	////DEBUG("#INFO validateFilename: Filename = %s\n", filename);

	if (!newSize)
		return -11;

	return 0;
}

/*-----------------------------------------------------------------------------
Funcao:	calcula Checksum

Entrada:
		data: ponteiro void* para uma estrutura
		qty:  quantidade de bytes para calcular
Retorno: valor do checksum
-----------------------------------------------------------------------------*/
static DWORD Checksum(void* data, int qty) {

	DWORD* calcChecksum = (DWORD*)data;
	unsigned long long int tmpChecksum = 0;
	for (int i = 0; i < qty; i++)
		tmpChecksum += calcChecksum[i];
	DWORD checksum = (DWORD)tmpChecksum + (DWORD)(tmpChecksum >> 32);

	return ~checksum;
}

/*-----------------------------------------------------------------------------
Funcao para debug.

Equivalente a um printf(), porem verifica se esta em modo Debug ou nao.

Se definida a variavel IS_DEBUG, os comandos de debug serao impressos.
-----------------------------------------------------------------------------*/
static void DEBUG(char* format, ...) {

#ifdef IS_DEBUG
	va_list args;
	va_start(args, format);

	vprintf(format, args);

	va_end(args);
#endif

}