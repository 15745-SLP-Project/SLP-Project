#include <stdio.h>

/*----------------------------------------------------------------------------
 * Internal Definitions
 *----------------------------------------------------------------------------*/

#define MSIZE 256

// The matrices used for matrix multiplication
double A[MSIZE][MSIZE];
double B[MSIZE][MSIZE];
double C[MSIZE][MSIZE];

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
void mmm(double A[MSIZE][MSIZE], double B[MSIZE][MSIZE],
         double C[MSIZE][MSIZE]) {
  int A_rows = MSIZE, A_cols = MSIZE, B_cols = MSIZE;
  int output_row, output_col, input_dim;
  double tmp[MSIZE];

  for (output_row = 0; output_row < A_rows; output_row++) {
    for (output_col = 0; output_col < B_cols; output_col++) {

      // No dependence between loops
#pragma clang loop unroll_count(4)
      for (input_dim = 0; input_dim < A_cols; input_dim++) {
        tmp[input_dim] = A[output_row][input_dim] * B[input_dim][output_col];
      }

      // Do reduction in a separate loop
      for (input_dim = 0; input_dim < A_cols; input_dim++) {
        C[output_row][output_col] += tmp[input_dim];
      }
    }
  }
}

// Sums all the elements in the given matrix together
double matrix_add_reduce(int rows, int cols, double M[rows][cols]) {
  double sum = 0;
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
  double sum = matrix_add_reduce(MSIZE, MSIZE, C);
  printf("%f\n", sum);
  return 0;
}
