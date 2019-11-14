# Sorry for recursive make!

all : kernel/vmpeoz init.tar

init.tar : programs/first_program shell
	@mkdir -p .initar/bin
	cp programs/first_program .initar/bin/
	cp shell/shell .initar/bin/
	cd .initar && tar cf ../init.tar *

libsupport :
	$(MAKE) -C support -f Makefile.target

kernel/vmpeoz : libsupport
	$(MAKE) -C kernel

programs/first_program :
	$(MAKE) -C programs

shell : libsupport
	$(MAKE) -C shell

.PHONY : clean kernel/vmpeoz programs/first_program
clean :
	$(MAKE) -C kernel clean
	$(MAKE) -C programs clean
	$(MAKE) -C shell clean
	$(MAKE) -C support -f Makefile.target clean
	rm -rf .initar init.tar
