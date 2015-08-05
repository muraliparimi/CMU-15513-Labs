/* 
 * trans.c - Matrix transpose B = A^T
 * Si Lao sil@andrew.cmu.edu
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, i1, j1;
    int sp1, sp2, sp3,sp4,sp5,sp6,sp7,sp8;
    REQUIRES(M > 0);
    REQUIRES(N > 0);
    if (M == 64 && N == 64) {
        for (j = 0; j < N; j += 8) { //Column-wise
            for (i = 0; i < M; i += 8) {
                sp1 = A[i][j + 4];
                sp2 = A[i][j + 5];
                sp3 = A[i][j + 6];
                sp4 = A[i][j + 7];
                sp5 = A[i + 1][j + 4];
                sp6 = A[i + 1][j + 5];
                sp7 = A[i + 1][j + 6];
                sp8 = A[i + 1][j + 7];
                for (i1 = i; i1 < i + 8 && i1 < M; i1 ++) {
                	for (j1 = j; j1 < j + 4 && j1 < N; j1 ++) {
                            B[j1][i1] = A[i1][j1];
                    }
                }
                for (i1 = i + 7; i1 >  i + 1; i1 --) {
                	for (j1 = j + 4; j1 < j + 8; j1 ++) {
                            B[j1][i1] = A[i1][j1];
                    }
                }
                B[j + 4][i] = sp1;
                B[j + 5][i] = sp2;
                B[j + 6][i] = sp3;
                B[j + 7][i] = sp4;
                B[j + 4][i + 1] = sp5;
                B[j + 5][i + 1] = sp6;
                B[j + 6][i + 1] = sp7;
                B[j + 7][i + 1] = sp8;
            }
        }
    } else {
        if (M == 32 && N == 32) {
            for (i = 0; i < N; i += 8) {
                for (j = 0; j < M; j += 8) {
                    for (i1 = i; i1 < i + 8 && i1 < N; i1 ++) {
                        for (j1 = j; j1 < j + 8 && j1 < M; j1 ++) {
                            if (i1 != j1)
                                B[j1][i1] = A[i1][j1];
                        }
                        if (i == j) { //for diagonal elements
                            B[i1][i1] = A[i1][i1];
                        }
                    }
                }
            }
        }
        if (M == 61 && N == 67) {
            for (i = 0; i < N; i += 17) {
                for (j = 0; j < M; j += 17) {
                    for (i1 = i; i1 < i + 17 && i1 < N; i1 ++) {
                        for (j1 = j; j1 < j + 17 && j1 < M; j1 ++) {
                            if (i1 != j1)
                                B[j1][i1] = A[i1][j1];
                        }
                        if (i == j) { //for diagonal elements
                            B[i1][i1] = A[i1][i1];
                        }
                    }
                }
            }
        }
    }

    ENSURES(is_transpose(M, N, A, B));
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

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
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

