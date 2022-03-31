TEST ?= foo
TARGET = $(TEST).0.out $(TEST).1.out $(TEST).2.out $(TEST).vec.out

all: $(TARGET)

%.0.ll: %.c
	clang -O0 -emit-llvm -S -o $@ $^
	opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$@ $@

%.1.ll: %.c
	clang -O1 -emit-llvm -S -o $@ $^
	opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$@ $@

%.2.ll: %.c
	clang -O2 -emit-llvm -S -o $@ $^
	opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$@ $@
# 	opt -slp-vectorizer -S -o $@ $^

%.vec.ll: %.c
	clang -O1 -emit-llvm -S -o $@ $^
	opt -load ../vectorizer/vectorizer.so -vectorizer -S -o $@ $@

%.S: %.ll
	llc -O0 $^ -o $@

%.out: %.S
	clang $^ -o $@

clean:
	-@rm -f *.ll
	-@rm -f *.S
	-@rm -f *.dump
	-@rm -f *.out
	-@rm -f *.bc
	-@rm -f .*.dot

.PRECIOUS: %.0.ll %.1.ll %.2.ll %.vec.ll %.S

.PHONY: all clean
