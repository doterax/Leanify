#ifndef LIBRARY_H_
#define LIBRARY_H_

#include <optional>
#include <string>

class LibraryEntry {
 private:
  std::string _path;

 public:
  explicit LibraryEntry(std::string path);

  bool isExists() const;
  void Save(const void* data, size_t dataSize);
  size_t Load(void* data, size_t dataSize) const;
};

class Library {
 public:
  static void Initialize(const std::string& library);
  static std::optional<LibraryEntry> GetEntry(void* data, size_t dataSize, const char* tag);
  static const std::string& GetStorageName();
};

#endif