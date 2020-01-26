export LIB_INCLUDE_DIR=../../libraries/
export LIB_LIBRARY_DIR=../../libraries/

all :
	$(MAKE) -C net
	$(MAKE) -C shell
	$(MAKE) -C ls

clean :
	$(MAKE) -C net clean
	$(MAKE) -C shell clean
	$(MAKE) -C ls clean

unittest :
	$(MAKE) -C net unittest
	$(MAKE) -C shell unittest
	$(MAKE) -C ls unittest

run-unittest :
	$(MAKE) -C net run-unittest
	$(MAKE) -C shell run-unittest
	$(MAKE) -C ls run-unittest

check :
	$(MAKE) -C net check
	$(MAKE) -C shell check
	$(MAKE) -C ls check

.PHONY : all clean unittest check run-unittest

