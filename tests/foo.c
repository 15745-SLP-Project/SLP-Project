#define N (1 << 28)

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

  return (int)s;
}
