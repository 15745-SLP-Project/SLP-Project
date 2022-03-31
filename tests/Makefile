TEST ?= foo

all: $(TEST).1.out $(TEST).2.out $(TEST).test.out

%.1.ll: %.c
	clang -O1 -mavx2 -emit-llvm -S -o $@ $^

%.2.ll: %.1.ll
# clang -O2 -emit-llvm -S -o $@ $^
	opt -slp-vectorizer -S -o $@ $^

%.test.ll: %.1.ll
	opt -load ../SLP/slp.so -slp -S -o $@ $^

%.S: %.ll
	llc -o $@ $^

%.out: %.S
	clang -o $@ $^

clean:
	-@rm -f *.ll
	-@rm -f *.S
	-@rm -f *.dump
	-@rm -f *.out

.PRECIOUS: %.1.ll %.2.ll %.test.ll %.S

.PHONY: all clean
