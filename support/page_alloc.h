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

  //
  // Allocates page-sized blocks that can be used for mapping virtual
  // pages. Addresses are typically physical and the memory address
  // cannot be read before it's been mapped in the address space.
  //
  // A chunk at the beginning of the memory region is used for
  // bookkeeping and has to be mapped linearly in accessing address
  // spaces.
  //
  class page_allocator {
  public:
    //
    // page_allocator - initializes the structure
    // @phys_region: range to draw pages from
    // @virtual_offset: used for accessing the bookkeeping area
    //
    page_allocator(const region &phys_region, uintptr_t virtual_offset)
      : _phys_region(phys_region)
    {
      assert((phys_region.start & 0xFFF) == 0 && "range must be 4k aligned");
      assert((phys_region.end & 0xFFF) == 0 && "range must be 4k aligned");

      // Calculate how much space we need for the freelist stack
      uintptr_t needed_for_bookkeeping = ((phys_region.end - phys_region.start) / 0x1000) * sizeof(uintptr_t);
      uintptr_t freelist_size_pages = (needed_for_bookkeeping + 0x0FFF) / 0x1000;

      // TODO: we're wasting some space here because the freelist
      // takes space out of the allocatable region. Make this tighter.
      _phys_bookkeeping.start = phys_region.start;
      _phys_bookkeeping.end = phys_region.start + freelist_size_pages * 0x1000;

      assert((_phys_bookkeeping.start & 0xFFF) == 0);
      assert((_phys_bookkeeping.end & 0xFFF) == 0);

      _fs_start = (uintptr_t *)(_phys_bookkeeping.start + virtual_offset);
      _fs_end = (uintptr_t *)(_phys_bookkeeping.end + virtual_offset);
      _fs_head = _fs_start;

      _watermark = (char *)_phys_bookkeeping.end;
    }

    void *alloc_page()
    {
      if (_fs_head != _fs_start) {
        // Take address from free list
        assert(_fs_head > _fs_start);
        assert(_fs_head < _fs_end);
        return (void *)*--_fs_head;
      }

      assert((uintptr_t)_watermark < _phys_region.end);
      return (void *)(_watermark += 0x1000);
    }

    void *alloc_page_zero()
    {
      void *mem = alloc_page();
      memset(mem, 0, 0x1000);
      return mem;
    }

    void free_page(void *page)
    {
      assert(((uintptr_t)page & 0xFFF) == 0);

      if (page == _watermark - 0x1000) {
        assert((uintptr_t)_watermark > _phys_bookkeeping.end);
        _watermark -= 0x1000;
      }
      else {
        assert(_fs_head < _fs_end);
        assert(_fs_head >= _fs_start);
        *_fs_head++ = (uintptr_t)page;
      }
    }

    size_t free_space() const
    {
      return ((_phys_region.end - (uintptr_t)_watermark) + (_fs_head - _fs_start)) / 0x1000;
    }

    //
    // Returns the region that has to be mapped into calling address
    // spaces.
    //
    region bookkeeping_phys_region() const
    {
      return _phys_bookkeeping;
    }

  private:
    // _start_adr <= _fs_start <= fs_head < _watermark < _end_adr
    region _phys_region, _phys_bookkeeping;
    char *_watermark;

    // Free list/stack
    uintptr_t *_fs_start, *_fs_end, *_fs_head;
  };

  template<size_t _Capacity, size_t _Align>
  class internal_page_allocator : public page_allocator {
  public:
    internal_page_allocator(uintptr_t virtual_ofs = 0)
      : page_allocator({(uintptr_t)_storage,
                        (uintptr_t)&_storage[_Capacity]},
                       virtual_ofs)
    {}

  private:
    char _storage[_Capacity] alignas(_Align);
  };
};


#endif // !PEOS2_SUPPORT_PAGE_ALLOC_H
