all: vectorizer.so

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) -g -O0 -fPIC

%.so: %.o
	$(CXX) -dylib -shared $^ -o $@
clean:
	rm -f *.o *~ *.so


# LLVM_CONFIG = $(shell llvm-config --cxxflags)
# CXXFLAGS = -rdynamic $(LLVM_CONFIG:c++14=c++17) -g -O0 -fPIC

# vectorizer: vectorizer.cpp
# 	$(CXX) -g vectorizer.cpp -o vectorizer

# clean:
# 	rm -rf vectorizer *.ll *.bc