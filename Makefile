######################################################################
# SOLARIS DAQ - Top-level Makefile
# Builds: SOLARIS_DAQ (GUI), solaris-broker, solaris-cli
# All executables are placed in this directory.
######################################################################

all: gui broker

#--- GUI (Qt6, uses qmake) ---
gui:
	@echo "=== Building GUI ==="
	cd GUI && qmake6 SOLARIS_DAQ.pro -o Makefile && $(MAKE)

#--- Broker & CLI ---
broker:
	@echo "=== Building Broker ==="
	$(MAKE) -C broker

clean:
	@echo "=== Cleaning GUI ==="
	-cd GUI && $(MAKE) clean 2>/dev/null
	rm -f GUI/Makefile GUI/.qmake.stash
	@echo "=== Cleaning Broker ==="
	$(MAKE) -C broker clean
	@echo "=== Cleaning root ==="
	rm -f SOLARIS_DAQ solaris-broker solaris-cli

.PHONY: all gui broker clean
