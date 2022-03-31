TEST ?= foo

all: $(TEST).0.S $(TEST).1.S $(TEST).2.S $(TEST).vec.S
	clang $(TEST).0.S -o $(TEST).0
	clang $(TEST).1.S -o $(TEST).1
	clang $(TEST).2.S -o $(TEST).2
	clang $(TEST).vec.S -o $(TEST).vec

%.0.ll: %.c
	clang -O0 -emit-llvm -S -o $@ $^
	opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$(TEST).0 $@

%.1.ll: %.c
	clang -O1 -emit-llvm -S -o $@ $^
	opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$(TEST).1 $@

%.2.ll: %.c
	clang -O2 -emit-llvm -S -o $@ $^
	opt --dot-cfg -disable-output -cfg-dot-filename-prefix .$(TEST).2 $@
# 	opt -slp-vectorizer -S -o $@ $^

%.vec.ll: %.c
	clang -O1 -emit-llvm -S -o $@ $^
	opt -load ../vectorizer/vectorizer.so -vectorizer -o $(TEST).vec.bc $@ 
	llvm-dis $(TEST).vec.bc -o $@

%.S: %.ll
	llc -O0 $^ -o $@

clean:
	-@rm -f *.ll
	-@rm -f *.S
	-@rm -f *.dump
	-@rm -f *.0
	-@rm -f *.1
	-@rm -f *.2
	-@rm -f *.vec
	-@rm -f *.bc
	-@rm -f .*.dot

.PRECIOUS: %.0.ll %.1.ll %.2.ll %.vec.ll

.PHONY: all clean
