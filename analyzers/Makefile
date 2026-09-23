# Build each analysis into its own shared object, loadable by the Analyzer window at runtime.
#
# An analysis needs nothing from SOLARIS_DAQ: Analysis and HistRegistry are pure abstract, so every
# call crosses through a vtable and no host symbol is referenced. That is why there is no -rdynamic,
# nothing linked from the DAQ, and no Qt, CAEN or ROOT here. Check it stays that way with:
#
#     nm -D -u build/Coincidence.so        # only libc / libstdc++ should appear
#
# ONE ANALYSIS PER FILE. The plugin entry points are extern "C" with fixed names, so a second
# ANALYSIS_ENTRY in the same .so would be a duplicate-symbol error.
#
#     make                 build every analysis
#     make build/Foo.so    build one
#     make clean

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fPIC -shared -I.. -DANALYSIS_PLUGIN_BUILD

SRC = $(wildcard *.cpp)
SO  = $(patsubst %.cpp,build/%.so,$(SRC))

# The plugin ABI surface, deliberately tiny. Touching either rebuilds every analysis, which is
# correct: both are part of what the host and the .so agree on.
ABI = ../Analysis.h ../BuiltHit.h

all: $(SO)

build/%.so: %.cpp $(ABI) | build
	$(CXX) $(CXXFLAGS) $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
