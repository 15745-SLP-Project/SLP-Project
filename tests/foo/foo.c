#include <stdio.h>

#define N (1 << 5)

static long A[N];

void foo(long i) {
  A[i + 0] = A[i + 0] * A[i + 0];
  A[i + 1] = A[i + 1] * A[i + 1];
  A[i + 2] = A[i + 2] * A[i + 2];
  A[i + 3] = A[i + 3] * A[i + 3];
}

void set() {
  for (long i = 0; i < N / 4; i += 4) {
    foo(i);
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

// int test(int a) {
//   int arr[4];
//   arr[0] = a + 0;
//   arr[1] = a + 1;
//   arr[2] = a + 2;
//   arr[3] = a + 3;
//   return arr[0] + arr[1] + arr[2] + arr[3];
// }

// int test1(int a) {
//   // int arr[4];
//   // arr[0] = a + 0;
//   // arr[1] = a + 1;
//   // arr[2] = a + 2;
//   // arr[3] = a + 3;
//   // return arr[0] + arr[1] + arr[2] + arr[3];
//   return 0;
// }

// int test2(int a, int b, int c, int d) {
//   return 0;
// }