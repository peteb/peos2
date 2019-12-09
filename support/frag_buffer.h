// -*- c++ -*-

#ifndef PEOS2_SUPPORT_FRAG_BUFFER_H
#define PEOS2_SUPPORT_FRAG_BUFFER_H

#include <support/assert.h>
#include <support/utils.h>
#include <support/pool.h>

namespace p2 {

  template<int _MaxLen>
  class frag_buffer {
  public:
    frag_buffer() {reset(); }

    void reset()
    {
      _holes.clear();
      _holes.emplace_anywhere(0, p2::numeric_limits<int>::max());
    }

    void insert(int start_pos, const char *data, int length)
    {
      const int end_pos = p2::min(start_pos + length, _MaxLen);
      const hole addition(start_pos, end_pos);

      // Algorithm:
      // 1. find overlapping hole
      // 2. replace hole with two holes for the leftovers
      //    on the left and right sides
      // 3. loop
      //
      // If a pool collection is added to during iteration, the added
      // item can turn up, so it's important that holes that are
      // edge-to-edge aren't considered overlapping.

      // TODO: remove O(n), maybe binary search? Though might be too much
      // overhead, and can't use p2::pool

      bool removed_holes = false;

      for (int i = 0; i < _holes.watermark(); ++i) {
        if (!_holes.valid(i))
          continue;

        const hole existing_hole = _holes[i];

        if (addition.overlaps(existing_hole)) {
          int leftover_left = p2::max(addition.start - existing_hole.start, 0);
          int leftover_right = p2::max(existing_hole.end - addition.end, 0);

          _holes.erase(i);

          if (leftover_left > 0)
            _holes.emplace_anywhere(existing_hole.start, existing_hole.start + leftover_left);

          if (leftover_right > 0)
            _holes.emplace_anywhere(existing_hole.end - leftover_right, existing_hole.end);

          removed_holes = true;
        }
      }

      if (removed_holes) {
        memcpy(_data + start_pos, data, length);
      }
    }

    const uint8_t *data() const
    {
      return _data;
    }

    int continuous_size() const
    {
      // Having continuous data implies a lack of holes except for the
      // hole that ends at INT_MAX. Note that this function is an
      // offline function -- it won't return "continuous size so far".

      if (_holes.size() != 1) {
        return 0;
      }

      // Find the single hole. TODO: make this O(1)
      for (int i = 0; i < _holes.watermark(); ++i) {
        if (_holes.valid(i))
          return _holes[i].start;
      }

      assert(!"unreachable");
      return 0;
    }

    // size - returns the largest block of valid data beginning at 0
    /*int size() const
    {
      return _size;
      }*/

  private:
    class hole {
    public:
      hole(int start, int end) : start(start), end(end) {}

      bool overlaps(const hole &other) const
      {
        return other.start < end && other.end > start;
      }

      int start, end;
    };

    uint8_t _data[_MaxLen];
    p2::pool<hole, _MaxLen / 40> _holes;
  };

}

#endif // !PEOS2_SUPPORT_FRAG_BUFFER
