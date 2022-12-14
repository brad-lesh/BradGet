#pragma once

#include <stdio.h>
#include "common.h"
#include "path.h"
#include <string>
#include <intrin.h>

class FileBuffer : public RefCounted {
public:
  virtual ~FileBuffer() {}

  virtual int getc() {
    uint8 chr;
    if (read(&chr, 1) != 1) {
      return EOF;
    }
    return chr;
  }
  virtual void putc(int chr) {
    write(&chr, 1);
  }

  virtual uint64 tell() const = 0;
  virtual void seek(int64 pos, int mode) = 0;
  virtual uint64 size() {
    uint64 pos = tell();
    seek(0, SEEK_END);
    uint64 res = tell();
    seek(pos, SEEK_SET);
    return res;
  }

  virtual size_t read(void* ptr, size_t size) = 0;
  virtual size_t write(void const* ptr, size_t size) = 0;
};

class File {
protected:
  FileBuffer* file_;
public:
  File()
    : file_(nullptr)
  {}
  File(FileBuffer* file)
    : file_(file)
  {}
  File(File const& file)
    : file_(file.file_)
  {
    if (file_) file_->addref();
  }
  File(File&& file)
    : file_(file.file_)
  {
    file.file_ = nullptr;
  }

  enum {
    READ = 0,
    REWRITE = 1,
    MODIFY = 2,
  };

  File(char const* name, uint32 mode = READ);
  File(std::string const& name, uint32 mode = READ)
    : File(name.c_str(), mode)
  {}
  ~File() {
    file_->release();
  }
  void release() {
    file_->release();
    file_ = nullptr;
  }

  File& operator=(File const& file) {
    if (file_ == file.file_) {
      return *this;
    }
    file_->release();
    file_ = file.file_;
    if (file_) file_->addref();
    return *this;
  }
  File& operator=(File&& file) {
    if (file_ == file.file_) {
      return *this;
    }
    file_->release();
    file_ = file.file_;
    file.file_ = nullptr;
    return *this;
  }

  operator bool() const {
    return file_ != nullptr;
  }

  int getc() {
    return file_->getc();
  }
  void putc(int chr) {
    file_->putc(chr);
  }

  void seek(int64 pos, int mode = SEEK_SET) {
    file_->seek(pos, mode);
  }
  uint64 tell() const {
    return file_->tell();
  }
  uint64 size() {
    return file_->size();
  }

  size_t read(void* dst, size_t size) {
    return file_->read(dst, size);
  }
  template<class T>
  T read() {
    T x;
    file_->read(&x, sizeof(T));
    return x;
  }
  uint8 read8() {
    uint8 x;
    file_->read(&x, 1);
    return x;
  }
  uint16 read16(bool big = false) {
    uint16 x;
    file_->read(&x, 2);
    if (big) x = _byteswap_ushort(x);
    return x;
  }
  uint32 read32(bool big = false) {
    uint32 x;
    file_->read(&x, 4);
    if (big) x = _byteswap_ulong(x);
    return x;
  }
  uint64 read64(bool big = false) {
    uint64 x;
    file_->read(&x, 8);
    if (big) x = _byteswap_uint64(x);
    return x;
  }

  size_t write(void const* ptr, size_t size) {
    return file_->write(ptr, size);
  }
  template<class T>
  bool write(T const& x) {
    return file_->write(&x, sizeof(T)) == sizeof(T);
  }
  bool write8(uint8 x) {
    return file_->write(&x, 1) == 1;
  }
  bool write16(uint16 x, bool big = false) {
    if (big) x = _byteswap_ushort(x);
    return file_->write(&x, 2) == 2;
  }
  bool write32(uint32 x, bool big = false) {
    if (big) x = _byteswap_ulong(x);
    return file_->write(&x, 4) == 4;
  }
  bool write64(uint64 x, bool big = false) {
    if (big) x = _byteswap_uint64(x);
    return file_->write(&x, 8) == 8;
  }

  void printf(char const* fmt, ...);

  bool getline(std::string& line);

  class LineIterator;
  LineIterator begin();
  LineIterator end();

  static File memfile(void const* ptr, size_t size, bool clone = false);
  File subfile(uint64 offset, uint64 size);

  void copy(File& src, uint64 size = max_uint64);
  void md5(void* digest);
  std::string md5();

  static bool exists(char const* path);
  static bool exists(std::string const& path) {
    return exists(path.c_str());
  }
};

class MemoryFile : public File {
public:
  MemoryFile(size_t initial = 16384, size_t grow = (1 << 20));
  uint8 const* data() const;
  size_t csize() const;
  uint8* reserve(uint32 size);
  void resize(uint32 size);
};

class File::LineIterator {
  friend class File;
  File file_;
  std::string line_;
  LineIterator(File& file) {
    if (file.getline(line_)) {
      file_ = file;
    }
  }
public:
  LineIterator() {}

  std::string const& operator*() {
    return line_;
  }
  std::string const* operator->() {
    return &line_;
  }

  bool operator!=(LineIterator const& it) const {
    return file_ != it.file_;
  }

  LineIterator& operator++() {
    if (!file_.getline(line_)) {
      file_.release();
    }
    return *this;
  }
};

class FileLoader {
  std::string root;
public:
  FileLoader(std::string const& root = path::root())
    : root(root)
  {
  }

  struct SearchResults {
    std::vector<std::string> files;
    std::vector<std::string> folders;
  };

  File load(char const* name) {
    return File(root / name);
  }
  SearchResults search(char const* mask);
};
