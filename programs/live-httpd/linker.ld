ENTRY(_start)

/* Without a linker script the constructor and destructors won't be
called. For some reason, gcc doesn't link things up correctly. */

SECTIONS {
  /*. = 0x00100000;*/

  .text ALIGN(4K) : AT(ADDR(.text)) {
    *(.text)
  }

  .rodata ALIGN(4K) : AT(ADDR(.rodata)) {
    *(.rodata)
  }

  .data ALIGN(4K) : AT(ADDR(.data)) {
    *(.data)
  }

  .bss ALIGN(4K) : AT(ADDR(.bss)) {
    *(.bss)
    *(COMMON)
  }
}
