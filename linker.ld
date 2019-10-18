ENTRY(_start)

SECTIONS {
  . = 1M;

  .text BLOCK(4K) : ALIGN(4K) {
    *(.mbhdr)
    *(.text)
  }

  .bss BLOCK(4K) : ALIGN(4K) {
    *(COMMON)
    *(.bss)
  }

  .rodata BLOCK(4K) : ALIGN(4K) {
    *(.rodata)
  }

  .data BLOCK(4K) : ALIGN(4K) {
    *(.data)
  }

  kernel_end = .;
}
