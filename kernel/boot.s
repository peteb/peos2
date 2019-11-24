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
        pushal

        // 36 = popad (32) + retval (4) = 1st argument to function
        mov 36(%esp), %ebx
        mov %esp, (%ebx)

        // Change esp and cr3, 40 = 2nd argument
        mov 40(%esp), %eax
        mov 44(%esp), %ebx
        mov %ebx, %cr3
        mov %eax, %esp

        popal
        ret

.global switch_task_iret
switch_task_iret:
        // This function is special; return IP comes *after* arguments
        push %eax
        mov 4(%esp), %ax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        pop %eax

        // Consume DS
        add $4, %esp

        iret

.macro isr_routine_common handler
        // Save GRPs and caller DS
        pushal
        xor %eax, %eax
        mov %ds, %ax
        push %eax

        // Set the kernel data segment selector
        mov $0x10, %ax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs

        push %esp  // isr_registers pointer
        call \handler
        add $4, %esp

        // Restore caller DS and GPRs
        pop %eax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs
        popal

        // Consume the error code
        add $4, %esp

        // Return to user mode/caller
        iret
.endm

.macro isr_routine name,handler
.extern \handler
.global \name
\name:
        // Push dummy code
        push $0x0

        isr_routine_common \handler
.endm

.macro isr_routine_err name,handler
.extern \handler
.global \name
\name:
        isr_routine_common \handler
.endm

        isr_routine     isr_divzero,     int_divzero
        isr_routine     isr_debug,       int_debug
        isr_routine     isr_nmi,         int_nmi
        isr_routine     isr_breakpoint,  int_breakpoint
        isr_routine     isr_overflow,    int_overflow
        isr_routine     isr_bre,         int_bre
        isr_routine     isr_invop,       int_invop
        isr_routine     isr_devnotavail, int_devnotavail
        isr_routine_err isr_doublefault, int_doublefault
        isr_routine_err isr_invtss,      int_invtss
        isr_routine_err isr_segnotpres,  int_segnotpres
        isr_routine_err isr_stacksegfault, int_stacksegfault
        isr_routine_err isr_gpf,         int_gpf
        isr_routine_err isr_page_fault,  int_page_fault
        isr_routine     isr_fpe,         int_fpe
        isr_routine_err isr_align,       int_align
        isr_routine     isr_machine,     int_machine
        isr_routine     isr_simdfp,      int_simdfp
        isr_routine     isr_virt,        int_virt
        isr_routine     isr_sec,         int_sec

        isr_routine     isr_syscall,     int_syscall
        isr_routine     isr_kbd,         int_kbd
        isr_routine     isr_timer,       int_timer

        isr_routine     isr_spurious,    int_spurious

        isr_routine     isr_com1,        int_com1
        isr_routine     isr_rtl8139,     int_rtl8139
