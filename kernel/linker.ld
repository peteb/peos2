ENTRY(_start)

SECTIONS {
  . = 0x00100000;

  /* Virtual address with straight mapping to 0x00100000 */
  kernel_start = . + 0xC0000000;

  /* Multiboot header, needs to be early */
  .text.mbhdr : { *(.text.mbhdr) }

  /* Enter upper half virtual memory */
  . += 0xC0000000;
  __start_READONLY = .;

  .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000) {
    *(.text)
  }

  .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000) {
    *(.rodata)
  }

  . = ALIGN(4K);
  __stop_READONLY = .;

  .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000) {
    *(.data)
  }

  .bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000) {
    *(.bss)
    *(COMMON)
  }

  . = ALIGN(4K);
  kernel_end = .;
}
