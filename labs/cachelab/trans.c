/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	int i; 
	int j; 
	int row; 
	int column;
	int diagonal = 0;
	int temp = 0;

    // Evaluate each of the test cases
	if (N == 32) {
        // Iterate across block
		for (column = 0; column < N; column += 8) { 
			for (row = 0; row < N; row += 8) {
                // Iterate over each block element
				for (i = row; i < row + 8; i++) { 
					for (j = column; j < column + 8; j++) {
						if (i != j) {
							B[j][i] = A[i][j];
						} else {
                            // Use temp to store value and reduce 
                            // conflict misses
							temp = A[i][j];
							diagonal = i;
						}
					}
                    // Store the diagonal element for further
                    // reduction
					if (row == column) {
						B[diagonal][diagonal] = temp; 
					}
				}	
			}
		}
	} else if (N == 64) {
		for (column = 0; column < N; column += 4) { 
			for (row = 0; row < N; row += 4) {
				for (i = row; i < row + 4; i++) {
					for (j = column; j < column + 4; j++) {
						if (i != j) {
							B[j][i] = A[i][j];
						} else {
							temp = A[i][j]; 
							diagonal = i;
						}
					}
					if (row == column) {
						B[diagonal][diagonal] = temp;
					}
				}	
			}
		}
	} 
    // For the last test case, we have to be careful of seg faults
	else {
		for (column = 0; column < M; column += 16) {
			for (row = 0; row < N; row += 16) {		
				for (i = row; (i < row + 16) && (i < N); i++) {
					for (j = column; (j < column + 16) && (j < M); j++) {
						if (i != j) {
							B[j][i] = A[i][j];
						} else {
							temp = A[i][j];
							diagonal = i;
						}
					}
					if (row == column) {
						B[diagonal][diagonal] = temp;
					}
				}
	 		}
		}
	}
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
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

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
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

