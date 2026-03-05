#include "fileio.h"

#include <iostream>
#include <fstream>

using std::cerr;
using std::endl;

std::vector<uint8_t> ReadFile(const std::filesystem::path& filepath) {
  std::error_code ec;
  auto file_size = std::filesystem::file_size(filepath, ec);
  if (ec) {
    cerr << "Error getting file size: " << filepath.string() << ": " << ec.message() << endl;
    return {};
  }
  if (file_size == 0)
    return {};

  std::ifstream fin(filepath, std::ios::binary);
  if (!fin) {
    cerr << "Error opening file: " << filepath.string() << endl;
    return {};
  }

  std::vector<uint8_t> data(static_cast<size_t>(file_size));
  fin.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!fin) {
    cerr << "Error reading file: " << filepath.string() << endl;
    return {};
  }
  return data;
}

bool WriteFile(const std::filesystem::path& filepath, const uint8_t* data, size_t size) {
  // Write to a temp file then rename for atomicity
  auto tmp = filepath;
  tmp += ".tmp";

  {
    std::ofstream fout(tmp, std::ios::binary);
    if (!fout) {
      cerr << "Error creating temp file: " << tmp.string() << endl;
      return false;
    }
    fout.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    if (!fout) {
      cerr << "Error writing temp file: " << tmp.string() << endl;
      std::filesystem::remove(tmp);
      return false;
    }
  }

  std::error_code ec;
  std::filesystem::rename(tmp, filepath, ec);
  if (ec) {
    cerr << "Error renaming file: " << ec.message() << endl;
    std::filesystem::remove(tmp);
    return false;
  }
  return true;
}

std::vector<std::filesystem::path> CollectFiles(const std::filesystem::path& path) {
  std::vector<std::filesystem::path> files;
  std::error_code ec;

  if (std::filesystem::is_directory(path, ec)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
      if (entry.is_regular_file())
        files.push_back(entry.path());
    }
    if (ec)
      cerr << "Error traversing directory: " << path.string() << ": " << ec.message() << endl;
  } else if (std::filesystem::is_regular_file(path, ec)) {
    files.push_back(path);
  } else {
    // Could be a glob or non-existent path
    cerr << "Path not found or not a regular file: " << path.string() << endl;
  }
  return files;
}
