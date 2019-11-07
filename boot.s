// Mulitboot header values
.set ALIGN,    0x01             // align loaded modules on page boundaries
.set MEMINFO,  0x02             // provide memory map
.set FLAGS,    ALIGN | MEMINFO  // this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       // 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS)

// Header according to the multiboot spec. It needs to be early in the binary so
// we put it in its own section so we can control that.
.section .text.mbhdr
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

// Initial stack for kernel
.section .bss
.global stack_top
        .align 16
stack_bottom:
        .skip 0x40000
stack_top:

        .align 0x1000
page_directory:
        .skip 4096
page_table:
        .skip 4096
        // TODO: calculate the needed number of page tables depending
        // on kernel size



.section .text

//
// Entry point
//
.global _start
_start:
        push %ebx
        push %eax

        jmp map_high_mem

back:   pop %eax
        pop %ebx

        mov $stack_top, %esp
        push %ebx   // save multiboot header
        push %eax   // multiboot magic

        call _init  // ctors
        call kernel_main
        call _fini  // dtors

1:	hlt
        jmp 1b


// All aboslute addresses in the kernel are above 0xC0000000,
// but there's no physical memory there, so we need to fix this
// with paging.
map_high_mem:
        // Setup two equally mapped page dir entries
        mov $(page_table - 0xC0000000), %eax
        or $3, %eax
        mov %eax, (page_directory - 0xC0000000 + 0)
        mov %eax, (page_directory - 0xC0000000 + 3072)

        // Start at physical address 0
        mov $0, %edi

        // End at how many pages we can map using our only page table
        mov $(1024 * 4096), %ecx

        // ESI points into our page table
        mov $(page_table - 0xC0000000), %esi

        // Map the physical memory pages in the page table
1:      cmp %ecx, %edi
        jge 2f

        mov %edi, %ebx
        or $3, %ebx
        movl %ebx, (%esi)

        add $4096, %edi
        add $4, %esi
        jmp 1b

        // Setup control register to point at our page directory
2:      mov $(page_directory - 0xC0000000), %ecx
        mov %ecx, %cr3

        // Enable paging
        mov %cr0, %ecx
        or $0x80010000, %ecx
        mov %ecx, %cr0

        // Do a long jump so EIP is in the upper part the address space
        lea 4f, %ecx
        jmp *%ecx

4:      jmp back

.global enter_user_mode
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

isr_routine     isr_debug,       int_debug
isr_routine_err isr_gpf,         int_gpf
isr_routine     isr_syscall,     int_syscall
isr_routine     isr_kbd,         int_kbd
isr_routine     isr_timer,       int_timer
//isr_routine_err isr_page_fault,  int_page_fault
isr_routine_err isr_doublefault, int_doublefault


.global isr_page_fault
isr_page_fault:
        cli
        hlt
