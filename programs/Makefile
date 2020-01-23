export LIB_INCLUDE_DIR=../../libraries/
export LIB_LIBRARY_DIR=../../libraries/

all :
	$(MAKE) -C net
	$(MAKE) -C shell

clean :
	$(MAKE) -C net clean
	$(MAKE) -C shell clean

unittest :
	$(MAKE) -C net unittest
	$(MAKE) -C shell unittest

run-unittest :
	$(MAKE) -C net run-unittest
	$(MAKE) -C shell run-unittest

check :
	$(MAKE) -C net check
	$(MAKE) -C shell check

.PHONY : all clean unittest check run-unittest

