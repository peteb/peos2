#include <support/string.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

using namespace p2;

static int read_line(char *out, size_t length);

static char input_buffer[512];
static size_t input_buffer_size;

int main(int /*argc*/, char */*argv*/[]) {
  syscall3(write, 0, "WELCOME TO SHELL\n", 17);

  while (true) {
    syscall3(write, 0, "> ", 2);

    char line[128];
    int bytes_read = verify(read_line(line, sizeof(line) - 1));

    if (bytes_read == 0 || line[bytes_read - 1] != '\n') {
      puts("Malformed line, ignoring");
      continue;
    }

    line[bytes_read - 1] = '\0';

    puts(0, p2::format<128>("read %d bytes: '%s'", bytes_read, line));
  }

  return 0;
}

START(main);

void panic(const char *explanation) {
  syscall3(write, 0, explanation, strlen(explanation));
  while (true);
}

static int consume_line_from_buffer(char *out, size_t length)
{
  const char *nl = strnchr(input_buffer, '\n', input_buffer_size);

  if (!nl)
    return 0;

  ++nl; // Move one past \n

  size_t rest = (input_buffer + input_buffer_size) - nl;
  size_t bytes_to_copy = p2::min<size_t>(length, nl - input_buffer);
  memcpy(out, input_buffer, bytes_to_copy);

  // Move rest of data to top of buffer. Should be fine, even though
  // it's an overlapping memcpy (for now ...)  Note also that if `length` is
  // less than what we need for the full line, the rest of the line
  // will be discarded.
  // TODO: use memmove
  memcpy(input_buffer, nl, rest);
  input_buffer_size -= (nl - input_buffer);
  return bytes_to_copy;
}

static int read_line(char *out, size_t length)
{
  while (true) {
    if (int bytes_read = consume_line_from_buffer(out, length); bytes_read != 0)
      return bytes_read;

    int bytes_read = syscall3(read,
                              0,
                              input_buffer + input_buffer_size,
                              sizeof(input_buffer) - input_buffer_size);
    if (bytes_read <= 0)
      return bytes_read;

    input_buffer_size += bytes_read;
  }

  return 0;
}
