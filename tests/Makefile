CC = aarch64-linux-gnu-gcc
OBJDUMP = aarch64-linux-gnu-objdump

TARGET = $(TEST).0.out $(TEST).1.out $(TEST).2.out $(TEST).unroll.out $(TEST).slp.out
CFLAGS = -target aarch64-linux-gnu -I /usr/aarch64-linux-gnu/include -I $(TEST)/src

INCDIRS = $(addprefix -I, $(sort $(dir $(shell find $(TEST) -name '*.h'))))
CFLAGS += $(INCDIRS)

OUTPUT_DIR = output/$(TEST)
TEST_NAME = $(OUTPUT_DIR)/$(TEST)

MANUAL_UNROLL = 0

$(OUTPUT_DIR):
	mkdir -p $@

TEST ?= axpy
SRCS := $(shell find $(TEST) -name '*.c')

0_Ss = $(patsubst %.c,%.0.S,$(SRCS))
0_os = $(patsubst %.c,%.0.o,$(SRCS))

1_Ss = $(patsubst %.c,%.1.S,$(SRCS))
1_os = $(patsubst %.c,%.1.o,$(SRCS))

2_Ss = $(patsubst %.c,%.2.S,$(SRCS))
2_os = $(patsubst %.c,%.2.o,$(SRCS))

unroll_Ss = $(patsubst %.c,%.unroll.S,$(SRCS))
unroll_os = $(patsubst %.c,%.unroll.o,$(SRCS))

slp_Ss = $(patsubst %.c,%.slp.S,$(SRCS))
slp_os = $(patsubst %.c,%.slp.o,$(SRCS))

all: $(OUTPUT_DIR) $(SRCS) $(TEST_NAME).0.out $(TEST_NAME).1.out $(TEST_NAME).2.out $(TEST_NAME).unroll.out $(TEST_NAME).slp.out

%.0.ll: %.c
	clang -O0 $(CFLAGS) -emit-llvm -S -o $@ $^

%.1.ll: %.c
	clang -O1 -ffast-math $(CFLAGS) -emit-llvm -S -o $@ $^

%.2.ll: %.c
	clang -O2 -ffast-math $(CFLAGS) -emit-llvm -S -o $@ $^

# Loop unroll before SLP, ONLY ENABLED for simple examples without nested loops
ifeq ($(MANUAL_UNROLL), 1)
%.unroll.ll: %.1.ll
	cp $^ $@
else
%.unroll.ll: %.1.ll
	opt -loop-unroll -unroll-count=4 -S -o $@ $^
endif

%.slp.ll: %.unroll.ll
	opt -instnamer -S -o $^ $^
	opt -load ../SLP/slp.so -slp -S -o $@ $^
	opt -dce -S -o $@ $@

%.S: %.ll
	llc -filetype=asm -march=aarch64 --aarch64-neon-syntax=generic -O0 $^ -o $@

%.o: %.ll
	llc -filetype=obj -march=aarch64 --aarch64-neon-syntax=generic -O0 $^ -o $@

$(TEST_NAME).0.out: $(0_os) | $(0_Ss)
	$(CC) -o $@ $^ -lm
	$(OBJDUMP) -d $@ > $(TEST_NAME).0.dump

$(TEST_NAME).1.out: $(1_os) | $(1_Ss)
	$(CC) -o $@ $^ -lm
	$(OBJDUMP) -d $@ > $(TEST_NAME).1.dump

$(TEST_NAME).2.out: $(2_os) | $(2_Ss)
	$(CC) -o $@ $^ -lm
	$(OBJDUMP) -d $@ > $(TEST_NAME).2.dump

$(TEST_NAME).unroll.out: $(unroll_os) | $(unroll_Ss)
	$(CC) -o $@ $^ -lm
	$(OBJDUMP) -d $@ > $(TEST_NAME).unroll.dump

$(TEST_NAME).slp.out: $(slp_os) | $(slp_Ss)
	$(CC) -o $@ $^ -lm
	$(OBJDUMP) -d $@ > $(TEST_NAME).slp.dump

%.qemu: %.out
	qemu-aarch64 -L /usr/aarch64-linux-gnu ./$^

.PRECIOUS: %.0.ll %.1.ll %.2.ll %.unroll.ll %.slp.ll

clean:
	@rm -rf output
	@find . -name '*.ll' -exec rm -r {} \;
	@find . -name '*.S' -exec rm -r {} \;
	@find . -name '*.o' -exec rm -r {} \;

.PHONY: all clean

