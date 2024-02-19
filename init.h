#ifndef __INIT__H__
#define __INIT__H__

#include "golfunctions.h"

void distributeRowsAndCols(int total, int numPartitions, int* partitionSizes) { // Serve per distribuire il resto. Total può essere totRows o totCols
    int resto = total % numPartitions; //remainder è il resto (nel caso in cui non sia divisibile)

    for (int i = 0; i < numPartitions; i++) {
        partitionSizes[i] = total / numPartitions; //viene assegnata la size (minima, senza remainder) ad ogni partizione
        if (i < resto) partitionSizes[i]++; //se ci sono ancora righe nel remainder da assegnare, la dà all'attuale partizione
    }
}

int sumPartitions(int start, int numPartitions, int* partitionSizes) { // Serve per calcolare la size di una sottomatrice. Start è l'indice di partenza
    int sum = 0;
    for (int i = 0; i < numPartitions; i++) { //numPartitions è il numero di partizioni che devo sommare assegnate al current rank
        sum += partitionSizes[start + i]; //partitionSizes è l'array che contiene le size  (numero di celle piccole) di tutte le partizioni
    }
    return sum; //restituisce la size (numero di celle totali) della sottomatrice del processo corrente
}

void initPartitions() {

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

void initArrays(){ // Inizializza gli array che contengono varie informazioni per ciascun processo
	startColPerProc = new int[size];
	startRowPerProc = new int[size];
	vecRowSizePerProc = new int[size];
	vecColSizePerProc = new int[size];
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
#endif //!__INIT__H__