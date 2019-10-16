// Mulitboot header values
.set ALIGN,    0x01             // align loaded modules on page boundaries
.set MEMINFO,  0x02             // provide memory map
.set FLAGS,    ALIGN | MEMINFO  // this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       // 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS)

// Header according to the multiboot spec
.section .mbhdr
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

// Stack for kernel
.section .bss
.align 16
stack_bottom:
.skip 0x40000
stack_top:

// Entry point
.section .text
.global _start
.type _start, @function
_start:
        mov $stack_top, %esp
        call _init  // Ctors etc
        call main
        call _fini  // Dtors etc

        cli
1:	hlt
        jmp 1b
