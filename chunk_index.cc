// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup
// contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stdio.h>
#include <string.h>
#include <new>
#include <utility>

#include "chunk_index.hh"
#include "debug.hh"
#include "dir.hh"
#include "index_file.hh"
#include "zbackup.pb.h"

ChunkIndex::Chain::Chain(ChunkId const &id, uint32_t size,
                         Bundle::Id const *bundleId)
    : next(0), size(size), bundleId(bundleId) {
  memcpy(cryptoHash, id.cryptoHash, sizeof(cryptoHash));
}

bool ChunkIndex::Chain::equalsTo(ChunkId const &id) {
  return memcmp(cryptoHash, id.cryptoHash, sizeof(cryptoHash)) == 0;
}

void ChunkIndex::loadIndex(IndexProcessor &ip) {
  Dir::Listing lst(indexPath);

  Dir::Entry entry;

  verbosePrintf("Loading index...\n");

  while (lst.getNext(entry)) {
    verbosePrintf("Loading index file %s...\n", entry.getFileName().c_str());
    try {
      string indexFn = Dir::addPath(indexPath, entry.getFileName());
      IndexFile::Reader reader(key, indexFn);

      ip.startIndex(indexFn);

      BundleInfo info;
      Bundle::Id bundleId;
      while (reader.readNextRecord(info, bundleId)) {
        Bundle::Id *savedId = storage.allocateObjects<Bundle::Id>(1);
        memcpy(savedId, &bundleId, sizeof(bundleId));

        ChunkId id;

        ip.startBundle(*savedId);

        for (int x = info.chunk_record_size(); x--;) {
          BundleInfo_ChunkRecord const &record = info.chunk_record(x);

          if (record.id().size() != ChunkId::BlobSize)
            throw exIncorrectChunkIdSize();

          id.setFromBlob(record.id().data());
          ip.processChunk(id, record.size());
        }

        ip.finishBundle(*savedId, info);
      }

      ip.finishIndex(indexFn);
    } catch (std::exception &e) {
      verbosePrintf("error: %s\n", e.what());
      continue;
    }
  }

  verbosePrintf("Index loaded.\n");
}

size_t ChunkIndex::size() { return hashTable.size(); }

void ChunkIndex::startIndex(string const &) {}

void ChunkIndex::startBundle(Bundle::Id const &bundleId) {
  lastBundleId = &bundleId;
}

void ChunkIndex::processChunk(ChunkId const &chunkId, uint32_t size) {
  registerNewChunkId(chunkId, size, lastBundleId);
}

void ChunkIndex::finishBundle(Bundle::Id const &, BundleInfo const &) {}

void ChunkIndex::finishIndex(string const &) {}

ChunkIndex::ChunkIndex(EncryptionKey const &key, TmpMgr &tmpMgr,
                       string const &indexPath, bool prohibitChunkIndexLoading)
    : key(key),
      tmpMgr(tmpMgr),
      indexPath(indexPath),
      storage(65536, 1),
      lastBundleId(NULL) {
  if (!prohibitChunkIndexLoading) loadIndex(*this);
  dPrintf("%s for %s is instantiated and initialized, hasKey: %s\n", __CLASS,
          indexPath.c_str(), key.hasKey() ? "true" : "false");
}

Bundle::Id const *ChunkIndex::findChunk(ChunkId::RollingHashPart rollingHash,
                                        ChunkInfoInterface &chunkInfo,
                                        uint32_t *size) {
  HashTable::iterator i = hashTable.find(rollingHash);

  ChunkId const *id = 0;

  if (i != hashTable.end()) {
    if (!id) id = &chunkInfo.getChunkId();
    // Check the chains
    for (Chain *chain = i->second; chain; chain = chain->next) {
      if (chain->equalsTo(*id)) {
        if (size) *size = chain->size;
        return chain->bundleId;
      }
    }
  }

  return NULL;
}

namespace {
struct ChunkInfoImmediate : public ChunkIndex::ChunkInfoInterface {
  ChunkId const &id;

  ChunkInfoImmediate(ChunkId const &id) : id(id) {}

  virtual ChunkId const &getChunkId() { return id; }
};
}

Bundle::Id const *ChunkIndex::findChunk(ChunkId const &chunkId,
                                        uint32_t *size) {
  ChunkInfoImmediate chunkInfo(chunkId);
  return findChunk(chunkId.rollingHash, chunkInfo, size);
}

ChunkIndex::Chain *ChunkIndex::registerNewChunkId(ChunkId const &id,
                                                  uint32_t size,
                                                  Bundle::Id const *bundleId) {
  HashTable::iterator i =
      hashTable.insert(std::make_pair(id.rollingHash, (Chain *)0)).first;

  Chain **chain = &i->second;

  // Check the chains
  for (; *chain; chain = &((*chain)->next))
    if ((*chain)->equalsTo(id)) {
      return NULL;  // The entry existed already
    }

  // Create a new chain
  *chain = new (storage.allocateObjects<Chain>(1)) Chain(id, size, bundleId);

  return *chain;
}

bool ChunkIndex::addChunk(ChunkId const &id, uint32_t size,
                          Bundle::Id const &bundleId) {
  if (Chain *chain = registerNewChunkId(id, size, NULL)) {
    // Allocate or re-use bundle id
    if (!lastBundleId || *lastBundleId != bundleId) {
      Bundle::Id *allocatedId = storage.allocateObjects<Bundle::Id>(1);
      memcpy(allocatedId, &bundleId, Bundle::IdSize);
      lastBundleId = allocatedId;
    }
    chain->bundleId = lastBundleId;

    return true;
  } else
    return false;
}
