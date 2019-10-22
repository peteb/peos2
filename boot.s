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
.global stack_top
stack_top:

// Entry point
.section .text
.global _start
.type _start, @function
_start:
        mov $stack_top, %esp
        push %ebx   // save multiboot header, argument to main
        push %eax   // multiboot magic

        call _init  // ctors
        call main
        call _fini  // dtors

1:	hlt
        jmp 1b


.global enter_ring3
.type enter_ring3, @function
enter_ring3:
        mov 4(%esp), %ax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        mov %esp, %eax
        mov 8(%esp), %ebx

        push %ds
        push %eax
        pushf
        push %ebx
        push $in_ring3
        iret

in_ring3:
        ret
