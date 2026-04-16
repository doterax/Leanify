#include "json.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "../utils.h"

using nlohmann::json;

// ---------------------------------------------------------------------------
// Number shortening for lossy mode
// ---------------------------------------------------------------------------

// Strip redundant characters from exponent parts of a serialised JSON string:
//   - leading zeros in the exponent magnitude  (e-05  → e-5, e+007 → e7)
//   - the '+' sign on positive exponents        (e+23  → e23)
// Only processes 'e'/'E' that appear outside quoted strings (i.e. in numbers).
static std::string NormalizeExponents(std::string s) {
  bool in_string = false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (in_string) {
      if (s[i] == '\\')
        ++i;  // skip escaped character (e.g. \", \\)
      else if (s[i] == '"')
        in_string = false;
      continue;
    }
    if (s[i] == '"') {
      in_string = true;
      continue;
    }
    if (s[i] == 'e' || s[i] == 'E') {
      size_t j = i + 1;
      // Drop an explicit '+'; keep '-'.
      if (j < s.size() && s[j] == '+')
        s.erase(j, 1);  // j now points at first exponent digit
      else if (j < s.size() && s[j] == '-')
        ++j;             // skip '-', j points at first exponent digit
      // Strip leading zeros (keep at least one digit).
      size_t start = j;
      while (j < s.size() - 1 && s[j] == '0')
        ++j;
      if (j > start)
        s.erase(start, j - start);
    }
  }
  return s;
}

// Try to shorten a finite float value within the given *relative* tolerance.
// The returned node is the potentially shortened replacement.
// Returns true if the node was modified.
static bool TryShortenFloat(json& node, double tolerance) {
  assert(node.is_number_float());
  const double x = node.get<double>();

  if (!std::isfinite(x))
    return false;

  // ---- 1. Try rounding to an integer ----
  const double ri = std::round(x);
  // True relative tolerance: scale on the larger absolute value.
  // If both are zero the error is zero too, so the check trivially passes.
  const double scale_int = std::max(std::abs(x), std::abs(ri));
  const bool near_int = (scale_int == 0.0) ||
                        (std::abs(x - ri) <= tolerance * scale_int);
  if (near_int) {
    // Fits in a 64-bit integer without overflow?
    if (ri >= static_cast<double>(INT64_MIN) && ri <= static_cast<double>(INT64_MAX)) {
      node = static_cast<int64_t>(ri);
      return true;
    }
  }

  // ---- 2. Try progressively fewer significant decimal digits ----
  // %.*g uses 'prec' significant digits and automatically picks the shorter
  // of fixed or exponential notation.
  char buf[64];
  for (int prec = 1; prec <= 15; prec++) {
    std::snprintf(buf, sizeof(buf), "%.*g", prec, x);
    char* end_ptr;
    const double reparsed = std::strtod(buf, &end_ptr);
    const double sc = std::max(std::abs(x), std::abs(reparsed));
    const bool within = (sc == 0.0) ||
                        (std::abs(x - reparsed) <= tolerance * sc);
    if (within) {
      // Only replace if it actually produces a *shorter* serialisation.
      json candidate(reparsed);
      const std::string short_str = candidate.dump();
      const std::string orig_str  = node.dump();
      if (short_str.size() <= orig_str.size()) {
        node = reparsed;
        return true;
      }
      return false;  // Already at minimal length.
    }
  }

  return false;
}

// Recursively shorten all float numbers in the JSON tree.
static void ShortenNumbers(json& node, double tolerance) {
  switch (node.type()) {
    case json::value_t::number_float:
      TryShortenFloat(node, tolerance);
      break;
    case json::value_t::array:
      for (auto& el : node)
        ShortenNumbers(el, tolerance);
      break;
    case json::value_t::object:
      for (auto& [key, val] : node.items())
        ShortenNumbers(val, tolerance);
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Json class implementation
// ---------------------------------------------------------------------------

Json::Json(void* p, size_t s, int depth) : Format(p, s, depth), is_valid_(false) {
  // Parse once; store the document for later use in Leanify().
  // ignore_comments = true accepts // and /* */ style JSONC comments.
  try {
    const char* raw = reinterpret_cast<const char*>(fp_);
    doc_ = json::parse(raw, raw + size_, nullptr, /*allow_exceptions=*/true, /*ignore_comments=*/true);
    is_valid_ = true;
  } catch (const json::parse_error&) {
    // Not valid JSON — leave is_valid_ = false.
  }
}

size_t Json::Leanify(size_t size_leanified /*= 0*/) {
  if (!is_valid_) {
    memmove(fp_ - size_leanified, fp_, size_);
    fp_ -= size_leanified;
    return size_;
  }

  // Always convert exact-integer floats (e.g. 42.0 → 42) — lossless.
  // With tolerance > 0, also shorten near-integer and periodic floats.
  ShortenNumbers(doc_, json_lossy_tolerance);

  // dump() with no indentation: compact JSON, Ryu shortest-round-trip numbers.
  // NormalizeExponents strips Windows-style leading zeros (e-05 → e-5).
  const std::string output = NormalizeExponents(doc_.dump());

  fp_ -= size_leanified;

  if (output.size() < size_) {
    VerbosePrint("JSON leanified: ", size_, " -> ", output.size());
    size_ = output.size();
    std::memcpy(fp_, output.data(), size_);
  } else {
    memmove(fp_, fp_ + size_leanified, size_);
  }

  return size_;
}
