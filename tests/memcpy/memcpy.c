#include <stdio.h>
#include <time.h>

#define N (1 << 20)

int A[N], B[N];

void memset2(int *dest, int x, unsigned long n) {
  // #pragma clang loop unroll_count(4)
  for (unsigned long i = 0; i < n; i++) {
    dest[i] = x;
  }
}

void memcpy2(int *dest, int *src, unsigned long n) {
  // #pragma clang loop unroll_count(4)
  for (unsigned long i = 0; i < n; i++) {
    dest[i] = src[i];
  }
}

int main() {

  clock_t start, end;
  memset2(A, 1, N);
  start = clock();
  memcpy2(B, A, N);
  end = clock();
  double t = ((double)(end - start)) / CLOCKS_PER_SEC * 1e6;

  printf("time = %f us\n", t);

  return 0;
}
