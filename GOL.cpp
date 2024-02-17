#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <mpi.h>  
#include <pthread.h>
#include <allegro.h>

#define v(r,c) (r)*(nColsCurrRank+2)+(c)

//configuration.txt
int nPartX, nPartY;		// N° partizioni lungo asse x (orizzontale)	e lungo asse y (verticale)
int nThreads;				// N° di thread
int step;			 		// N° di step

//input.txt
int totalRows, totalCols;	// N° righe/colonne totali della matrice 

//Valori dell'ambiente MPI
int size;		// N° complessivo di processi
int rank;		// id del processo attuale

//Variabili inizializzate dalla funzione initAllPartitions
int nRowsCurrRank, nColsCurrRank;			// N° di righe/colonne del processo corrente

int *nRowsForEachPart, *nColsForEachPart;	// Array che contengono, per ogni partizione, il n° di righe/colonne al proprio interno (nella partizione)
//##################### ad esempio nRowsForEachPart[0] conterrà il n° di righe nella prima partizione e nColsForEachPart[0] il n° di colonne.

int nPartXForEachProc, nPartYForEachProc;	// N° di partizioni X(oriz)/partizioni Y(vert) che contine ciascun processo

int nProcOnX;	// N° di processi lungo l'asse x

//RANKS
int rankX, rankY;		// rank del processo con riferimento all'asse x/y
int rankMaster = 0;		// rank del processo Master
// rank dei processi adiacenti --> secondo il vicinato di Moore 
int rankUp, rankDown, rankLeft, rankRight, rankUpLeft, rankUpRight, rankDownLeft, rankDownRight;	

//MATRICI PER LO SWAP
int * readM;		// matrice di lettura
int * writeM;		// matrice di scrittura

//INDICI DI INIZIO
int beginXCurrProc, beginYCurrProc;				// indice d'inizio delle colonne (su x)/ righe (su y) del processo corrente
int *startColForEachProc, *startRowForEachProc;	// array che contengono, per ogni posizione, gli indici d'inizio delle colonne/righe di ciascun processo

//SIZE DEI PROCESSI
int *nRowsForEachProc, *nColsForEachProc;	// array che contengono, per ogni posizione, la sizeY (rows) / sizeX (cols) di ciascun processo

//MATRICE TOTALE
int *matTot;	// matrice totale: che viene usata dal rank 0 per ricevere le matrici dagli altri processi e fare la stampa dell'AC

//Datatypes
MPI_Datatype typeColumn;				// Datatype per inviare una colonna
MPI_Datatype typeMatLessHaloBorders;	// Datatype per inviare una matrice SENZA gli halo borders (mat interna) 

//Variabili PTHREADS
pthread_t * IDthreads;	// array di thread

//BARRIERS
pthread_mutex_t mutexcompletedBarrier;    				// mutex barriera di computazione dell'AC
pthread_mutex_t mutexComunicationBarrier;   			// mutex per la barriera di comunicazione delle send /receive
int contcompletedBarrier =0, contComunicationBarrier=0;	//contatori per le barrier

//CONDITIONS
pthread_cond_t condcompletedBarrier;      	// condition per la completedBarrier
pthread_cond_t condComunicationBarrier;     // condition per la ComunicationBarrier


//Variabili per ALLEGRO
clock_t inizio, fine;		// tempo all'inizio e alla fine dell'algoritmo per calcolare la durata
int delayAllegro = 80;		// delay per la stampa a schermo di allegro
int graphicCellDim = 50;	// dimensione grafica di una cella
int infoLine = 55;			// spazio per scrivere i dati dell'AC
bool allegroOn = true; 

void initAllegro(){ 
    allegro_init();
	install_keyboard();
    set_color_depth(32);  
	set_window_title("Girls Project APSD");
    set_gfx_mode(GFX_AUTODETECT_WINDOWED, totalCols * graphicCellDim, totalRows * graphicCellDim + infoLine, 0, 0);  
}
void drawWithAllegro(int step){  //funzione per la stampa a schermo tramite allegro
	int endY = totalRows * graphicCellDim+5; //inizia poco dopo la fineX della matrice sulle X
	
	for(int i = 0; i < totalRows; i++){
		for(int j = 0; j < totalCols; j++){
			int x = j * graphicCellDim;
			int y = i * graphicCellDim;
			if (matTot[i * totalCols + j] == 1) rectfill(screen, x, y, x + graphicCellDim, y + graphicCellDim,  makecol(19, 0, 7)); //Cella VIVA
			else rectfill(screen, x, y, x + graphicCellDim, y + graphicCellDim, makecol(239, 255, 200));							//Cella MORTA
		}
	}
    rectfill(screen, 0, endY, totalCols * graphicCellDim, totalRows * graphicCellDim + infoLine, makecol(232, 92, 144));

	// Stampa a schermo --> GRIGLIA
	for(int i = 0; i < totalRows; i++) line(screen, 0, i * graphicCellDim, totalCols * graphicCellDim, i * graphicCellDim, makecol(219, 209, 197));
	for(int i = 0; i < totalCols; i++) line(screen, i * graphicCellDim, 0, i * graphicCellDim, totalRows * graphicCellDim, makecol(219, 209, 197));

	
	//Stampa a schermo --> PARTIZIONI VERTICALI 
	int y=0;
	for(int i = 0; i < nPartY; i++){
		if(i % nPartYForEachProc == 0) line(screen, 0, y, totalCols * graphicCellDim, y, makecol(29, 120, 116));  //Processi
		else line(screen, 0, y, totalCols * graphicCellDim, y, makecol(250, 157, 50));							  //Thread (divisioni nei processi)
		y += nRowsForEachPart[i] * graphicCellDim;
	}

	//Stampa a schermo --> PARTIZIONI ORIZZONTALI 
	int x=0;
	for(int i = 0; i < nPartX; i++){
		if(i%nPartXForEachProc == 0) line(screen, x, 0, x, totalRows * graphicCellDim, makecol(189, 33, 4)); 
		else line(screen, x, 0, x, totalRows * graphicCellDim, makecol(193, 140, 93));
		x += nColsForEachPart[i] * graphicCellDim;
	}

	//Stampa a schermo --> INFO
	char str[64];
	sprintf(str, "Step %d", step);
	textout_ex(screen, font, str, 0, endY+1, makecol(255, 255, 255), -1); //+1 per farlo partire poco dopo la fineY della matrice
	sprintf(str, "N° di Thread: %d",nThreads);	
    textout_ex(screen, font, str, 0, endY+15, makecol(255, 255, 255), -1); // Testo bianco
	sprintf(str, "N° di partizioni su X: %d",nPartX);
	textout_ex(screen, font, str, 0, endY+26, makecol(255, 255, 255), -1); // Testo bianco
	sprintf(str, "N° di partizioni su Y: %d",nPartY);
	textout_ex(screen, font, str, 0, endY+37, makecol(255, 255, 255), -1); // Testo bianco
	sprintf(str, "N° righe dell'AC: %d",totalRows);
	textout_ex(screen, font, str, 250, endY+26, makecol(255, 255, 255), -1); // Testo bianco
	sprintf(str, "N° colonne dell'AC: %d",totalCols);
	textout_ex(screen, font, str, 250, endY+37, makecol(255, 255, 255), -1); // Testo bianco
	usleep(delayAllegro * 1000);
}
void distributeRowsAndCols(int total, int numPartitions, int* partitionSizes) { // Serve per distribuire il resto. Total può essere totalRows o totalCols
    int remainder = total % numPartitions; //remainder è il resto (nel caso in cui non sia divisibile)
    for (int i = 0; i < numPartitions; i++) {
        partitionSizes[i] = total / numPartitions; //viene assegnata la size (minima, senza remainder) ad ogni partizione
        if (i < remainder) partitionSizes[i]++; //se ci sono ancora righe nel remainder da assegnare, la dà all'attuale partizione
    }
}

int sumPartitions(int start, int numPartitions, int* partitionSizes) { // Serve per calcolare la size di una sottomatrice. Start è l'indice di partenza
    int sum = 0;
    for (int i = 0; i < numPartitions; i++) { //numPartitions è il numero di partizioni che devo sommare assegnate al current rank
        sum += partitionSizes[start + i]; //partitionSizes è l'array che contiene le size  (numero di celle piccole) di tutte le partizioni
    }
    return sum; //restituisce la size (numero di celle totali) della sottomatrice del processo corrente
}

void initAllPartitions() {

	// Inizializza le dimensioni delle partizioni
    nRowsForEachPart = new int[nPartY]; //quante righe di celle per ogni partizione
    nColsForEachPart = new int[nPartX]; //quante colonne di celle per ogni partizione
	
    // Trova un divisore comune (i) per nThreads e nPartX o nThreads e nPartY
    //(in base alla formula size* nThreads = nPartX * nPartY si sa che ne esiste sempre uno)
	// Decide COME suddividere la matrice totale tra i processi (quali sottomatrici assegnare a ciascun processo)
    for (int i = nThreads; i > 0; i--) {
        if ((nThreads % i == 0) && (nPartX % i == 0)) { //se i è un divisore comune di nThreads e nPartX
            nPartYForEachProc = nThreads / i; //per fare in modo che ad ogni thread venga assegnata una partizione: (nThreads /1)*1= nThreads
            nPartXForEachProc = i;
            break;
        } else if ((nThreads % i == 0) && (nPartY % i == 0)) { //se i è un divisore comune di nThreads e nPartY
            nPartYForEachProc = i;
            nPartXForEachProc = nThreads / i;
            break;
        } 
	}

	//Distribuisco le righe e le colonne tra le partizioni (considerando anche il resto, ovvero il remainder)
    distributeRowsAndCols(totalRows, nPartY, nRowsForEachPart);
    distributeRowsAndCols(totalCols, nPartX, nColsForEachPart);

    // Calcola il numero di processi lungo l'asse X
	// Quante sottomatrici finiscono su X 
    nProcOnX = nPartX / nPartXForEachProc;

    // Scomponi il rank del processo --> COORDINATE DEL PROCESSO
    rankX = rank % nProcOnX;
    rankY = rank / nProcOnX;

    // Calcola le dimensioni della sottomatrice per il processo corrente
    nRowsCurrRank = sumPartitions(rankY * nPartYForEachProc, nPartYForEachProc, nRowsForEachPart);
    nColsCurrRank = sumPartitions(rankX * nPartXForEachProc, nPartXForEachProc, nColsForEachPart);

    // Inizializza matrici di lettura e scrittura
    readM = new int[(nRowsCurrRank + 2) * (nColsCurrRank + 2)]; //+2 perchè si considerano le halo cells
    writeM = new int[(nRowsCurrRank + 2) * (nColsCurrRank + 2)];
}
void calculateMooreNeighbourhood(){ // Calcolo del rank dei processi adiacenti (sopra, sotto, sinistra, destra, diagonali)
    rankUp = ((rank - nProcOnX + size) % size);
    rankDown = ((rank + nProcOnX) % size);
    rankLeft = ((rank - 1 + nProcOnX) % nProcOnX) + (rank / nProcOnX) * nProcOnX;
    rankRight = ((rank + 1) % nProcOnX) + (rank / nProcOnX) * nProcOnX;
    rankUpLeft = ((rankUp - 1 + nProcOnX) % nProcOnX) + (rankUp / nProcOnX) * nProcOnX;
    rankUpRight = ((rankUp + 1) % nProcOnX) + (rankUp / nProcOnX) * nProcOnX;
    rankDownLeft = ((rankDown - 1 + nProcOnX) % nProcOnX) + (rankDown / nProcOnX) * nProcOnX;
    rankDownRight = ((rankDown + 1) % nProcOnX) + (rankDown / nProcOnX) * nProcOnX;
}
void sendRecvRows(int rankUp, int rankDown, MPI_Request &req) {
    MPI_Isend(&readM[v(1,1)], nColsCurrRank, MPI_INT, rankUp, 17, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(nRowsCurrRank, 1)], nColsCurrRank, MPI_INT, rankDown, 20, MPI_COMM_WORLD, &req);

    MPI_Recv(&readM[v(nRowsCurrRank+1, 1)], nColsCurrRank, MPI_INT, rankDown, 17, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(0,1)], nColsCurrRank, MPI_INT, rankUp, 20, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}
void sendRecvCols(int rankLeft, int rankRight, MPI_Request &req) {
    MPI_Isend(&readM[v(0, 1)], 1, typeColumn, rankLeft, 12, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(0, nColsCurrRank)], 1, typeColumn, rankRight, 30, MPI_COMM_WORLD, &req);

    MPI_Recv(&readM[v(0, 0)], 1, typeColumn, rankLeft, 30, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(0, nColsCurrRank+1)], 1, typeColumn, rankRight, 12, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}
void sendRecvCorners(int rankUpLeft, int rankUpRight, int rankDownLeft, int rankDownRight, MPI_Request &req) {
    MPI_Isend(&readM[v(1, 1)], 1, MPI_INT, rankUpLeft, 40, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(1, nColsCurrRank)], 1, MPI_INT, rankUpRight, 41, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(nRowsCurrRank, 1)], 1, MPI_INT, rankDownLeft, 42, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(nRowsCurrRank, nColsCurrRank)], 1, MPI_INT, rankDownRight, 43, MPI_COMM_WORLD, &req);

    MPI_Recv(&readM[v(nRowsCurrRank+1, 0)], 1, MPI_INT, rankDownLeft, 40, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(nRowsCurrRank+1, nColsCurrRank+1)], 1, MPI_INT, rankDownRight, 41, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(0, 0)], 1, MPI_INT, rankUpLeft, 42, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(0, nColsCurrRank+1)], 1, MPI_INT, rankUpRight, 43, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}
void exchangeBordersMoore() {
    MPI_Request req;
    // Invio e ricezione delle righe superiore e inferiore
    sendRecvRows(rankUp, rankDown, req);
    // Invio e ricezione delle colonne sinistra e destra
    sendRecvCols(rankLeft, rankRight, req);
    // Invio e ricezione dei corner: in alto a sinistra, in alto a destra, in basso a sinistra, in basso a destra
    sendRecvCorners(rankUpLeft, rankUpRight, rankDownLeft, rankDownRight, req);
}
//legge il valore di una riga del file di configurazione e lo restutisce come intero
int readConfigValue(FILE* file, char* buffer, int maxLineLength, long maxLimit) {
    if (fgets(buffer, maxLineLength, file)) {
        long value = strtol(buffer, NULL, 10);
        if (value <= maxLimit) {
            return (int)value;
        } else {
            fprintf(stderr, "Errore: Valore fuori limite.\n");
            exit(1);
        }
    }
    return -1;
}
//legge il file di configurazione (riga per riga) e inizializza le variabili globali (nPartX, nPartY, nThreads, step)
void configurationReader() {
    int MAX_LINE_LENGTH = 20;
    int INT_MAX_LIMIT = 2147483647;

    char buffer[MAX_LINE_LENGTH];
    FILE *file = fopen("configuration.txt", "r");

    if (file) {
        nPartX = readConfigValue(file, buffer, MAX_LINE_LENGTH, INT_MAX_LIMIT);
        nPartY = readConfigValue(file, buffer, MAX_LINE_LENGTH, INT_MAX_LIMIT);
        nThreads = readConfigValue(file, buffer, MAX_LINE_LENGTH, INT_MAX_LIMIT);
        step = readConfigValue(file, buffer, MAX_LINE_LENGTH, INT_MAX_LIMIT);
        fclose(file);
    } else {
        fprintf(stderr, "Errore: Impossibile aprire il file di configurazione.\n");
        exit(1);
    }
}
//conta il numero di righe e colonne del file di input
void countRowsAndCols(FILE* inputFile, int& totalRows, int& totalCols) {
    char c;
    bool end = false, colsCounted = false;
    while (!end) {
        c = fgetc(inputFile);
        if (c == EOF) {
            end = true;
        } else if (c == '\n') {
            colsCounted = true;
            totalRows++;
        } else if (!colsCounted) {
            totalCols++;
        }
    }
}
void fillMatrix(FILE* inputFile, int* matTot, int totalRows, int totalCols) {
    rewind(inputFile);
    char c;
    bool end = false;
    int cont = 0;
    while (!end) {
        c = fgetc(inputFile);
        if (c == EOF) {
            end = true;
        } else if (c != '\n') {
            if (cont >= totalRows * totalCols) {
                perror("La matrice all'interno di input.txt ha un numero errato di righe/colonne");
                delete[] matTot;
                exit(1);
            }
            matTot[cont] = c - '0';
            cont++;
        }
    }
}
void inputDimensionReader() {
    totalRows = 0;
    totalCols = 0;

    FILE* inputFile = fopen("input.txt", "r");
    if (inputFile == NULL) {
        perror("Impossibile aprire il file di input");
        delete[] matTot;
        exit(1);
    }

    countRowsAndCols(inputFile, totalRows, totalCols);
    matTot = new int[totalRows * totalCols];
    fillMatrix(inputFile, matTot, totalRows, totalCols);

    fclose(inputFile);
}
int countMatProc(int arr[], int end) {
    int result = 0;
    for (int i = 0; i < end; i++) {
        result += arr[i];
    }
    return result;
}
void inputReader(){  // Serve per leggere, da input.txt, la sottomatrice processo corrente.
	int partXPrevious = rankX * nPartXForEachProc; //n° di partizioni lungo l'asse X precedenti all'attuale
	int partYPrevious = rankY * nPartYForEachProc; 
	beginYCurrProc = countMatProc(nRowsForEachPart, partYPrevious);
    beginXCurrProc = countMatProc(nColsForEachPart, partXPrevious);

	//vado a copiare nella matrice readM del processo attuale, la porzione corrispondente dalla matrice totale in input.txt 
    for (int i=0;i<nRowsCurrRank+2;i++) { //i e j potrebbero partire anche da 1, in quanto la prima è halo, ma conviene inizializzarle a 0, per avitare errori
        for (int j=0;j<nColsCurrRank+2;j++) {
            if (i==0 || i==nRowsCurrRank+1 || j==0 || j==nColsCurrRank+1) { //gli halo li inizializzo tutti a 0
                readM[v(i, j)] = 0;
            } else {
                readM[v(i, j)] = matTot[(i-1+beginYCurrProc) * totalCols + (j-1+beginXCurrProc)]; //se non è un halo border copio il valore dalla mat totale in input.txt
				//                        ^^ devo togliere i -1 per gli haloBoard
            }
            writeM[v(i, j)] = 0;
        }
    }
	if(rank != 0)
		delete [] matTot;
	return;
}
int countAliveNeighbors(int x, int y) {
    int count = 0;
    for (int dx = -1; dx < 2; ++dx){
        for (int dy = -1; dy < 2; ++dy) {
            if((dx != 0 || dy != 0) && readM[v((x+dx+totalRows) % totalRows, (y+dy+totalCols) % totalCols)] == 1) {
                count++;
            }
        }
	}
	return count;
}
void transitionFunction(int x, int y){ 
	//--------------------------------------------------------------------
	// Regole del gioco della vita:
	//	- se una cella viva ha meno di 2 o più di 3 vicini vivi --> MUORE
	//	- se una cella viva ha 2 o 3 vicini vivi --> SOPRAVVIVE
	//	- se una cella morta ha 3 vicini vivi --> NASCE
	//--------------------------------------------------------------------

	int cont = countAliveNeighbors(x, y); //Funzione per contare i vicini vivi

	//Applica le regole:
    if(readM[v(x, y)] == 1) {
        if(cont == 2 || cont == 3) {
            writeM[v(x, y)] = 1;
        }
        else {
            writeM[v(x, y)] = 0;
        }
    }
    else {
        if(cont == 3) {
            writeM[v(x, y)] = 1;
        }
        else {
            writeM[v(x, y)] = 0;
        }
    }
}
void execTransFunc(int beginThreadX, int beginThreadY, int sizePartizX, int sizePartizY){
	for(int i = beginThreadX; i < beginThreadX + sizePartizX; i++){
		for(int j = beginThreadY; j < beginThreadY + sizePartizY; j++){
			transitionFunction(j, i);
		}
	}
}

void recvMatTot(){   // Il processo 0 riceve la sottomatrice da ciascun processo per ricreare la mat totale
	int matIndex, tempIndex;
	//Prendo prima la sottomatrice (del rank 0) e la copio in matTot
	for(int i = 1; i < nRowsCurrRank + 2; i++){ //1 --> halo board
		for(int j = 1; j < nColsCurrRank + 2;j++){
			matIndex = (i-1) * totalCols + (j-1); 
			matTot[matIndex] = writeM[v(i,j)];}} 

	for(int rank = 1; rank < size; rank++){
		int temp[(nRowsForEachProc[rank]*nColsForEachProc[rank])]; //array dove salvo temporaneamente la matrice interna linearizzata che mi arriva da ogni thread 
		MPI_Recv(temp, nRowsForEachProc[rank]*nColsForEachProc[rank], MPI_INT, rank, 777, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		//una volta che ho ricevuto la sottomat di quel proc la copio in mat Tot
		for(int i=0;i<nRowsForEachProc[rank];i++){  //faccio un for sulle righe e colonne della sottomat (questo perte da 0 e non da 1 perche invio la mat interna) 
			for(int j=0;j<nColsForEachProc[rank];j++){
				matIndex = (i+startRowForEachProc[rank]) * totalCols + (j+startColForEachProc[rank]); //devo linearizzare l'indice per accedere alla pos in matTot
                tempIndex = i * nColsForEachProc[rank] + j; //anche temp è linearizzato quindi devo fare la stessa cosa
				matTot[matIndex] = temp[tempIndex];}}
	}
	return;
}

void initArrays(){ // Inizializza gli array che contengono varie informazioni per ciascun processo
	startColForEachProc = new int[size];
	startRowForEachProc = new int[size];
	nRowsForEachProc = new int[size];
	nColsForEachProc = new int[size];
}

void recvInfo(){   //riceve le info dagli altri processi (stampa a schermo) 
	//all'indice 0 metto tutte le sue informazioni
	startColForEachProc[0] = beginXCurrProc; //startColForEachProc e startRowForEachProc del processo 0 (rank 0) vengono inizializzati con i valori corrispondenti all'inizio delle colonne e delle righe della sua porzione.
	startRowForEachProc[0] = beginYCurrProc;  //beginXCurrProc e Y vengono calcolate con la funzione inputReader
	nRowsForEachProc[0] = nRowsCurrRank; //nRows e nCols ottenuti con la funzione initAllPartitions (parte finale della initAllPartitions)
	nColsForEachProc[0] = nColsCurrRank;

	//ricevere le informazioni sulle dimensioni delle sottomatrici assegnate a ciascun processo per la stampa grafica
	int info[4];
	MPI_Status stat;
	for(int i = 1; i < size; i++){
		MPI_Recv(info, 4, MPI_INT, MPI_ANY_SOURCE, 222, MPI_COMM_WORLD, &stat);   
		startColForEachProc[stat.MPI_SOURCE] = info[0];
		startRowForEachProc[stat.MPI_SOURCE] = info[1];	
		nColsForEachProc[stat.MPI_SOURCE] = info[2]; 
		nRowsForEachProc[stat.MPI_SOURCE] = info[3];
	}
}

void sendInfo(){ //ogni proc con rank!=0 invia tutte le sue info allo 0
	int info[4] = {beginXCurrProc, beginYCurrProc, nColsCurrRank, nRowsCurrRank};
	MPI_Send(info, 4, MPI_INT, rankMaster, 222, MPI_COMM_WORLD); 
}

void swap(){
	int* p = readM;
    readM = writeM;
    writeM = p;
}

void initBarriers(){ 
	//inizializzo barriera per le fasi di comunicazione
	contComunicationBarrier = 0;
	pthread_mutex_init(&mutexComunicationBarrier, NULL);

	//inizializzo barriera per far si che tutti abbiano finito la computazione
    contcompletedBarrier = 0;
	pthread_mutex_init(&mutexcompletedBarrier, NULL);
    
	//inizializzo le condition per le barrier precedenti
    pthread_cond_init(&condComunicationBarrier, NULL);
    pthread_cond_init(&condcompletedBarrier, NULL);
}

void handleBarrier(pthread_mutex_t* mutex, int* counter, pthread_cond_t* condition) {
    pthread_mutex_lock(mutex);
    (*counter)++;
    if(*counter < nThreads){
        pthread_cond_wait(condition, mutex);
    }
    *counter = 0;
    pthread_cond_broadcast(condition);
    pthread_mutex_unlock(mutex);
}
void completedBarrier(){ 
    handleBarrier(&mutexcompletedBarrier, &contcompletedBarrier, &condcompletedBarrier);
}
void comunicationBarrier(){ 
    handleBarrier(&mutexComunicationBarrier, &contComunicationBarrier, &condComunicationBarrier);
}
void destroyBarriers(){
	pthread_mutex_destroy(&mutexComunicationBarrier);
	pthread_mutex_destroy(&mutexcompletedBarrier);
}
void destroyConditions(){ 
	pthread_cond_destroy(&condComunicationBarrier);
	pthread_cond_destroy(&condcompletedBarrier);
}
void destroy(){
	destroyBarriers();
	destroyConditions();
}

void * runThread(void * arg){
	int tid = *(int *)arg;
	int tidX = tid % nPartXForEachProc;  //calcolo l'tid lungo lasse X della partizione
	int tidY = tid / nPartXForEachProc; 
	int partXPrevious = rankX * nPartXForEachProc + tidX;  //numero di partizioni lungo l'asse X che ci sono prima dell'attuale
	int partYPrevious = rankY * nPartYForEachProc + tidY;
	int beginThreadX = 1; //la partizione inizia dalla prima riga. (perchè la 0 è halo)
	int beginThreadY = 1;

	for(int j = rankX * nPartXForEachProc; j<partXPrevious; ++j) beginThreadX += nColsForEachPart[j]; 
	for(int i = rankY * nPartYForEachProc; i<partYPrevious; ++i) beginThreadY += nRowsForEachPart[i];
    for(int s = 0; s < step; s++){
        comunicationBarrier();   //sincronizzo tutti i thread prima di procedere. Mi devo assicurare che tutti i processi abbiano ricevuto i dati corretti prima di continuare
		//Perche serve la barrier di comunicazione? mentre i thread vengono creai ed eseguono la loro funzione, il mainThread è andao avanti nel main è sta facendo 
		// la exchangeBorders 
		execTransFunc(beginThreadX, beginThreadY, nColsForEachPart[partXPrevious], nRowsForEachPart[partYPrevious]); //inizioX e Y, size X e Y
        completedBarrier(); //assicura che tutti i thread abbiano completato l'esecuzione di execTransFunc prima di procedere.Altrimenti alcuni potrebbero andare
		//al passo successivo mentre altri stanno ancora computando, possibili problemi.
    }
    return NULL;
}

void initializeThreads(){
    IDthreads = new pthread_t[nThreads]; 
    for(int i=1;i<nThreads;i++){  //non parto da 0 ma da 1, in quanto ho già il main thread che ha rank0.
		//ad esempio se nThread=8 ----> rank0=mainThread  + 7 thread creati da me. Nel frattempo anche al main thread faccio fare la sua parte
        int * tid = new int(i);   //idem
        pthread_create(&IDthreads[i], NULL, runThread, (void *) tid);  //posso togliere (void*)
    }
}
void createDatatype(){
	MPI_Type_vector(nRowsCurrRank+2, 1, nColsCurrRank+2, MPI_INT, &typeColumn); 
	MPI_Type_vector(nRowsCurrRank, nColsCurrRank, nColsCurrRank+2, MPI_INT, &typeMatLessHaloBorders);
	MPI_Type_commit(&typeColumn);
	MPI_Type_commit(&typeMatLessHaloBorders);
}
void freeDatatype(){
	MPI_Type_free(&typeColumn);
	MPI_Type_free(&typeMatLessHaloBorders);
}


int main(int argc, char* argv[]){
	
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	configurationReader();  //legge congig.txt e calcola nPartX e nPartY
    if (size != nPartX * nPartY / nThreads){   // se non sono divisibili chiudo il programma
        exit(1);  
	}
	inputDimensionReader(); //calcola totalCols e totalRows, inizializza matTot e ci copia tutti i valori dell'input.txt
	initAllPartitions(); //trova una divisione ottimale calcolando: nPartYForEachProc,nPartXForEachProc. Inizializza gli array che calcolano le dimensioni di ciascuna
	//partizione nRowsForEachPart,nColsForEachPart. Scompone il rank del proc attuale,calcola il numero di proc lungo X, calcola il numero delle righe e colonne 
	//del proc attuale
	calculateMooreNeighbourhood();  //Calcola i rank dei processi vicini 
	initArrays();
	inputReader();  //inizializza la readM e writeM del proc attuale leggendo input.txt
	
	//creazione e inizializzazione dei typeVector rappresentanti colonna e matrice interna
	createDatatype();
    initBarriers();
	
	inizio = clock();
	if(rank==0){
		if(allegroOn){
			initAllegro();
			drawWithAllegro(0);
		}
		recvInfo(); //rank0 riceve tutto, altrimenti inviano le informazioni
	}
	else{
		sendInfo();
	}

    initializeThreads();
	for (int i = 0; i < step; i++){
		//sleep(1); //attivare solo per rallentare gli step --> togliere per calcolare il tempo
		exchangeBordersMoore();
        comunicationBarrier(); //main thread arriva alla barriera. Presente sia qua che nella funzione degli altri thread creati. Una volta che sono arrivati
		//tutti, escono dalla barriera. Sicuro i bordi saranno stati inviati e ricevuti, e tutti posso computare.
		execTransFunc(1, 1, nColsForEachPart[rankX * nPartXForEachProc], nRowsForEachPart[rankY * nPartYForEachProc]);  //Nel frattempo faccio computare anche al main thread.
		//Il main thread computerà ogni volta la partizione iniziale (1,1)
    	completedBarrier(); //stesso ragionamento ma dopo, se non metto la barrier, e ad esempio il main thread finisce prima, continua l'esecuzione del 
		// main e potrebbe aggiornare la grafica, senza che tutti abbiano finito
		if(allegroOn){
			if(rank == 0){
				recvMatTot();
				drawWithAllegro(i + 1);
			}
			else
				MPI_Send(&writeM[v(1,1)], 1, typeMatLessHaloBorders, rankMaster, 777, MPI_COMM_WORLD);  //invio la matrice interna (senza halo), se non sono il master
		}
		swap();
	}
	fine = clock();
	if(rank == 0){
		if(allegroOn)
			allegro_exit();
		double tempoEsec = (double)(fine-inizio)/CLOCKS_PER_SEC;
		printf("\nTEMPO DI ESECUZIONE TOTALE: %fs \n", tempoEsec);
	}
	
	destroy();
	freeDatatype();
	
	MPI_Finalize();
	return 0;
}

//mpic++ GirlsProject.cpp -I/usr/include -L/usr/lib/x86_64-linux-gnu -lalleg -o GirlsProject