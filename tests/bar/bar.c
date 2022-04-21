#include <stdio.h>

#define N (1 << 28)

static long A[N];

void set() {
  #pragma clang loop unroll_count(4)
  for (long i = 0; i < N; i ++) {
    A[i] = A[i] * A[i];
  }
}

long sum() {
  long sum = 0;
  for (long i = 0; i < (N >> 26); i++) {
    sum += A[i];
  }
  return sum;
}

int main() {
  set();

  long s = sum();
  printf("%ld\n", s);

  return (int)s;
}
