# Sorry for recursive make!

GRUB_CFG?=grub.cfg

ifneq ($(HOSTED),1)
all : kernel init.tar
endif

image : kernel init.tar
	@mkdir -p .image/boot/grub
	cp $(GRUB_CFG) .image/boot/grub/grub.cfg
	cp kernel/vmpeoz .image/boot/
	cp init.tar .image/boot/
	grub-mkrescue -o peos2.img .image

vboximg : image
	VBoxManage convertfromraw --format VDI peos2.img peos2.vdi

init.tar : programs kernel
	@mkdir -p .initar/bin
	cp programs/shell .initar/bin/
	cp programs/shell_launcher .initar/bin/
	cp programs/tester .initar/bin/
	cp programs/nsa .initar/bin/
	cp programs/ls .initar/bin/
	cd .initar && tar cf ../init.tar *

libraries :
	$(MAKE) -C libraries

kernel : libraries
	$(MAKE) -C kernel

programs : libraries kernel
	$(MAKE) -C programs

clean :
	$(MAKE) -C kernel clean
	$(MAKE) -C programs clean
	$(MAKE) -C libraries clean
	$(MAKE) -C programs clean
	rm -rf .initar .image init.tar

unittest : libraries
	$(MAKE) -C libraries unittest
	$(MAKE) -C programs -f Makefile.host unittest

run-unittest : libraries
	$(MAKE) -C libraries run-unittest
	$(MAKE) -C programs run-unittest

check :
	@test/runner.rb

dist-docker :
	docker build . -t peos-release

# TODO: rename peos-httpd peos-live
dist-docker-live :
	docker build live -t peos-httpd

publish-docker-live :
	docker tag peos-httpd eu.gcr.io/the-big-dump/peos-httpd:latest
	docker push eu.gcr.io/the-big-dump/peos-httpd:latest

run-docker-live :
	docker run --privileged -it -p 8080:8080 peos-httpd:latest

toolchain :
	docker build toolchain -t docker.pkg.github.com/peteb/peos2/peos-toolchain:latest

.PHONY : clean kernel/vmpeoz programs/first_program check unittest dist-docker dist-docker-live \
	publish-docker-live run-docker-live toolchain libraries kernel programs run-unittest
