// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup
// contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef BACKUP_COLLECTOR_HH_INCLUDED
#define BACKUP_COLLECTOR_HH_INCLUDED

#include <string>
#include <vector>

#include "backup_file.hh"
#include "backup_restorer.hh"
#include "config.hh"

#include "debug.hh"

using std::string;

class BundleCollector : public IndexProcessor {
 private:
  string bundlesPath;
  ChunkStorage::Reader *chunkStorageReader;
  ChunkStorage::Writer *chunkStorageWriter;
  bool gcDeep;
  Config config;

  Bundle::Id savedId;
  int totalChunks, usedChunks, indexTotalChunks, indexUsedChunks;
  int indexModifiedBundles, indexKeptBundles, indexRemovedBundles;
  bool indexModified, indexNecessary;
  vector<string> filesToUnlink;
  BackupRestorer::ChunkSet overallChunkSet;
  std::set<Bundle::Id> overallBundleSet;

  void copyUsedChunks(BundleInfo const &info);

 public:
  BundleCollector(string const &bundlesPath, ChunkStorage::Reader *,
                  ChunkStorage::Writer *, bool gcDeep, Config &config);

  BackupRestorer::ChunkSet usedChunkSet;

  void startIndex(string const &indexFn);

  void finishIndex(string const &indexFn);

  void startBundle(Bundle::Id const &bundleId);

  void processChunk(ChunkId const &chunkId, uint32_t size);

  void finishBundle(Bundle::Id const &bundleId, BundleInfo const &info);

  void commit();
};

#endif
