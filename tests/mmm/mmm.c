/**
 * mmm.c
 *
 * integer matrix-mult benchmark
 *
 * In lab4, we will compile with march=rv32im to generate native mul
 *instructions
 *
 * NOTE: mmmRV32I.c and mmmRV32IM.c are identical.
 *       mmmRV32I.reg and mmmRV32IM.reg are different results depending on
 *       whether the -march=rv32i or -march=rv32im flag is used.
 *       With -march=rv32i, multiply is emulated using rv32i instructions only.
 *       With -march=rv32im, the m extension needs to be implemented.
 **/

/*----------------------------------------------------------------------------
 * Internal Definitions
 *----------------------------------------------------------------------------*/

#define MSIZE 1280

// The matrices used for matrix multiplication
unsigned int A[MSIZE][MSIZE];
unsigned int B[MSIZE][MSIZE];
unsigned int C[MSIZE][MSIZE];

/*----------------------------------------------------------------------------
 * Functions
 *----------------------------------------------------------------------------*/

static void init() {
  int i, j;
  for (i = 0; i < MSIZE; i++) {
    #pragma clang loop unroll_count(4)
    for (j = 0; j < MSIZE; j++) {
      A[i][j] = i * MSIZE + j;
      B[i][j] = ((i + 1) << 16) + (j + 1);
      C[i][j] = 0;
    }
  }
}

/* Performs matrix-matrix multiplication of A and B, storing the result in the
 * matrix C. */
static void mmm(unsigned int A[MSIZE][MSIZE], unsigned int B[MSIZE][MSIZE],
                unsigned int C[MSIZE][MSIZE]) {
  int A_rows = MSIZE, A_cols = MSIZE, B_cols = MSIZE;
  int output_row, output_col, input_dim;

  for (output_row = 0; output_row < A_rows; output_row++) {
    for (output_col = 0; output_col < B_cols; output_col++) {
      #pragma clang loop unroll_count(4)
      for (input_dim = 0; input_dim < A_cols; input_dim++) {
        C[output_row][output_col] +=
            A[output_row][input_dim] * B[input_dim][output_col];
      }
    }
  }
}

// Sums all the elements in the given matrix together
unsigned int matrix_add_reduce(int rows, int cols, unsigned int M[rows][cols]) {
  unsigned int sum = 0;
  int row, col;

  for (row = 0; row < rows; row++) {
    #pragma clang loop unroll_count(4)
    for (col = 0; col < cols; col++) {
      sum += M[row][col];
    }
  }

  return sum;
}

// Main method for the program
int main() {

  init();

  mmm(A, B, C);

  /* Sum the output matrix and return the binary representation of the
   * floating-point sum (it is not converted to an integer). */
  unsigned int sum = matrix_add_reduce(MSIZE, MSIZE, C);
  return *(int *)&sum;
}
