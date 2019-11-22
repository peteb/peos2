# Peos2
Peos2 is an operating system for i386 (so far). I'm not going to pretend it's free from Unix influences.

## Background
Why build yet another operating system?

- Because it's fun!
- Learning
- A good place to study algorithms and system design
- It's meditative
- To celebrate ARPANET 50 years

There are probably more reasons, but I'm not looking to make anything "useful". Also, I'm not looking to build a fancy GUI or anything like that, I'd rather get a nice network stack up and running.

## Building
This is very easy if you've got Docker – the Docker image contains everything you need to build and test:

```bash
pushd build/compiler
./build-compiler-image
popd
build/build-shell
make all image
```

You can also use `make check` to run a bunch of tests in different configurations to verify that everything's OK.

## Running
I'm running the system using Bochs (`bochs -q` in root) and QEMU (`run-qemu` in root). The Bochs that can be installed with `brew` (macOS) doesn't support remote GDB debugging, so if you want that you need to download its source and build it yourself:

```bash
./configure --enable-gdb-stub --with-sdl2 --disable-docbook --enable-a20-pin --enable-alignment-check --enable-all-optimizations --enable-cdrom --enable-clgd54xx --enable-cpu-level=6 --enable-disasm --enable-fpu --enable-iodebug --enable-large-ramfile --enable-logging --enable-long-phy-address --enable-pci --enable-plugins --enable-readline --enable-show-ips --enable-usb
```

Note that you won't get SMP support.

If you want to play around with networking, using `./run-qemu terminal`, `./run-wireshark` and `./connect-qemu-monitor` together with remote gdb debugging ("target remote localhost:1234", automatically enabled) is a pretty nice setup. Remember to build with debug flags for best experience (`OPT_FLAGS=-g make all`). TCP port 8884 on the host is forwarded to guest 123.

## Testing
I try to extract support code into the "support" library and add unittests ("support/unittests") which can run on the host. There's also support for integration tests using Tcl Expect scripts reading from the serial port/COM1, have a look in the "test" directory.


## Features
- Basic x86 stuff: GDT, IDT, PIC, etc.
- Preemptive multitasking
- User space programs (ring 3) with their own address spaces
- Fork and exec
- Terminals
- Virtual filesystem where you can mount drivers
- Initial ramdisk loaded from a TAR file

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
