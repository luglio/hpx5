// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <farmhash.h>
#include <libcuckoo/cuckoohash_map.hh>
#include <libhpx/gpa.h>
#include "btt.h"

namespace {
  typedef uint64_t Key;

  class Hasher {
   public:
    size_t operator()(const Key key) const {
      return util::Hash64(reinterpret_cast<const char*>(&key), sizeof(key));
    }
  };

  typedef std::tuple<int32_t, int32_t, void*> Entry;
  typedef cuckoohash_map<hpx_addr_t, Entry, Hasher> Map;

  class BlockTranslationTable : Map {
   public:
    BlockTranslationTable(size_t);
    bool trypin(hpx_addr_t gva, void** lva);
    void unpin(hpx_addr_t gva);
    uint32_t getOwner(hpx_addr_t gva) const;
  };

  Key KeyFromAddr(hpx_addr_t gva) {
    return gva;
  }

  uint32_t GetHome(hpx_addr_t gva) {
    return gpa_to_rank(gva);
  }
}

BlockTranslationTable::BlockTranslationTable(size_t size) : Map(size) {
}

bool
BlockTranslationTable::trypin(hpx_addr_t gva, void** lva) {
  Key key = KeyFromAddr(gva);
  return update_fn(key, [lva](Entry& entry) {
      std::get<1>(entry)++;
      if (lva) {
        *lva = std::get<2>(entry);
      }
    });
}

void
BlockTranslationTable::unpin(hpx_addr_t gva) {
  Key key = KeyFromAddr(gva);
  bool found = update_fn(gva, [](Entry& entry) {
      std::get<1>(entry)--;
    });
  assert(found);
}

uint32_t
BlockTranslationTable::getOwner(hpx_addr_t gva) const {
  Entry entry;
  Key key = KeyFromAddr(gva);
  bool found = find(key, entry);
  if (found) {
    return std::get<0>(entry);
  }
  else {
    return GetHome(gva);
  }
}

void *
btt_new(size_t size) {
  return new BlockTranslationTable(size);
}

void
btt_delete(void* obj) {
  BlockTranslationTable *btt = static_cast<BlockTranslationTable*>(obj);
  delete btt;
}

bool
btt_try_pin(void* obj, hpx_addr_t gva, void** lva) {
  BlockTranslationTable *btt = static_cast<BlockTranslationTable*>(obj);
  return btt->trypin(gva, lva);
}

void
btt_unpin(void* obj, hpx_addr_t gva) {
  BlockTranslationTable *btt = static_cast<BlockTranslationTable*>(obj);
  btt->unpin(gva);
}

uint32_t
btt_owner_of(const void* obj, hpx_addr_t gva) {
  const BlockTranslationTable *btt =
  static_cast<const BlockTranslationTable*>(obj);
  btt->getOwner(gva);
}