
#define MSIZE 1024

unsigned int A[MSIZE];
unsigned int B[MSIZE];

static void init() {
  for (int i=0; i < MSIZE; i++) {
  	A[i] = i;
    B[i] = MSIZE - i;
  }
}

static int dotprod() {
	unsigned int tmp[MSIZE];

	#pragma clang loop unroll_count(4)
	for (int i=0; i < MSIZE; i++) {
		tmp[i] = A[i] * B[i];
	}

	int out = 0;
	for (int i=0; i < MSIZE; i++) {
    out += tmp[i];
  }
  return out;
}

int main() {
  init();
  return dotprod();
}
