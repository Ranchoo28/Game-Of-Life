#ifndef __GLOBALS__H__
#define __GLOBALS__H__

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <mpi.h>  
#include <allegro.h>

//configuration.txt
int nPartX, nPartY;		// N° partizioni lungo asse x (orizzontale)	e lungo asse y (verticale)
int nThreads;				// N° di thread
int step;			 		// N° di step

//input.txt
int totRows, totCols;	// N° righe/colonne totali della matrice 

//Valori dell'ambiente MPI
int size;		// N° complessivo di processi
int rank;		// id del processo attuale

//Variabili inizializzate dalla funzione initAllPartitions
int nRowsThisRank, nColsThisRank;			// N° di righe/colonne del processo corrente

int *nRowsPerPartition, *nColsPerPartition;	// Array che contengono, per ogni partizione, il n° di righe/colonne al proprio interno (nella partizione)
//##################### ad esempio nRowsPerPartition[0] conterrà il n° di righe nella prima partizione e nColsPerPartition[0] il n° di colonne.

int nPartXPerProc, nPartYPerProc;	// N° di partizioni X(oriz)/partizioni Y(vert) che contine ciascun processo

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
int startXThisProc, startYThisProc;				// indice d'inizio delle colonne (su x)/ righe (su y) del processo corrente
int *startColPerProc, *startRowPerProc;	// array che contengono, per ogni posizione, gli indici d'inizio delle colonne/righe di ciascun processo

//SIZE DEI PROCESSI
int *vecRowSizePerProc, *vecColSizePerProc;	// array che contengono, per ogni posizione, la sizeY (rows) / sizeX (cols) di ciascun processo

//MATRICE TOTALE
int *matrix;	// matrice totale: che viene usata dal rank 0 per ricevere le matrici dagli altri processi e fare la stampa dell'AC

//Datatypes
MPI_Datatype typeColumn;				// Datatype per inviare una colonna
MPI_Datatype typeMatWithoutHalos;	// Datatype per inviare una matrice SENZA gli halo borders (mat interna) 

//Variabili PTHREADS
pthread_t * vecThreads;	// array di thread

//BARRIERS
pthread_mutex_t mutexReadyBarrier;    				// mutex barriera di computazione dell'AC
pthread_mutex_t mutexComunicationBarrier;   			// mutex per la barriera di comunicazione delle send /receive
int contReadyBarrier =0, contComunicationBarrier=0;	//contatori per le barrier

//CONDITIONS
pthread_cond_t condReadyBarrier;      	// condition per la completedBarrier
pthread_cond_t condComunicationBarrier;     // condition per la ComunicationBarrier


//Variabili per ALLEGRO
clock_t inizio, fine;		// tempo all'inizio e alla fine dell'algoritmo per calcolare la durata
int delayAllegro = 80;		// delay per la stampa a schermo di allegro
int graphicCellDim = 50;	// dimensione grafica di una cella
int infoLine = 55;			// spazio per scrivere i dati dell'AC
bool allegroRun = true; 

#endif // !__GLOBALS__H__