# Build each analysis into its own shared object, loadable by the Analyzer window at runtime.
#
# An analysis needs nothing from SOLARIS_DAQ: Analysis and HistRegistry are pure abstract, so every
# call crosses through a vtable and no host symbol is referenced. That is why there is no -rdynamic
# and nothing linked from the DAQ. Qt IS allowed since ABI v2 (analyses lay out their own windows):
# libQt6* is a shared library both the DAQ and the .so link against, not a host symbol. CAEN and
# ROOT remain banned. Check it stays that way with:
#
#     nm -D -u build/Coincidence.so        # only libc / libstdc++ / libQt6* / libcurl should appear
#
# ClassInfluxDB.cpp is compiled INTO each .so (the exception, not a violation): an analysis that
# writes to the database (ABI v5: HistRegistry::InfluxOut/InfluxConfig hand it url/token/database)
# carries its own client rather than the host's InfluxDB *, which lives in the executable and is
# also owned by the GUI thread's writer. Same TU, RTLD_LOCAL, no -rdynamic: each copy binds to
# libcurl, exactly like Qt above.
#
# ONE ANALYSIS PER FILE. The plugin entry points are extern "C" with fixed names, so a second
# ANALYSIS_ENTRY in the same .so would be a duplicate-symbol error.
#
#     make                 build every analysis
#     make build/Foo.so    build one
#     make clean

CXX      = g++
QTFLAGS  = $(shell pkg-config --cflags Qt6Widgets)
QLIBS    = $(shell pkg-config --libs Qt6Widgets)
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fPIC -shared -I.. -DANALYSIS_PLUGIN_BUILD $(QTFLAGS)

SRC = $(wildcard *.cpp)
SO  = $(patsubst %.cpp,build/%.so,$(SRC))

# The plugin ABI surface, deliberately tiny. Touching either rebuilds every analysis, which is
# correct: both are part of what the host and the .so agree on.
ABI = ../Analysis.h ../BuiltHit.h

all: $(SO)

build/%.so: %.cpp ../ClassInfluxDB.cpp ../ClassInfluxDB.h RateScatter.h $(ABI) | build
	$(CXX) $(CXXFLAGS) $< ../ClassInfluxDB.cpp -o $@ $(QLIBS) -lcurl

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
