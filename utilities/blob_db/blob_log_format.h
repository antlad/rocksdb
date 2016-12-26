//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Log format information shared by reader and writer.

#pragma once

#ifndef ROCKSDB_LITE

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include "rocksdb/status.h"
#include "rocksdb/types.h"

namespace rocksdb {

namespace blobstorage {
class BlobFile;
class BlobDBImpl;

enum RecordType : uint8_t {
  // Zero is reserved for preallocated files
  kFullType = 0,

  // For fragments
  kFirstType = 1,
  kMiddleType = 2,
  kLastType = 3,
  kMaxRecordType = kLastType
};

enum RecordSubType : uint8_t {
  kRegularType = 0,
  kTTLType = 1,
  kTimestampType = 2,
};

extern const uint32_t kMagicNumber;

class Reader;

typedef std::pair<uint32_t, uint32_t> ttlrange_t;
typedef std::pair<uint64_t, uint64_t> tsrange_t;
typedef std::pair<rocksdb::SequenceNumber, rocksdb::SequenceNumber> snrange_t;

class BlobLogHeader {
  friend class BlobFile;
  friend class BlobDBImpl;

 private:
  uint32_t magic_number_ = 0;
  std::unique_ptr<ttlrange_t> ttl_guess_;
  std::unique_ptr<tsrange_t> ts_guess_;

 private:
  void setTTLGuess(const ttlrange_t& ttl) {
    ttl_guess_.reset(new ttlrange_t(ttl));
  }

  void setTSGuess(const tsrange_t& ts) { ts_guess_.reset(new tsrange_t(ts)); }

 public:
  // magic number + flags + ttl guess + timestamp range
  static const size_t kHeaderSize = 4 + 4 + 4 * 2 + 8 * 2;
  // 32

  void EncodeTo(std::string* dst) const;

  Status DecodeFrom(Slice* input);

  BlobLogHeader();

  bool HasTTL() const { return !!ttl_guess_; }

  bool HasTimestamp() const { return !!ts_guess_; }

  BlobLogHeader& operator=(BlobLogHeader&& in) noexcept;
};

// Footer encapsulates the fixed information stored at the tail
// end of every blob log file.
class BlobLogFooter {
  friend class BlobFile;

 public:
  // Use this constructor when you plan to write out the footer using
  // EncodeTo(). Never use this constructor with DecodeFrom().
  BlobLogFooter();

  uint64_t magic_number() const { return magic_number_; }

  void EncodeTo(std::string* dst) const;

  Status DecodeFrom(Slice* input);

  // convert this object to a human readable form
  std::string ToString() const;

  // footer size = 4 byte magic number
  // 8 bytes count
  // 4, 4 - ttl range
  // 8, 8 - sn range
  // 8, 8 - ts range
  // = 56
  static const int kFooterSize = 4 + 4 + 8 + (4 * 2) + (8 * 2) + (8 * 2);

  bool HasTTL() const { return !!ttl_range_; }

  bool HasTimestamp() const { return !!ts_range_; }

  uint64_t GetBlobCount() const { return blob_count_; }

  const ttlrange_t& GetTTLRange() const { return *(ttl_range_.get()); }

  const tsrange_t& GetTimeRange() const { return *(ts_range_.get()); }

  const snrange_t& GetSNRange() const { return sn_range_; }

 private:
  uint32_t magic_number_ = 0;
  uint64_t blob_count_ = 0;

  std::unique_ptr<ttlrange_t> ttl_range_;
  std::unique_ptr<tsrange_t> ts_range_;
  snrange_t sn_range_;

 private:
  void setTTLRange(const ttlrange_t& ttl) {
    ttl_range_.reset(new ttlrange_t(ttl));
  }
  void setTimeRange(const tsrange_t& ts) { ts_range_.reset(new tsrange_t(ts)); }
};

extern const size_t kBlockSize;

class BlobLogRecord {
  friend class Reader;

 private:
  // this might not be set.
  uint32_t checksum_;
  uint32_t header_cksum_;
  uint32_t key_size_;
  uint64_t blob_size_;
  uint64_t time_val_;
  uint32_t ttl_val_;
  SequenceNumber sn_;
  char type_;
  char subtype_;
  Slice key_;
  Slice blob_;
  std::unique_ptr<char[]> key_buffer_;
  size_t kbs_;
  std::unique_ptr<char[]> blob_buffer_;
  size_t bbs_;

 private:
  void clear();

  char* getKeyBuffer() { return key_buffer_.get(); }

  char* getBlobBuffer() { return blob_buffer_.get(); }

  void resizeKeyBuffer(size_t kbs);

  void resizeBlobBuffer(size_t bbs);

 public:
  // Header is checksum (4 bytes), header checksum (4bytes),
  // Key Length ( 4 bytes ),
  // Blob Length ( 8 bytes), timestamp/ttl (8 bytes),
  // type (1 byte), subtype (1 byte)
  // = 34
  static const int kHeaderSize = 4 + 4 + 4 + 8 + 4 + 8 + 1 + 1;

  static const int kFooterSize = 8;

 public:
  BlobLogRecord();

  ~BlobLogRecord();

  const Slice& Key() const { return key_; }

  const Slice& Blob() const { return blob_; }

  uint32_t GetKeySize() const { return key_size_; }

  uint64_t GetBlobSize() const { return blob_size_; }

  uint32_t GetTTL() const { return ttl_val_; }

  uint64_t GetTimeVal() const { return time_val_; }

  SequenceNumber GetSN() const { return sn_; }

  Status DecodeHeaderFrom(Slice* input);
};

}  // namespace blobstorage
}  // namespace rocksdb
#endif  // ROCKSDB_LITE
