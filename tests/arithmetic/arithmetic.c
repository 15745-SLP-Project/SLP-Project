#include <math.h>
#include <stdio.h>
#include <time.h>

#define N (1 << 18)

static float X[N], Y[N], Z[N];

void set() {
  for (long i = 0; i < N; i++) {
    X[i] = (float)i;
    Y[i] = (float)i;
    Z[i] = (float)i;
  }
}

void arithmetic(float a, float *X, float *Y, float *Z) {
  for (long i = 0; i < N; i += 2) {
    Z[i] = a * powf(X[i], a) + sqrtf(Y[i]) + sinf(Z[i]);
  }
}

float sum() {
  float sum = 0;
  for (long i = 0; i < N; i++) {
    sum += Z[i];
  }
  return sum;
}

int main() {
  set();

  clock_t start, end;
  start = clock();
  arithmetic(2.0, X, Y, Z);
  end = clock();
  double t = ((double)(end - start)) / CLOCKS_PER_SEC * 1e6;

  float s = sum();
  printf("sum = %f, time = %f us\n", s, t);

  return 0;
}
