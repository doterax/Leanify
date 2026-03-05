#include "library.h"

#include <string>

#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <SHA1/sha1.hpp>
#include "filesystem.h"

class Storage {
 public:
  virtual ~Storage() = default;
  virtual LibraryEntry* GetEntry(void* data, size_t dataSize, const char* tag) = 0;
  virtual const std::string& GetStorageName() = 0;
};

static Storage* LibraryStorage = nullptr;

class FileEntry : public LibraryEntry {
 private:
  std::string _path;

 public:
  FileEntry(const std::string& path) : _path(path) {}

 public:
  bool isExists() override {
    return std::filesystem::exists(_path);
  }
  void Save(void* data, size_t dataSize) override {
    std::ofstream fout(_path, std::ios_base::binary);
    fout.write(static_cast<const char*>(data), dataSize);
    fout.close();
  }
  size_t Load(void* data, size_t dataSize) override {
    auto size = std::filesystem::file_size(_path);
    std::ifstream fin(_path, std::ios_base::binary);
    fin.read(static_cast<char*>(data), size);
    fin.close();
    return size;
  }
};

class DirectoryStorage : public Storage {
 private:
  std::string _pathToDirectory;

 protected:
  LibraryEntry* CreateEntry(const std::string& hash) {
    auto firstTwo = std::string(hash, 0, 2);
    auto directory = std::filesystem::path(_pathToDirectory) / firstTwo;
    std::filesystem::create_directories(directory);
    auto fullPath = directory / hash;
    return new FileEntry(fullPath.string());
  }

 public:
  DirectoryStorage(const std::string& pathToDirectory) : _pathToDirectory(pathToDirectory) {
    if (_pathToDirectory == "*") {
      _pathToDirectory = (std::filesystem::temp_directory_path() / "leanify_library").string();
    }
    std::filesystem::create_directories(_pathToDirectory);
    if (!std::filesystem::is_directory(_pathToDirectory))
      throw std::runtime_error("Library directory does not exist: " + _pathToDirectory);
  }

  LibraryEntry* GetEntry(void* data, size_t dataSize, const char* tag) {
    auto hash = sha1(tag);
    hash.add(data, dataSize);
    char hex[SHA1_HEX_SIZE];
    hash.print_hex(hex);
    return CreateEntry(hex);
  }
  const std::string& GetStorageName() {
    return _pathToDirectory;
  }
};

#ifdef _WIN32
void Library::Initialize(const std::wstring& library) {
  char mbs[MAX_PATH] = { 0 };
  WideCharToMultiByte(CP_ACP, 0, library.c_str(), -1, mbs, sizeof(mbs) - 1, nullptr, nullptr);
  LibraryStorage = new DirectoryStorage(mbs);
}
#else
void Library::Initialize(const std::string& library) {
  LibraryStorage = new DirectoryStorage(library);
}
#endif
LibraryEntry* Library::GetEntry(void* data, size_t dataSize, const char* tag) {
  return LibraryStorage ? LibraryStorage->GetEntry(data, dataSize, tag) : nullptr;
}

const std::string& Library::GetStorageName() {
  static std::string none = "None";
  return LibraryStorage ? LibraryStorage->GetStorageName() : none;
}