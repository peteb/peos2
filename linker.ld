ENTRY(_start)

SECTIONS {
  . = 0xC0100000;

  .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000) {
    *(.mbhdr)
    *(.text)
  }

  .bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000) {
    *(COMMON)
    *(.bss)
  }

  .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000) {
    *(.rodata)
  }

  .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000) {
    *(.data)
  }

  kernel_end = .;
}
