# Sorry for recursive make!

GRUB_CFG ?= grub.cfg

all : kernel/vmpeoz init.tar

image : kernel/vmpeoz init.tar
	@mkdir -p .image/boot/grub
	cp $(GRUB_CFG) .image/boot/grub/grub.cfg
	cp kernel/vmpeoz .image/boot/
	cp init.tar .image/boot/
	grub-mkrescue -o peos2.img .image

vboximg : image
	VBoxManage convertfromraw --format VDI peos2.img peos2.vdi

init.tar : programs kernel/vmpeoz
	@mkdir -p .initar/bin
	cp programs/shell .initar/bin/
	cp programs/shell_launcher .initar/bin/
	cp programs/tester .initar/bin/
	cp programs/nsa .initar/bin/
	cp programs/ls .initar/bin/
	cd .initar && tar cf ../init.tar *

libsupport :
	$(MAKE) -C support

kernel/vmpeoz : libsupport
	$(MAKE) -C kernel

programs : libsupport kernel/vmpeoz
	$(MAKE) -C programs

.PHONY : clean kernel/vmpeoz programs/first_program check unittest

clean :
	$(MAKE) -C kernel clean
	$(MAKE) -C programs clean
	$(MAKE) -C programs -f Makefile.host clean
	$(MAKE) -C support clean
	rm -rf .initar .image init.tar

unittest : libsupport
	$(MAKE) -C support unittest
	$(MAKE) -C programs -f Makefile.host unittest

check :
	@test/runner.rb
