#include "tar.h"
#include "screen.h"
#include "support/format.h"

struct header {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char type;
  char linkname[100];
  char tail[255];
};

void tar_parse(const void *data, size_t size, tar_parse_callback callback_fun, void *opaque) {
  if (size == 0) {
    return;
  }

  assert(data);

  (void)callback_fun;
  (void)opaque;
  // Casting `data` to a header * should be fine; the struct is just chars
  header *hdr = (header *)data;
  (void)hdr;
}
