/* transpose function of 32 x 32*/
void transpose_32x32(int M, int N, int A[N][M], int B[M][N]) {
    /* Your Code Here [1/3] */
    int i, j, x, y;
    int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    
    // 8x8分块转置，使用临时变量避免对角线冲突
    for (i = 0; i < N; i += 8) {
        for (j = 0; j < M; j += 8) {
            // 对于对角线块，使用临时变量避免cache冲突
            if (i == j) {
                for (x = 0; x < 8; x++) {
                    temp0 = A[i + x][j + 0];
                    temp1 = A[i + x][j + 1];
                    temp2 = A[i + x][j + 2];
                    temp3 = A[i + x][j + 3];
                    temp4 = A[i + x][j + 4];
                    temp5 = A[i + x][j + 5];
                    temp6 = A[i + x][j + 6];
                    temp7 = A[i + x][j + 7];
                    
                    B[j + 0][i + x] = temp0;
                    B[j + 1][i + x] = temp1;
                    B[j + 2][i + x] = temp2;
                    B[j + 3][i + x] = temp3;
                    B[j + 4][i + x] = temp4;
                    B[j + 5][i + x] = temp5;
                    B[j + 6][i + x] = temp6;
                    B[j + 7][i + x] = temp7;
                }
            } else {
                // 非对角线块，直接转置
                for (x = 0; x < 8; x++) {
                    for (y = 0; y < 8; y++) {
                        B[j + y][i + x] = A[i + x][j + y];
                    }
                }
            }
        }
    }
}

/* transpose function of 64 x 64*/
void transpose_64x64(int M, int N, int A[N][M], int B[M][N]) {
    /* Your Code Here [2/3] */
    int i, j, x, y;
    int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    
    // 64x64矩阵需要更精细的分块策略
    // 使用4x4分块，因为8x8会导致过多cache冲突
    for (i = 0; i < N; i += 4) {
        for (j = 0; j < M; j += 4) {
            if (i == j) {
                // 对角线块使用临时变量
                for (x = 0; x < 4; x++) {
                    temp0 = A[i + x][j + 0];
                    temp1 = A[i + x][j + 1];
                    temp2 = A[i + x][j + 2];
                    temp3 = A[i + x][j + 3];
                    
                    B[j + 0][i + x] = temp0;
                    B[j + 1][i + x] = temp1;
                    B[j + 2][i + x] = temp2;
                    B[j + 3][i + x] = temp3;
                }
            } else {
                // 非对角线块直接转置
                for (x = 0; x < 4; x++) {
                    for (y = 0; y < 4; y++) {
                        B[j + y][i + x] = A[i + x][j + y];
                    }
                }
            }
        }
    }
}

/* transpose function of 61 x 67*/
void transpose_61x67(int M, int N, int A[N][M], int B[M][N]) {
    /* Your Code Here [3/3] */
    int i, j, x, y;
    int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    
    // 不规则矩阵使用较小的分块
    int block_size = 8;
    
    for (i = 0; i < N; i += block_size) {
        for (j = 0; j < M; j += block_size) {
            // 处理边界情况
            int max_x = (i + block_size < N) ? block_size : (N - i);
            int max_y = (j + block_size < M) ? block_size : (M - j);
            
            for (x = 0; x < max_x; x++) {
                for (y = 0; y < max_y; y++) {
                    B[j + y][i + x] = A[i + x][j + y];
                }
            }
        }
    }
}
