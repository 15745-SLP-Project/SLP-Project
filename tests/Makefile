TEST ?= foo
TARGET = $(TEST).0.out $(TEST).1.out $(TEST).2.out $(TEST).slp.out $(TEST).vec.out
CFG = 0
CFLAGS = -target aarch64-linux-gnu

all: $(TARGET)

%.0.ll: %.c
	clang -O0 $(CFLAGS) -emit-llvm -S -o $@ $^
	@if [ "$(CFG)" -eq 1 ]; then \
		opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$@ $@; \
	fi

%.1.ll: %.c
	clang -O1 $(CFLAGS) -emit-llvm -S -o $@ $^
	@if [ "$(CFG)" -eq 1 ]; then \
		opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$@ $@; \
	fi
	opt --instnamer -S -o $@ $@

%.2.ll: %.c
	clang -O2 $(CFLAGS) -emit-llvm -S -o $@ $^
	@if [ "$(CFG)" -eq 1 ]; then \
		opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$@ $@; \
	fi
# 	opt -slp-vectorizer -S -o $@ $^

%.slp.ll: %.1.ll
	opt -load ../SLP/slp.so -slp -S -o $@ $^

%.vec.ll: %.1.ll
	opt -load ../vectorizer/vectorizer.so -vectorizer -S -o $@ $^

%.S: %.ll
	llc -march=aarch64 --aarch64-neon-syntax=generic -O0 $^ -o $@

%.out: %.S
	clang $^ -o $@
#	aarch64-linux-gnu-gcc $^ -o $@

%.qemu: %.out
	qemu-aarch64 -L /usr/aarch64-linux-gnu ./$^

clean:
	-@rm -f *.ll
	-@rm -f *.S
	-@rm -f *.dump
	-@rm -f *.out
	-@rm -f *.bc
	-@rm -f .*.dot

.PRECIOUS: %.0.ll %.1.ll %.2.ll %.vec.ll %.slp.ll %.S

.PHONY: all clean
