# Sorry for recursive make!

GRUB_CFG ?= grub.cfg

all : kernel/vmpeoz init.tar

image : kernel/vmpeoz init.tar
	@mkdir -p .image/boot/grub
	cp $(GRUB_CFG) .image/boot/grub
	cp kernel/vmpeoz .image/boot/
	cp init.tar .image/boot/
	grub-mkrescue -o peos2.img .image

init.tar : programs/first_program shell tester
	@mkdir -p .initar/bin
	cp programs/first_program .initar/bin/
	cp shell/shell .initar/bin/
	cp tester/tester .initar/bin/
	cd .initar && tar cf ../init.tar *

libsupport :
	$(MAKE) -C support -f Makefile.target

kernel/vmpeoz : libsupport
	$(MAKE) -C kernel

programs/first_program :
	$(MAKE) -C programs

shell : libsupport
	$(MAKE) -C shell

tester : libsupport
	$(MAKE) -C tester

.PHONY : clean kernel/vmpeoz programs/first_program check
clean :
	$(MAKE) -C kernel clean
	$(MAKE) -C programs clean
	$(MAKE) -C shell clean
	$(MAKE) -C support -f Makefile.target clean
	rm -rf .initar .image init.tar

check :
	@test/runner.rb
