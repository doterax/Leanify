#ifndef VERSION_H_
#define VERSION_H_

#define VERSION_MAJOR 0
#define VERSION_MINOR 4
#define VERSION_BUILD 4

#define _STR(x) #x
#define STR(x) _STR(x)
#define VERSION_STR STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_BUILD)
#define VERSION_NUM VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD

// Compiler identification
#if defined(__clang__)
  #define COMPILER_STR "Clang " STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__)
#elif defined(__GNUC__)
  #define COMPILER_STR "GCC " STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
  #define COMPILER_STR "MSVC " STR(_MSC_VER)
#else
  #define COMPILER_STR "Unknown compiler"
#endif

#if defined(__LP64__) || defined(_WIN64)
  #define ARCH_STR "64-bit"
#else
  #define ARCH_STR "32-bit"
#endif

#endif  // VERSION_H_
