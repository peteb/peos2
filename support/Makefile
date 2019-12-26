# -*- makefile -*-

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

OBJECTS=utils.o string.o
OBJECT_PATHS=$(addprefix $(OBJDIR)/,$(OBJECTS))

all : $(OBJDIR)/libsupport.a

clean :
	rm -rf $(OBJDIR)

$(OBJDIR)/libsupport.a : $(OBJECT_PATHS)
	ar rcs $(OBJDIR)/libsupport.a $(OBJECT_PATHS)
