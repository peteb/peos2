# -*- makefile -*-

OBJECTS=utils.o string.o

.PHONY : clean

$(OBJDIR)/%.o : %.s
	@mkdir -p $(@D)
	$(AS) $< -o $@

$(OBJDIR)/%.o : %.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) -MMD -MP

$(OBJDIR)/%.o : %.cc
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(CXXFLAGS) -MMD -MP
