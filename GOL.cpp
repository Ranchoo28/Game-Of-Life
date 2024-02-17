#include "myallegro.h"
#include <pthread.h>
#include <allegro.h>

#define v(r,c) (r)*(nColsThisRank+2)+(c)

void distributeRowsAndCols(int total, int numPartitions, int* partitionSizes) { // Serve per distribuire il resto. Total può essere totRows o totCols
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
    nRowsPerPartition = new int[nPartY]; //quante righe di celle per ogni partizione
    nColsPerPartition = new int[nPartX]; //quante colonne di celle per ogni partizione
	
    // Trova un divisore comune (i) per nThreads e nPartX o nThreads e nPartY
    //(in base alla formula size* nThreads = nPartX * nPartY si sa che ne esiste sempre uno)
	// Decide COME suddividere la matrice totale tra i processi (quali sottomatrici assegnare a ciascun processo)
    for (int i = nThreads; i > 0; i--) {
        if ((nThreads % i == 0) && (nPartX % i == 0)) { //se i è un divisore comune di nThreads e nPartX
            nPartYPerProc = nThreads / i; //per fare in modo che ad ogni thread venga assegnata una partizione: (nThreads /1)*1= nThreads
            nPartXPerProc = i;
            break;
        } else if ((nThreads % i == 0) && (nPartY % i == 0)) { //se i è un divisore comune di nThreads e nPartY
            nPartYPerProc = i;
            nPartXPerProc = nThreads / i;
            break;
        } 
	}

	//Distribuisco le righe e le colonne tra le partizioni (considerando anche il resto, ovvero il remainder)
    distributeRowsAndCols(totRows, nPartY, nRowsPerPartition);
    distributeRowsAndCols(totCols, nPartX, nColsPerPartition);

    // Calcola il numero di processi lungo l'asse X
	// Quante sottomatrici finiscono su X 
    nProcOnX = nPartX / nPartXPerProc;

    // Scomponi il rank del processo --> COORDINATE DEL PROCESSO
    rankX = rank % nProcOnX;
    rankY = rank / nProcOnX;

    // Calcola le dimensioni della sottomatrice per il processo corrente
    nRowsThisRank = sumPartitions(rankY * nPartYPerProc, nPartYPerProc, nRowsPerPartition);
    nColsThisRank = sumPartitions(rankX * nPartXPerProc, nPartXPerProc, nColsPerPartition);

    // Inizializza matrici di lettura e scrittura
    readM = new int[(nRowsThisRank + 2) * (nColsThisRank + 2)]; //+2 perchè si considerano le halo cells
    writeM = new int[(nRowsThisRank + 2) * (nColsThisRank + 2)];
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
    MPI_Isend(&readM[v(1,1)], nColsThisRank, MPI_INT, rankUp, 17, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(nRowsThisRank, 1)], nColsThisRank, MPI_INT, rankDown, 20, MPI_COMM_WORLD, &req);

    MPI_Recv(&readM[v(nRowsThisRank+1, 1)], nColsThisRank, MPI_INT, rankDown, 17, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(0,1)], nColsThisRank, MPI_INT, rankUp, 20, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}
void sendRecvCols(int rankLeft, int rankRight, MPI_Request &req) {
    MPI_Isend(&readM[v(0, 1)], 1, typeColumn, rankLeft, 12, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(0, nColsThisRank)], 1, typeColumn, rankRight, 30, MPI_COMM_WORLD, &req);

    MPI_Recv(&readM[v(0, 0)], 1, typeColumn, rankLeft, 30, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(0, nColsThisRank+1)], 1, typeColumn, rankRight, 12, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}
void sendRecvCorners(int rankUpLeft, int rankUpRight, int rankDownLeft, int rankDownRight, MPI_Request &req) {
    MPI_Isend(&readM[v(1, 1)], 1, MPI_INT, rankUpLeft, 40, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(1, nColsThisRank)], 1, MPI_INT, rankUpRight, 41, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(nRowsThisRank, 1)], 1, MPI_INT, rankDownLeft, 42, MPI_COMM_WORLD, &req);
    MPI_Isend(&readM[v(nRowsThisRank, nColsThisRank)], 1, MPI_INT, rankDownRight, 43, MPI_COMM_WORLD, &req);

    MPI_Recv(&readM[v(nRowsThisRank+1, 0)], 1, MPI_INT, rankDownLeft, 40, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(nRowsThisRank+1, nColsThisRank+1)], 1, MPI_INT, rankDownRight, 41, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(0, 0)], 1, MPI_INT, rankUpLeft, 42, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&readM[v(0, nColsThisRank+1)], 1, MPI_INT, rankUpRight, 43, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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
void countRowsAndCols(FILE* inputFile, int& totRows, int& totCols) {
    char c;
    bool end = false, colsCounted = false;
    while (!end) {
        c = fgetc(inputFile);
        if (c == EOF) {
            end = true;
        } else if (c == '\n') {
            colsCounted = true;
            totRows++;
        } else if (!colsCounted) {
            totCols++;
        }
    }
}
void fillMatrix(FILE* inputFile, int* matrix, int totRows, int totCols) {
    rewind(inputFile);
    char c;
    bool end = false;
    int cont = 0;
    while (!end) {
        c = fgetc(inputFile);
        if (c == EOF) {
            end = true;
        } else if (c != '\n') {
            if (cont >= totRows * totCols) {
                printf("Righe: %d, Colonne: %d, cont: %d\n", totRows, totCols, cont);
                perror("La matrice all'interno di input.txt ha un numero errato di righe/colonne");
                delete[] matrix;
                exit(1);
            }
            matrix[cont] = c - '0';
            cont++;
        }
    }
}
void inputDimensionReader() {
    totRows = 0;
    totCols = 0;

    FILE* inputFile = fopen("input.txt", "r");
    if (inputFile == NULL) {
        perror("Impossibile aprire il file di input");
        delete[] matrix;
        exit(1);
    }

    countRowsAndCols(inputFile, totRows, totCols);
    matrix = new int[totRows * totCols];
    fillMatrix(inputFile, matrix, totRows, totCols);

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
	int partXPrevious = rankX * nPartXPerProc; //n° di partizioni lungo l'asse X precedenti all'attuale
	int partYPrevious = rankY * nPartYPerProc; 
	startYThisProc = countMatProc(nRowsPerPartition, partYPrevious);
    startXThisProc = countMatProc(nColsPerPartition, partXPrevious);

	//vado a copiare nella matrice readM del processo attuale, la porzione corrispondente dalla matrice totale in input.txt 
    for (int i=0;i<nRowsThisRank+2;i++) { //i e j potrebbero partire anche da 1, in quanto la prima è halo, ma conviene inizializzarle a 0, per avitare errori
        for (int j=0;j<nColsThisRank+2;j++) {
            if (i==0 || i==nRowsThisRank+1 || j==0 || j==nColsThisRank+1) { //gli halo li inizializzo tutti a 0
                readM[v(i, j)] = 0;
            } else {
                readM[v(i, j)] = matrix[(i-1+startYThisProc) * totCols + (j-1+startXThisProc)]; //se non è un halo border copio il valore dalla mat totale in input.txt
				//                        ^^ devo togliere i -1 per gli haloBoard
            }
            writeM[v(i, j)] = 0;
        }
    }
	if(rank != 0)
		delete [] matrix;
	return;
}
int countAliveNeighbors(int x, int y) {
    int count = 0;
    for (int dx = -1; dx < 2; ++dx){
        for (int dy = -1; dy < 2; ++dy) {
            if((dx != 0 || dy != 0) && readM[v((x+dx+totRows) % totRows, (y+dy+totCols) % totCols)] == 1) {
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

void recvmatrix(){   // Il processo 0 riceve la sottomatrice da ciascun processo per ricreare la mat totale
	int matIndex, tempIndex;
	//Prendo prima la sottomatrice (del rank 0) e la copio in matrix
	for(int i = 1; i < nRowsThisRank + 2; i++){ //1 --> halo board
		for(int j = 1; j < nColsThisRank + 2;j++){
			matIndex = (i-1) * totCols + (j-1); 
			matrix[matIndex] = writeM[v(i,j)];}} 

	for(int rank = 1; rank < size; rank++){
		int temp[(vecRowSizePerProc[rank]*vecColSizePerProc[rank])]; //array dove salvo temporaneamente la matrice interna linearizzata che mi arriva da ogni thread 
		MPI_Recv(temp, vecRowSizePerProc[rank]*vecColSizePerProc[rank], MPI_INT, rank, 777, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		//una volta che ho ricevuto la sottomat di quel proc la copio in mat Tot
		for(int i=0;i<vecRowSizePerProc[rank];i++){  //faccio un for sulle righe e colonne della sottomat (questo perte da 0 e non da 1 perche invio la mat interna) 
			for(int j=0;j<vecColSizePerProc[rank];j++){
				matIndex = (i+startRowPerProc[rank]) * totCols + (j+startColPerProc[rank]); //devo linearizzare l'indice per accedere alla pos in matrix
                tempIndex = i * vecColSizePerProc[rank] + j; //anche temp è linearizzato quindi devo fare la stessa cosa
				matrix[matIndex] = temp[tempIndex];}}
	}
	return;
}

void initArrays(){ // Inizializza gli array che contengono varie informazioni per ciascun processo
	startColPerProc = new int[size];
	startRowPerProc = new int[size];
	vecRowSizePerProc = new int[size];
	vecColSizePerProc = new int[size];
}

void recvInfo(){   //riceve le info dagli altri processi (stampa a schermo) 
	//all'indice 0 metto tutte le sue informazioni
	startColPerProc[0] = startXThisProc; //startColPerProc e startRowPerProc del processo 0 (rank 0) vengono inizializzati con i valori corrispondenti all'inizio delle colonne e delle righe della sua porzione.
	startRowPerProc[0] = startYThisProc;  //startXThisProc e Y vengono calcolate con la funzione inputReader
	vecRowSizePerProc[0] = nRowsThisRank; //nRows e nCols ottenuti con la funzione initAllPartitions (parte finale della initAllPartitions)
	vecColSizePerProc[0] = nColsThisRank;

	//ricevere le informazioni sulle dimensioni delle sottomatrici assegnate a ciascun processo per la stampa grafica
	int info[4];
	MPI_Status stat;
	for(int i = 1; i < size; i++){
		MPI_Recv(info, 4, MPI_INT, MPI_ANY_SOURCE, 222, MPI_COMM_WORLD, &stat);   
		startColPerProc[stat.MPI_SOURCE] = info[0];
		startRowPerProc[stat.MPI_SOURCE] = info[1];	
		vecColSizePerProc[stat.MPI_SOURCE] = info[2]; 
		vecRowSizePerProc[stat.MPI_SOURCE] = info[3];
	}
}

void sendInfo(){ //ogni proc con rank!=0 invia tutte le sue info allo 0
	int info[4] = {startXThisProc, startYThisProc, nColsThisRank, nRowsThisRank};
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
    contReadyBarrier = 0;
	pthread_mutex_init(&mutexReadyBarrier, NULL);
    
	//inizializzo le condition per le barrier precedenti
    pthread_cond_init(&condComunicationBarrier, NULL);
    pthread_cond_init(&condReadyBarrier, NULL);
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
    handleBarrier(&mutexReadyBarrier, &contReadyBarrier, &condReadyBarrier);
}
void comunicationBarrier(){ 
    handleBarrier(&mutexComunicationBarrier, &contComunicationBarrier, &condComunicationBarrier);
}
void destroyBarriers(){
	pthread_mutex_destroy(&mutexComunicationBarrier);
	pthread_mutex_destroy(&mutexReadyBarrier);
}
void destroyConditions(){ 
	pthread_cond_destroy(&condComunicationBarrier);
	pthread_cond_destroy(&condReadyBarrier);
}
void destroy(){
	destroyBarriers();
	destroyConditions();
}

void * runThread(void * arg){
	int tid = *(int *)arg;
	int tidX = tid % nPartXPerProc;  //calcolo l'tid lungo lasse X della partizione
	int tidY = tid / nPartXPerProc; 
	int partXPrevious = rankX * nPartXPerProc + tidX;  //numero di partizioni lungo l'asse X che ci sono prima dell'attuale
	int partYPrevious = rankY * nPartYPerProc + tidY;
	int beginThreadX = 1; //la partizione inizia dalla prima riga. (perchè la 0 è halo)
	int beginThreadY = 1;

	for(int j = rankX * nPartXPerProc; j<partXPrevious; ++j) beginThreadX += nColsPerPartition[j]; 
	for(int i = rankY * nPartYPerProc; i<partYPrevious; ++i) beginThreadY += nRowsPerPartition[i];
    for(int s = 0; s < step; s++){
        comunicationBarrier();   //sincronizzo tutti i thread prima di procedere. Mi devo assicurare che tutti i processi abbiano ricevuto i dati corretti prima di continuare
		//Perche serve la barrier di comunicazione? mentre i thread vengono creai ed eseguono la loro funzione, il mainThread è andao avanti nel main è sta facendo 
		// la exchangeBorders 
		execTransFunc(beginThreadX, beginThreadY, nColsPerPartition[partXPrevious], nRowsPerPartition[partYPrevious]); //inizioX e Y, size X e Y
        completedBarrier(); //assicura che tutti i thread abbiano completato l'esecuzione di execTransFunc prima di procedere.Altrimenti alcuni potrebbero andare
		//al passo successivo mentre altri stanno ancora computando, possibili problemi.
    }
    return NULL;
}

void initializeThreads(){
    vecThreads = new pthread_t[nThreads]; 
    for(int i=1;i<nThreads;i++){  //non parto da 0 ma da 1, in quanto ho già il main thread che ha rank0.
		//ad esempio se nThread=8 ----> rank0=mainThread  + 7 thread creati da me. Nel frattempo anche al main thread faccio fare la sua parte
        int * tid = new int(i);   //idem
        pthread_create(&vecThreads[i], NULL, runThread, (void *) tid);  //posso togliere (void*)
    }
}
void createDatatype(){
	MPI_Type_vector(nRowsThisRank+2, 1, nColsThisRank+2, MPI_INT, &typeColumn); 
	MPI_Type_vector(nRowsThisRank, nColsThisRank, nColsThisRank+2, MPI_INT, &typeMatWithoutHalos);
	MPI_Type_commit(&typeColumn);
	MPI_Type_commit(&typeMatWithoutHalos);
}
void freeDatatype(){
	MPI_Type_free(&typeColumn);
	MPI_Type_free(&typeMatWithoutHalos);
}


int main(int argc, char* argv[]){
	
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	configurationReader();  //legge congig.txt e calcola nPartX e nPartY
    if (size != nPartX * nPartY / nThreads){   // se non sono divisibili chiudo il programma
        exit(1);  
	}
	inputDimensionReader(); //calcola totCols e totRows, inizializza matrix e ci copia tutti i valori dell'input.txt
	initAllPartitions(); //trova una divisione ottimale calcolando: nPartYPerProc,nPartXPerProc. Inizializza gli array che calcolano le dimensioni di ciascuna
	//partizione nRowsPerPartition,nColsPerPartition. Scompone il rank del proc attuale,calcola il numero di proc lungo X, calcola il numero delle righe e colonne 
	//del proc attuale
	calculateMooreNeighbourhood();  //Calcola i rank dei processi vicini 
	initArrays();
	inputReader();  //inizializza la readM e writeM del proc attuale leggendo input.txt
	
	//creazione e inizializzazione dei typeVector rappresentanti colonna e matrice interna
	createDatatype();
    initBarriers();
	
	inizio = clock();
	if(rank==0){
		if(allegroRun){
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
		sleep(1); //attivare solo per rallentare gli step --> togliere per calcolare il tempo
		exchangeBordersMoore();
        comunicationBarrier(); //main thread arriva alla barriera. Presente sia qua che nella funzione degli altri thread creati. Una volta che sono arrivati
		//tutti, escono dalla barriera. Sicuro i bordi saranno stati inviati e ricevuti, e tutti posso computare.
		execTransFunc(1, 1, nColsPerPartition[rankX * nPartXPerProc], nRowsPerPartition[rankY * nPartYPerProc]);  //Nel frattempo faccio computare anche al main thread.
		//Il main thread computerà ogni volta la partizione iniziale (1,1)
    	completedBarrier(); //stesso ragionamento ma dopo, se non metto la barrier, e ad esempio il main thread finisce prima, continua l'esecuzione del 
		// main e potrebbe aggiornare la grafica, senza che tutti abbiano finito
		if(allegroRun){
			if(rank == 0){
				recvmatrix();
				drawWithAllegro(i + 1);
			}
			else
				MPI_Send(&writeM[v(1,1)], 1, typeMatWithoutHalos, rankMaster, 777, MPI_COMM_WORLD);  //invio la matrice interna (senza halo), se non sono il master
		}
		swap();
	}
	fine = clock();

	if(rank == 0){
		if(allegroRun)
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
//mpic++ GOL.cpp -I/usr/include -L/usr/lib/x86_64-linux-gnu -lalleg -o GOL