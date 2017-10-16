// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup
// contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef SHA256_HH_INCLUDED
#define SHA256_HH_INCLUDED

#include <openssl/sha.h>
#include <stddef.h>
#include <string>

using std::string;

/// A simple wrapper over openssl
class Sha256 {
  SHA256_CTX ctx;

 public:
  enum {
    // Number of bytes a digest has
    Size = SHA256_DIGEST_LENGTH
  };

  Sha256();

  /// Adds more data
  void add(void const *data, size_t size);

  /// Result should point at at least Size bytes
  void finish(void *result);

  /// Returns result as a string blob
  string finish();
};

#endif
