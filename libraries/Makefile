export LIB_INCLUDE_DIR=..
export LIB_LIBRARY_DIR=..

PROJECTS=support net
TARGETS=all clean unittest run-unittest check

define generate_target
$(1):
	$(foreach subdir, $(PROJECTS), $(MAKE) -C $(subdir) $(1) ;)
endef

$(foreach target, $(TARGETS), $(eval $(call generate_target, $(target))) ;)

.PHONY : $(TARGETS)
