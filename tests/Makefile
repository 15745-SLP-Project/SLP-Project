TEST ?= foo

all: $(TEST).1.S $(TEST).2.S
	clang $(TEST).1.S -o $(TEST).1
	clang $(TEST).2.S -o $(TEST).2

%.1.ll: %.c
	clang -O1 -mavx2 -emit-llvm -S -o $@ $^

%.2.ll: %.1.ll
# clang -O2 -emit-llvm -S -o $@ $^
	opt -slp-vectorizer -S -o $@ $^

%.S: %.ll
	llc $^ -o $@

clean:
	-@rm -f *.ll
	-@rm -f *.S
	-@rm -f *.dump
	-@rm -f *.1
	-@rm -f *.2

.PRECIOUS: %.1.ll %.2.ll

.PHONY: all clean
