#ifndef LEANIFY_H_
#define LEANIFY_H_

#include <cstddef>
#include <cstdint>
#include <string>

extern int max_depth;

size_t LeanifyFile(void* file_pointer, size_t file_size, size_t size_leanified = 0,
                   const std::string& filename = "", int depth = 1);

size_t ZlibRecompress(uint8_t* src, size_t src_len, size_t size_leanified = 0);

#endif  // LEANIFY_H_
