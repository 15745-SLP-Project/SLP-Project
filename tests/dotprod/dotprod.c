#include <stdio.h>
#include <time.h>

#define MSIZE (1 << 18)

float A[MSIZE];
float B[MSIZE];

static void init() {
  for (int i = 0; i < MSIZE; i++) {
    A[i] = i;
    B[i] = MSIZE - i;
  }
}

float dotprod() {
  float tmp[MSIZE];

  for (int i = 0; i < MSIZE; i++) {
    tmp[i] = A[i] * B[i];
  }

  float out = 0;
  for (int i = 0; i < MSIZE; i++) {
    out += tmp[i];
  }
  return out;
}

int main() {
  init();

  clock_t start, end;
  double t;

  start = clock();
  float result = dotprod();
  end = clock();
  t = ((double)(end - start)) / CLOCKS_PER_SEC * 1e6;

  printf("result = %f, time = %f us\n", result, t);

  return 0;
}
