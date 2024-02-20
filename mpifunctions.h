#ifndef __MPIFUNCTIONS__H__
#define __MPIFUNCTIONS__H__

#include "readingconf.h"

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

void recvmatrix(){   // Il processo 0 riceve la sottomatrice da ciascun processo per ricreare la mat totale
	int matIndex, tempIndex;
	//Prendo prima la sottomatrice (del rank 0) e la copio in matrix
	for(int i = 1; i < nRowsThisRank + 2; i++){ //1 --> halo board
		for(int j = 1; j < nColsThisRank + 2;j++){
			matIndex = (i-1) * totCols + (j-1); 
			matrix[matIndex] = writeM[v(i,j)];
		}
	} 

	for(int rank = 1; rank < size; rank++){
		int temp[(vecRowSizePerProc[rank]*vecColSizePerProc[rank])]; //array dove salvo temporaneamente la matrice interna linearizzata che mi arriva da ogni thread 
		MPI_Recv(temp, vecRowSizePerProc[rank]*vecColSizePerProc[rank], MPI_INT, rank, 777, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		//una volta che ho ricevuto la sottomat di quel proc la copio in mat Tot
		for(int i=0;i<vecRowSizePerProc[rank];i++){  //faccio un for sulle righe e colonne della sottomat (questo perte da 0 e non da 1 perche invio la mat interna) 
			for(int j=0;j<vecColSizePerProc[rank];j++){

				matIndex = (i+startRowPerProc[rank]) * totCols + (j+startColPerProc[rank]); //devo linearizzare l'indice per accedere alla pos in matrix
                tempIndex = i * vecColSizePerProc[rank] + j; //anche temp è linearizzato quindi devo fare la stessa cosa
				matrix[matIndex] = temp[tempIndex];
			}
		}
	}
	return;
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

#endif //!__MPIFUNCTIONS__H__