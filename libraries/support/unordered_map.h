// -*- c++ -*-

#ifndef PEOS2_SUPPORT_UNORDERED_MAP_H
#define PEOS2_SUPPORT_UNORDERED_MAP_H

#include <stddef.h>

#include "support/pool.h"

namespace p2 {
template<typename _Key, typename _Value, size_t _Capacity>
class unordered_map {
public:
  struct entry {
    entry(const _Key &key, const _Value &value) : key(key), value(value) {}
    _Key key; _Value value;
  };

private:
  using storage_t = p2::pool<entry, _Capacity>;

public:
  using iterator = typename storage_t::iterator;
  using const_iterator = typename storage_t::const_iterator;

  iterator find(const _Key &key)
  {
    for (auto it = begin(); it != end(); ++it) {
      if (it->key == key)
        return it;
    }

    return end();
  }

  // Doesn't overwrite existing entries
  bool insert(const _Key &key, const _Value &value)
  {
    if (find(key) != end())
      return false;

    _storage.emplace_anywhere(key, value);
    return true;
  }

  void erase(const iterator &iterator)
  {
    _storage.erase(iterator);
  }

  _Value &operator [](const _Key &key)
  {
    if (auto it = find(key); it != end())
      return it->value;

    insert(key, _Value());
    // TODO: return value from insert

    auto it = find(key);
    assert(it != end());
    return it->value;
  }

  size_t size() const          {return _storage.size(); }
  iterator begin()             {return _storage.begin(); }
  iterator end()               {return _storage.end(); }
  const_iterator begin() const {return _storage.begin(); }
  const_iterator end() const   {return _storage.end(); }

private:
  storage_t _storage;
};


}

#endif // !PEOS2_SUPPORT_UNORDERED_MAP_H
