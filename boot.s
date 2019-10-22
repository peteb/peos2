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


.global enter_user_mode
.type enter_user_mode, @function
enter_user_mode:
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
        push $.in_ring3
        iret

.in_ring3:
        ret


.global load_gdt
.type load_gdt, @function
load_gdt:
        mov 4(%esp), %eax        // GDT base + limit
        mov 8(%esp), %ecx        // Data segment selector

        lgdt (%eax)
        jmp $0x08, $.update_data_segs

.update_data_segs:
        mov %cx, %ds
        mov %cx, %es
        mov %cx, %fs
        mov %cx, %gs
        mov %cx, %ss
        mov %cr0, %eax
        or $1, %eax
        mov %eax, %cr0
        ret


.macro isr_routine name,handler
.extern \handler
.global \name
\name:
        pusha
        call \handler
        popa
        iret
.endm

isr_routine   isr_debug, int_debug
isr_routine     isr_gpf, int_gpf
isr_routine isr_syscall, int_syscall
isr_routine     isr_kbd, int_kbd
