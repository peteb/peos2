// -*- c++ -*-

#ifndef PEOS2_SUPPORT_PAGE_ALLOC_H
#define PEOS2_SUPPORT_PAGE_ALLOC_H

#include <stdint.h>

namespace p2 {
  struct region {
    uintptr_t start, end;

    uintptr_t size() const {
      return end - start;
    }
  };

  class page_allocator {
  public:
    page_allocator(uintptr_t start_adr, uintptr_t end_adr)
      : _start_adr(start_adr),
        _end_adr(end_adr)
    {
      assert((start_adr & 0xFFF) == 0 && "range must be 4k aligned");
      assert((end_adr & 0xFFF) == 0 && "range must be 4k aligned");

      // Calculate how much space we need for the freelist stack
      uintptr_t needed_for_bookkeeping = ((end_adr - start_adr) / 0x1000) * sizeof(uintptr_t);
      uintptr_t freelist_size_pages = (needed_for_bookkeeping + 0x0FFF) / 0x1000;

      // TODO: we're wasting some space here because the freelist
      // takes space out of the allocatable region. Make this tighter.
      _fs_start = (uintptr_t *)start_adr;
      _fs_head = _fs_start;
      _watermark = (char *)(start_adr + freelist_size_pages * 0x1000);
      _fs_end = (uintptr_t *)_watermark;
    }

    void *alloc_page() {
      if (_fs_head != _fs_start) {
        // Take address from free list
        assert(_fs_head > _fs_start);
        assert(_fs_head < _fs_end);
        return (void *)*--_fs_head;
      }

      assert((uintptr_t)_watermark < _end_adr);
      return (void *)(_watermark += 0x1000);
    }

    void *alloc_page_zero() {
      void *mem = alloc_page();
      memset(mem, 0, 0x1000);
      return mem;
    }

    void free_page(void *page) {
      assert(((uintptr_t)page & 0xFFF) == 0);

      if (page == _watermark - 0x1000) {
        assert((uintptr_t *)_watermark > _fs_end);
        _watermark -= 0x1000;
      }
      else {
        assert(_fs_head < _fs_end);
        assert(_fs_head >= _fs_start);
        *_fs_head++ = (uintptr_t)page;
      }
    }

    size_t free_space() const {
      return ((_end_adr - (uintptr_t)_watermark) + (_fs_head - _fs_start)) / 0x1000;
    }

  private:
    // _start_adr <= _fs_start <= fs_head < _watermark < _end_adr
    uintptr_t _start_adr, _end_adr;
    char *_watermark;

    // Free list/stack
    uintptr_t *_fs_start, *_fs_end, *_fs_head;
  };
};


#endif // !PEOS2_SUPPORT_PAGE_ALLOC_H
