#include "library.h"
#include "fileio.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <filesystem>

#include <SHA1/sha1.hpp>

// --- LibraryEntry implementation ---

LibraryEntry::LibraryEntry(std::string path) : _path(std::move(path)) {}

bool LibraryEntry::isExists() const {
  return std::filesystem::exists(_path);
}

void LibraryEntry::Save(const void* data, size_t dataSize) {
  WriteFile(_path, static_cast<const uint8_t*>(data), dataSize);
}

size_t LibraryEntry::Load(void* data, size_t dataSize) const {
  auto content = ReadFile(_path);
  if (!content || content->empty())
    return 0;
  size_t copySize = std::min(content->size(), dataSize);
  std::memcpy(data, content->data(), copySize);
  return copySize;
}

// --- DirectoryStorage ---

class DirectoryStorage {
 private:
  std::string _pathToDirectory;

  LibraryEntry CreateEntry(const std::string& hash) {
    auto firstTwo = std::string(hash, 0, 2);
    auto directory = std::filesystem::path(_pathToDirectory) / firstTwo;
    std::filesystem::create_directories(directory);
    auto fullPath = directory / hash;
    return LibraryEntry(fullPath.string());
  }

 public:
  explicit DirectoryStorage(const std::string& pathToDirectory) : _pathToDirectory(pathToDirectory) {
    if (_pathToDirectory == "*") {
      _pathToDirectory = (std::filesystem::temp_directory_path() / "leanify_library").string();
    }
    std::filesystem::create_directories(_pathToDirectory);
    if (!std::filesystem::is_directory(_pathToDirectory))
      throw std::runtime_error("Library directory does not exist: " + _pathToDirectory);
  }

  LibraryEntry GetEntry(void* data, size_t dataSize, const char* tag) {
    auto hash = sha1(tag);
    hash.add(data, dataSize);
    char hex[SHA1_HEX_SIZE];
    hash.print_hex(hex);
    return CreateEntry(hex);
  }

  const std::string& GetStorageName() const {
    return _pathToDirectory;
  }
};

static DirectoryStorage* LibraryStorage = nullptr;

void Library::Initialize(const std::string& library) {
  LibraryStorage = new DirectoryStorage(library);
}

std::optional<LibraryEntry> Library::GetEntry(void* data, size_t dataSize, const char* tag) {
  if (!LibraryStorage)
    return std::nullopt;
  return LibraryStorage->GetEntry(data, dataSize, tag);
}

const std::string& Library::GetStorageName() {
  static std::string none = "None";
  return LibraryStorage ? LibraryStorage->GetStorageName() : none;
}