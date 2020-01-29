export LIB_INCLUDE_DIR=../../libraries/
export LIB_LIBRARY_DIR=../../libraries/

PROJECTS=live-httpd shell ls
TARGETS=all clean unittest run-unittest check

define generate_target
$(1):
	$(foreach subdir, $(PROJECTS), $(MAKE) -C $(subdir) $(1) ;)
endef

$(foreach target, $(TARGETS), $(eval $(call generate_target, $(target))) ;)

.PHONY : $(TARGETS)

