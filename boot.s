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

// Initial stack for kernel
.section .bss
.align 16
stack_bottom:
.skip 0x40000
.global stack_top
stack_top:

//
// Entry point
//
.section .text
.global _start
.type _start, @function
_start:
        mov $stack_top, %esp
        push %ebx   // save multiboot header
        push %eax   // multiboot magic

        call _init  // ctors
        call kernel_start
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


//
// Switches kernel task without relying on interrupt specific logic.
//
.global switch_task
switch_task:
        push %ebx
        push %esi
        push %edi
        push %ebp

        // Save old esp so we can return at this point
        mov 20(%esp), %ebx
        mov %esp, (%ebx)

        // Change esp
        mov 24(%esp), %esp

        pop %ebp
        pop %edi
        pop %esi
        pop %ebx
        ret

.global switch_task_iret
switch_task_iret:
        iret

.macro isr_routine name,handler
.extern \handler
.global \name
\name:
        push $0  // Dummy error code
        pushal
        call \handler
        popal
        add $4, %esp
        iret
.endm

.macro isr_routine_err name,handler
.extern \handler
.global \name
\name:
        pushal
        call \handler
        popal
        add $4, %esp  // Consume the error code
        iret
.endm

isr_routine     isr_debug,      int_debug
isr_routine_err isr_gpf,        int_gpf
isr_routine     isr_syscall,    int_syscall
isr_routine     isr_kbd,        int_kbd
isr_routine     isr_timer,      int_timer
isr_routine     isr_page_fault, int_page_fault
