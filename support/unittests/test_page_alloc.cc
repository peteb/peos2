#include "support/unittest.h"
#include "support/page_alloc.h"

TESTSUITE(p2::page_alloc) {
  TESTCASE("too small arena panics") {
    // The allocator needs an area of at least 4096 +
    // ALIGN_UP(sizeof(uintptr_t), 0x1000) (bookkeeping)
    char buf[1024] alignas(0x1000);
    ASSERT_PANIC(p2::page_allocator({(uintptr_t)buf, (uintptr_t)buf + sizeof(buf)}, 0));
  }

  TESTCASE("still too small panics") {
    char buf[0x1000] alignas(0x1000);  // We also need space for bookkeeping
    ASSERT_PANIC(p2::page_allocator({(uintptr_t)buf, (uintptr_t)buf + sizeof(buf)}, 0));
  }

  TESTCASE("still too small panics") {
    char buf[0x1FFFF] alignas(0x1000);
    ASSERT_PANIC(p2::page_allocator({(uintptr_t)buf, (uintptr_t)buf + sizeof(buf)}, 0));
  }

  TESTCASE("size for exactly one payload + bookkeeping page is good enough") {
    char buf[0x2000] alignas(0x1000);
    p2::page_allocator alloc({(uintptr_t)buf, (uintptr_t)buf + sizeof(buf)}, 0);
    // No panic
    ASSERT_EQ(alloc.free_pages(), 1u);
  }

  TESTCASE("free_pages: calculated correctly for larger arena") {
    char buf[0x1000 + 0x1000 * 32] alignas(0x1000);
    p2::page_allocator alloc({(uintptr_t)buf, (uintptr_t)buf + sizeof(buf)}, 0);

    ASSERT_EQ(alloc.free_pages(), 32u);
  }

  TESTCASE("scenario: minimum allocation") {
    char buf[0x2000] alignas(0x1000);
    p2::page_allocator alloc({(uintptr_t)buf, (uintptr_t)buf + sizeof(buf)}, 0);

    void *page = alloc.alloc_page();
    ASSERT_EQ((uintptr_t)page, (uintptr_t)&buf[0x1000]);
    ASSERT_EQ(alloc.free_pages(), 0u);
    alloc.free_page(page);
    ASSERT_EQ(alloc.free_pages(), 1u);

    page = alloc.alloc_page();
    ASSERT_EQ((uintptr_t)page, (uintptr_t)&buf[0x1000]);

    ASSERT_PANIC(alloc.alloc_page());
  }

  TESTCASE("scenario: free list allocation") {
    char buf[0x1000 + 0x1000 * 32] alignas(0x1000);
    p2::page_allocator alloc({(uintptr_t)buf, (uintptr_t)buf + sizeof(buf)}, 0);

    void *page1 = alloc.alloc_page();
    ASSERT_EQ(alloc.free_pages(), 31u);

    void *page2 = alloc.alloc_page();
    ASSERT_EQ(alloc.free_pages(), 30u);

    void *page3 = alloc.alloc_page();
    ASSERT_EQ(alloc.free_pages(), 29u);

    void *page4 = alloc.alloc_page();
    ASSERT_EQ(alloc.free_pages(), 28u);

    // Remove one item, causing a hole (so the item is pushed on the free list/stack)
    alloc.free_page(page2);
    ASSERT_EQ(alloc.free_pages(), 29u);

    // Check that new allocation will use the free list
    void *page2_temp = alloc.alloc_page();
    ASSERT_EQ(page2_temp, page2);
    ASSERT_EQ(alloc.free_pages(), 28u);

    // Free all pages (puting them all on the free list
    alloc.free_page(page1);
    alloc.free_page(page2);
    alloc.free_page(page3);
    alloc.free_page(page4);

    ASSERT_EQ(alloc.free_pages(), 32u);

    // Allocate the rest of the space
    for (int i = 0; i < 32; ++i)
      alloc.alloc_page();

    ASSERT_EQ(alloc.free_pages(), 0u);
    ASSERT_PANIC(alloc.alloc_page());
  }

  TESTCASE("scenario: watermark allocation/free") {
    char buf[0x1000 + 0x1000 * 32] alignas(0x1000);
    p2::page_allocator alloc({(uintptr_t)buf, (uintptr_t)buf + sizeof(buf)}, 0);

    void *page1 = alloc.alloc_page();
    void *page2 = alloc.alloc_page();
    void *page3 = alloc.alloc_page();
    void *page4 = alloc.alloc_page();

    alloc.free_page(page4);
    ASSERT_EQ(alloc.free_pages(), 29u);
    alloc.free_page(page3);
    ASSERT_EQ(alloc.free_pages(), 30u);

    // Add the page back and see that we get the same address
    void *page3_temp = alloc.alloc_page();
    ASSERT_EQ(alloc.free_pages(), 29u);
    ASSERT_EQ(page3_temp, page3);

    alloc.free_page(page3_temp);
    ASSERT_EQ(alloc.free_pages(), 30u);

    alloc.free_page(page2);
    ASSERT_EQ(alloc.free_pages(), 31u);
    alloc.free_page(page1);
    ASSERT_EQ(alloc.free_pages(), 32u);

    // Allocate the rest of the space
    for (int i = 0; i < 32; ++i)
      alloc.alloc_page();

    ASSERT_EQ(alloc.free_pages(), 0u);
    ASSERT_PANIC(alloc.alloc_page());
  }

}
