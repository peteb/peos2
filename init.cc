#include "syscall_decls.h"
#include "support/format.h"

//
// Kernel kicks off execution of this user space program "as soon as
// possible", which is after the kernel and all the drivers have been
// initialized.  The function is run at 0x00101000, the page after the
// multiboot header, and that's the reason why we put the code in the
// .text.init section.
//
extern "C" __attribute__((section(".text.init"))) int init_main() {
  // Setup
  int stdout = SYSCALL2(open, "/dev/term0", 0);
  //int stdin = SYSCALL2(open, "/dev/term0", 0);


  static char bss_data = 0;
  char stack_data = 0;

  {
    p2::format<64> out("BSS data: %x, stack data: %x\n", (uintptr_t)&bss_data, (uintptr_t)&stack_data);
    SYSCALL3(write, stdout, out.str().c_str(), out.str().size());
  }

  while (true) {}
  /*// Create a file in the ramfs
  static char contents[] = "hello there";
  int test_fd = SYSCALL2(open, "/ramfs/test_file", 0);
  SYSCALL4(control, test_fd, CTRL_RAMFS_SET_FILE_RANGE, (uint32_t)contents, sizeof(contents) - 1);

  {
    // Try to read it
    int read_fd = SYSCALL2(open, "/ramfs/test_file", 0);
    char buf[10];

    int bytes_read;
    while ((bytes_read = SYSCALL3(read, read_fd, buf, sizeof(buf) - 1)) > 0) {
      buf[bytes_read] = '\0';
      p2::format<256> out("Read %d bytes: \"%s\"\n", bytes_read, buf);
      SYSCALL3(write, stdout, out.str().c_str(), out.str().size());
    }
  }

  // Start I/O
  SYSCALL3(write, stdout, "Hellote!\n", 9);

  while (true) {
    char input[240];
    int read = SYSCALL3(read, stdin, input, sizeof(input) - 1);
    input[read] = '\0';

    p2::format<sizeof(input) + 50> output("Read %d bytes: %s");
    output % read % input;
    SYSCALL3(write, stdout, output.str().c_str(), output.str().size());
    }*/

  return 0;
}
