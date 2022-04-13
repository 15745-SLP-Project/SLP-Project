all: slp.so

LLVM_CONFIG = $(shell llvm-config --cxxflags)
CXXFLAGS = -rdynamic $(LLVM_CONFIG:c++14=c++17) -g -O0 -fPIC

slp.o: slp.cpp slp.hpp

utils.o: utils.cpp utils.hpp

slp.so: slp.o utils.o
	$(CXX) -dylib -shared $^ -o $@

clean:
	rm -f *.o *~ *.so

.PHONY: clean all
