# Peos2
Peos2 is an operating system for i386 (so far). I'm not going to pretend it's free from Unix influences.

![](https://github.com/peteb/peos2/workflows/make%20check/badge.svg)

## Background
Why build yet another operating system?

- Because it's fun!
- Learning
- A good place to study algorithms and system design
- It's meditative
- To celebrate ARPANET 50 years

There are probably more reasons, but I'm not looking to make anything "useful". Also, I'm not looking to build a fancy GUI or anything like that, I'd rather get a network stack up and running.

## Features
- Basic x86 stuff: GDT, IDT, PIC, etc.
- Preemptive multitasking
- User space programs (ring 3) with their own address spaces
- Fork and exec
- Terminals
- Virtual filesystem where you can mount drivers
- Initial ramdisk loaded from a TAR file
- Network driver (RTL8139)

## Building the toolchain
Everything needed to build and run is encapsulated by a Docker image that can be built like so:

```bash
build/compiler/build-compiler-image
```

## Running integration tests
Useful to verify that the environment is OK. Takes a while to run as
it contains load tests and multiple configurations.

The integration test suite builds the kernel with various compiler
flags, boots it and runs tests. It'll also build and run all
unittests.

```bash
./build-shell make check
```

## Building and running bootable binaries
This will create a bootable kernel binary and an init.tar file that
contains the initial ramdisk -- this is enough to run things using
QEMU due to its Multiboot support. This will also build a
CD-ROM disk image with GRUB that can be booted using Bochs and other
emulators (and possibly even real machines!)

```bash
DEFS=-DNODEBUG OPT_FLAGS="-O0 -g" build-shell setenv target make all image
```

To run the kernel in QEMU with "user networking", ie, the guest will
live on its own network behind NAT and can be reached using port
forwarding from the host:

```bash
./run-qemu httpd
```

You can verify that everything works by connecting on port 8080 (`nc
localhost 8080`).

For debugging and controlling QEMU, you can attach to the monitor
using `./connect-qemu-monitor`, and to debug using gdb, `target remote
localhost:1234` can be used.

To display network traffic, either `./run-tshark` or `./run-wireshark`
can be used.

## Building and running unittests
```bash
build-shell setenv host make -C support unittest check
```

## Building and running the live environment image
For better performance and full exposure to the Internet, QEMU's TAP
networking support is used in the live environment. This is
implemented by taking the bridged NIC's MAC address and using it for
the virtual NIC within QEMU. All packets heading for that MAC address
will be forwarded over the NIC-TAP bridge and handled by the guest.

This can be tested safely locally using Docker port forward networking:

```bash
make dist-docker dist-docker-httpd
make run-docker-httpd
curl http://localhost:8080  # Verification
```

Running this in Docker "host" networking mode will tear down your
`$BRIDGE_IF` interface (defaults to eth0), and it might or might not
restore it properly after shutting down.

## Running and debugging using Bochs
The Bochs that can be installed with `brew` (macOS) doesn't support
remote GDB debugging, so if you want that you need to download its
source code and build it yourself:

```bash
./configure --enable-gdb-stub --with-sdl2 --disable-docbook --enable-a20-pin --enable-alignment-check --enable-all-optimizations --enable-cdrom --enable-clgd54xx --enable-cpu-level=6 --enable-disasm --enable-fpu --enable-iodebug --enable-large-ramfile --enable-logging --enable-long-phy-address --enable-pci --enable-plugins --enable-readline --enable-show-ips --enable-usb
```

Note that you won't get SMP support.

## Build system
There's always an "environment" active when building the code which
can either be "host" (ie, binaries that can be run by the host system
running the compiler), or "target", which needs an emulator or
hardware to run.

Makefile rules shouldn't change meaning depending on the current
environment -- for example, `make all` in the kernel should always try
to build the kernel itself rather than unittests. If `make all` is
executed within a non-compatible environment (like "host"), it can and
probably will fail.


## Notes
- Kernel isn't initialized when constructors are run, so only use them for trivial things
- The kernel starts with paging enabled and two identity mappings, one at 0x0, the other at 0xC0000000, with EIP being in the latter. The lower part is removed during kernel init
- Everything is a "work in progress"

## Style
- Internal structs/classes for a subsystem NO prefix (ie, GOOD: "process", "context".)
- External handle types YES prefix (ie, GOOD: "vfs_context", "mem_space")
- Handle variables, GOOD: "context_handle"
- Member variables, GOOD: "_member"
- To fix name collision, GOOD: "context &context_"
