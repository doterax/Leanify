#ifndef FILEIO_H_
#define FILEIO_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

// Read entire file into a vector. Returns empty vector on error.
std::vector<uint8_t> ReadFile(const std::filesystem::path& filepath);

// Write data to file atomically (temp file + rename). Returns true on success.
bool WriteFile(const std::filesystem::path& filepath, const uint8_t* data, size_t size);

// Collect all regular files under a path (recursively for directories).
std::vector<std::filesystem::path> CollectFiles(const std::filesystem::path& path);

#endif  // FILEIO_H_
