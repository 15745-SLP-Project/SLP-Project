#define N 16
int A[N];

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

void test4(int a, int b, int c, int d, long i) {
  // should be parallelizable into a vector add
  // bitcast(A[i:i+4]) = <a, b, c, d> + <a, b, c, d>
  A[i] = a + a;
  A[i + 1] = b + b;
  A[i + 2] = c + c;
  A[i + 3] = d + d;

  return;
}

void test5(int a, int b, int c, int d, long i) {
  // should be parallelizable into a vector add
  // bitcast(A[i:i+4]) = <a, b, c, d> + bitcast(A[i:i+4])
  A[i] = a * A[i];
  A[i + 1] = b * A[i + 1];
  A[i + 2] = c * A[i + 2];
  A[i + 3] = d * A[i + 3];

  return;
}

int main() {
  test1(1, 2, 3, 4);
  test2(1, 2, 3, 4, 0);
  test3(1, 2, 3, 4, 0);
  test4(1, 2, 3, 4, 0);
  test5(1, 2, 3, 4, 0);

  return 0;
}
