//Jiwon Shin
//js6450

#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	int i, j, k, l, d, temp;
	if(N == 32){
	for (i = 0; i < N; i+=8){
        	for (j = 0; j < M; j+=8){
			for(k = j; k < j+8; k++){
				for(l = i; l < i+8; l++){
					if(l == k){
						d = l;
						temp = A[d][d];
					}
					else{
						B[l][k] = A[k][l];
					}
				}
				if(i == j){
					B[d][d] = temp;
				}
			}
		}
	return;
	}
	if(N == 64){
	for (i = 0; i < N; i+=4) 
        	for (j = 0; j < M; j += 4)
			for(k = j; k< j+4; k++){
				for(l = i; l < i+4; l++){
					if(l == k){
						d = l;
						temp = A[d][d];
					}
					else{
						B[l][k] = A[k][l];
					}
				}
				if(i == j){
					B[d][d] = temp;
				}
			}
		}
	return;
	}
	else{
	for (i = 0; i < M; i += 16)
        	for (j = 0; j < N; j += 16)
			for(k = j; (k < j+16) && (k < N) ; k++){
				for(l = i; (l < i+16) && (l < M); l++){
					if(l == k){
						d = l;
						temp = A[d][d];
					}
					else{
						B[l][k] = A[k][l];
					}
				}
				if(i == j){
					B[d][d] = temp;
				}
			}
	}
}


char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions 
    registerTransFunction(trans, trans_desc); */

}

int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

