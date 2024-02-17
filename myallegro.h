#ifndef __MYALLEGRO__H__
#define __MYALLEGRO__H__


#include "globals.h"

void initAllegro(){ 
    allegro_init();
	install_keyboard();
    set_color_depth(32);  
	set_window_title("Musalee & Ranchoo APSD Project");
    set_gfx_mode(GFX_AUTODETECT_WINDOWED, totCols * graphicCellDim, totRows * graphicCellDim + infoLine, 0, 0);  
}
void drawWithAllegro(int stepCurr){  //funzione per la stampa a schermo tramite allegro
	int endY = totRows * graphicCellDim+5; //inizia poco dopo la fineX della matrice sulle X
	
	for(int i = 0; i < totRows; i++){
		for(int j = 0; j < totCols; j++){
			int x = j * graphicCellDim;
			int y = i * graphicCellDim;
			if (matrix[i * totCols + j] == 1) rectfill(screen, x, y, x + graphicCellDim, y + graphicCellDim,  makecol(50,205,50)); //Cella VIVA
			else rectfill(screen, x, y, x + graphicCellDim, y + graphicCellDim, makecol(255,255,255));							//Cella MORTA
		}
	}
    rectfill(screen, 0, endY, totCols * graphicCellDim, totRows * graphicCellDim + infoLine, makecol(0, 162, 255));

	// Stampa a schermo --> GRIGLIA
	for(int i = 0; i < totRows; i++) line(screen, 0, i * graphicCellDim, totCols * graphicCellDim, i * graphicCellDim, makecol(219, 209, 197));
	for(int i = 0; i < totCols; i++) line(screen, i * graphicCellDim, 0, i * graphicCellDim, totRows * graphicCellDim, makecol(219, 209, 197));

	
	//Stampa a schermo --> PARTIZIONI VERTICALI 
	int y=0;
	for(int i = 0; i < nPartY; i++){
		if(i % nPartYPerProc == 0) line(screen, 0, y, totCols * graphicCellDim, y, makecol(29, 120, 116));  //Processi
		else line(screen, 0, y, totCols * graphicCellDim, y, makecol(250, 157, 50));							  //Thread (divisioni nei processi)
		y += nRowsPerPartition[i] * graphicCellDim;
	}

	//Stampa a schermo --> PARTIZIONI ORIZZONTALI 
	int x=0;
	for(int i = 0; i < nPartX; i++){
		if(i%nPartXPerProc == 0) line(screen, x, 0, x, totRows * graphicCellDim, makecol(189, 33, 4)); 
		else line(screen, x, 0, x, totRows * graphicCellDim, makecol(193, 140, 93));
		x += nColsPerPartition[i] * graphicCellDim;
	}

	//Stampa a schermo --> INFO
	char str[64];
	sprintf(str, "Step %d su %d", stepCurr, step);
	textout_ex(screen, font, str, 0, endY+1, makecol(255, 255, 255), -1); //+1 per farlo partire poco dopo la fineY della matrice
	sprintf(str, "N° di partizioni su X: %d",nPartX);
	textout_ex(screen, font, str, 0, endY+15, makecol(255, 255, 255), -1); // Testo bianco
	sprintf(str, "N° di partizioni su Y: %d",nPartY);
	textout_ex(screen, font, str, 0, endY+26, makecol(255, 255, 255), -1); // Testo bianco
    sprintf(str, "N° di Thread: %d",nThreads);	
    textout_ex(screen, font, str, 0, endY+37, makecol(255, 255, 255), -1); // Testo bianco
	sprintf(str, "Dimensioni TOT dell'AC: %d x %d",totRows, totCols);
	textout_ex(screen, font, str, 250, endY+26, makecol(255, 255, 255), -1); // Testo bianco
	usleep(delayAllegro * 1000);
}


#endif //!__MYALLEGRO__H__