# Sorry for recursive make!

all : kernel/vmpeoz init.tar


init.tar : programs/first_program
	@mkdir -p .initar/bin
	cp programs/first_program .initar/bin/
	cd .initar && tar cf ../init.tar *

kernel/vmpeoz :
	$(MAKE) -C kernel

programs/first_program :
	$(MAKE) -C programs

.PHONY : clean kernel/vmpeoz programs/first_program
clean :
	$(MAKE) -C kernel clean
	$(MAKE) -C programs clean
	rm -r .initar init.tar
