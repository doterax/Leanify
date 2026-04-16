#ifndef FORMATS_JSON_H_
#define FORMATS_JSON_H_

// Suppress warnings from the nlohmann header that we cannot change.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <nlohmann/json.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "format.h"

// Global tolerance for lossy JSON number optimization.
// 0.0  = lossless (only comment stripping + whitespace removal + shortest round-trip numbers)
// >0.0 = lossy: shorten numbers within this relative tolerance
extern double json_lossy_tolerance;

class Json : public Format {
 public:
  Json(void* p, size_t s, int depth = 1);

  bool IsValid() const { return is_valid_; }

  size_t Leanify(size_t size_leanified = 0) override;

 private:
  bool is_valid_;
  nlohmann::json doc_;
};

#endif  // FORMATS_JSON_H_
