#define N 16
int A[N];
int B[N];
int C[N];

int test1(int a, int b, int c, int d) {
  // should be parallelizable into a vector add
  // <e, f, g, h> = <a, b, c, d> + <a, b, c, d>
  int e = a * a;
  int f = b * b;
  int g = c * c;
  int h = d * d;

  // now need to unpack
  return e + f + g + h;
}

int test2(int a, int b, int c, int d, long i) {
  // should be parallelizable into a vector add
  // <e, f, g, h> = <a, b, c, d> + bitcast(A[i:i+4])
  int e = a * A[i];
  int f = b * A[i + 1];
  int g = c * A[i + 2];
  int h = d * A[i + 3];

  // now need to unpack
  return e + f + g + h;
}

int test3(int a, int b, int c, int d, long i) {
  // should be parallelizable into a vector add
  // <e, f, g, h> = <a, b, c, d> + <A[i], A[i+1], A[i+2], A[i+3]>
  int e = a * A[i];
  int f = b * A[i + 1];
  int g = c * A[i + 2];
  int h = d * A[i + 3];

  // now need to unpack
  return e + f + g + h;
}

int test4(int a, int b, int c, int d, long i) {
  // should be parallelizable into a vector add
  // bitcast(A[i:i+4]) = <a, b, c, d> + <a, b, c, d>
  A[i] = a + a;
  A[i + 1] = b + b;
  A[i + 2] = c + c;
  A[i + 3] = d + d;

  return A[i] + A[i+1] + A[i+2] + A[i+3];
}

int test5(long i) {
  // should be parallelizable into a vector add
  // bitcast(C[i:i+4]) = bitcast(A[i+1:i+5]) + bitcast(B[i+2:i+6])
  C[i]      = A[i + 1] + B[i + 2];
  C[i + 1]  = A[i + 2] + B[i + 3];
  C[i + 2]  = A[i + 3] + B[i + 4];
  C[i + 3]  = A[i + 4] + B[i + 5];

  return 0;
}

// IMPORTANT: this test currently does not work
// int test5(long i) {
//   // should be parallelizable into a vector add
//   // bitcast(A[i:i+4]) = bitcast(A[i+4:i+8]) + bitcast(A[i:i+4])
//   A[i]      = A[i + 4] + A[i];
//   A[i + 1]  = A[i + 5] + A[i + 1];
//   A[i + 2]  = A[i + 6] + A[i + 2];
//   A[i + 3]  = A[i + 7] + A[i + 3];

//   return 0;
// }

int test6(int a, int b, int c, int d, long i) {
  // should be parallelizable into a vector add
  // bitcast(C[i:i+4]) = bitcast(A[i+1:i+5]) + bitcast(B[i+2:i+6])
  C[i]      = A[i + 1] + B[i + 2] * a;
  C[i + 1]  = A[i + 2] + B[i + 3] * b;
  C[i + 2]  = A[i + 3] + B[i + 4] * c;
  C[i + 3]  = A[i + 4] + B[i + 5] * d;
  return 0;
}

int main() {
  for (int i=0; i < N; i++) A[i] = i;
  printf("test1: %d\n", test1(1, 2, 3, 4));
  printf("test2: %d\n", test2(1, 2, 3, 4, 0));
  printf("test3: %d\n", test3(1, 2, 3, 4, 0));
  printf("test4: %d\n", test4(1, 2, 3, 4, 0));
  printf("test5: %d\n", test5(0));
  printf("test6: %d\n", test6(1, 2, 3, 4, 0));
  return 0;
}
