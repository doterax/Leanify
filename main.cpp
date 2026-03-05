#include "main.h"

#include <climits>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <ftw.h>
#include <unistd.h>
#endif

#include <CLI/CLI11.hpp>
#include <taskflow/taskflow.hpp>

#include "fileio.h"
#include "library.h"
#include "formats/jpeg.h"
#include "formats/png.h"
#include "formats/zip.h"
#include "leanify.h"
#include "version.h"


using std::cerr;
using std::cout;
using std::endl;
using std::string;

namespace {
void SignalHandler(int sig) {
  const char* name = "Unknown signal";
  switch (sig) {
    case SIGSEGV: name = "SIGSEGV (Segmentation fault)"; break;
    case SIGABRT: name = "SIGABRT (Abort)"; break;
    case SIGFPE:  name = "SIGFPE (Floating point exception)"; break;
    case SIGILL:  name = "SIGILL (Illegal instruction)"; break;
  }
  fprintf(stderr, "\nFatal signal caught: %s (signal %d)\n", name, sig);
  fflush(stderr);
  _exit(128 + sig);
}

void InstallSignalHandlers() {
  std::signal(SIGSEGV, SignalHandler);
  std::signal(SIGABRT, SignalHandler);
  std::signal(SIGFPE,  SignalHandler);
  std::signal(SIGILL,  SignalHandler);
}

#ifdef _WIN32
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
  DWORD code = ep->ExceptionRecord->ExceptionCode;
  void* addr = ep->ExceptionRecord->ExceptionAddress;
  fprintf(stderr, "\nUnhandled exception 0x%08lX at address %p\n", code, addr);
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
      fprintf(stderr, "ACCESS_VIOLATION: %s address %p\n",
              ep->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
              reinterpret_cast<void*>(ep->ExceptionRecord->ExceptionInformation[1]));
      break;
    case EXCEPTION_STACK_OVERFLOW:
      fprintf(stderr, "STACK_OVERFLOW\n");
      break;
  }
  fflush(stderr);
  return EXCEPTION_EXECUTE_HANDLER;
}
#endif
}  // namespace

static std::atomic<int> error_count{0};

template <typename T>
std::string ToString(const T a_value, const int n = 2) {
  std::ostringstream out;
  out.precision(n);
  out << std::fixed << a_value;
  return out.str();
}


std::string BuildSize(size_t size) {
  if (size < 1024)
    return std::to_string(size) + " B";
  else if (size < 1024 * 1024)
    return ToString(size / 1024.0) + " KB";
  else
    return ToString(size / 1024.0 / 1024.0) + " MB";
}


#ifdef _WIN32
int ProcessFile(const std::wstring& file_path) {
  char mbs[MAX_PATH] = { 0 };
  WideCharToMultiByte(CP_ACP, 0, file_path.c_str(), -1, mbs, sizeof(mbs) - 1, nullptr, nullptr);
  string filename(mbs);
#else
int ProcessFile(const std::string& file_path) {
  const std::string& filename = file_path;
#endif  // _WIN32

  try {

  if (!parallel_processing)
    cout << "Processing: " << filename << endl;

  File input_file(file_path.c_str());

  if (input_file.IsOK()) {
    size_t original_size = input_file.GetSize();

    size_t new_size(0);
    string libraryTag = ToString(iterations) + (zopflipng_lossy_transparent ? "lossy" : "lossless") +
                        (is_fast ? "no_zopflipng" : "zopflipng");

    bool reusedFromLibrary = false;
    auto filePointer = input_file.GetFilePointer();
    auto libraryEntry = Library::GetEntry(filePointer, original_size, libraryTag.c_str());
    if (!libraryEntry || !libraryEntry->isExists()) {
      new_size = LeanifyFile(filePointer, original_size, 0, filename);
      if (libraryEntry)
        libraryEntry->Save(filePointer, new_size);
      delete libraryEntry;
    } else {
      new_size = libraryEntry->Load(filePointer, original_size);
      delete libraryEntry;
      if (new_size == 0) {
        // Library load failed (race condition or corrupt cache), process normally
        new_size = LeanifyFile(filePointer, original_size, 0, filename);
      } else {
        reusedFromLibrary = true;
      }
    }

    std::string log;
    if (parallel_processing)
      log = "Processed: " + filename + "\n";

    if (original_size > 0 && new_size <= original_size) {
      log +=
          BuildSize(original_size) +
          " -> " +
          BuildSize(new_size) +
          ((!reusedFromLibrary) ? "\tLeanified: " : "\tReused: ") +
          BuildSize(original_size - new_size) +
          " (" +
          ToString(100 - 100.0 * new_size / original_size) +
          "%)";
    } else {
      log += BuildSize(original_size) + " -> " + BuildSize(new_size);
    }

    cout << log << endl;

    input_file.UnMapFile(new_size);
  }

  } catch (const std::exception& e) {
    cerr << "Error processing " << filename << ": " << e.what() << endl;
    error_count++;
  } catch (...) {
    cerr << "Unknown error processing " << filename << endl;
    error_count++;
  }

  return 0;
}



void PauseIfNotTerminal() {
// pause if Leanify is not started in terminal
// so that user can see the output instead of just a flash of a black box
#ifdef _WIN32
  if (is_pause)
    system("pause");
#endif  // _WIN32
}

tf::Taskflow taskflow;

#ifdef _WIN32
int EnqueueProcessFileTask(const wchar_t* file_path) {
  std::wstring filePath(file_path);
#else
// written like this in order to be callback function of ftw()
int EnqueueProcessFileTask(const char* file_path, const struct stat* sb = nullptr, int typeflag = FTW_F) {
  if (typeflag != FTW_F)
    return 0;
  std::string filePath(file_path);
#endif  // _WIN32

  taskflow.emplace([filePath = std::move(filePath)]() {
    ProcessFile(filePath);
  });
  return 0;
}


#ifdef _WIN32
int main() {
  int argc;
  wchar_t* command_line = GetCommandLineW();
  wchar_t** wargv = CommandLineToArgvW(command_line, &argc);

  // Convert wide argv to UTF-8 for CLI11
  std::vector<std::string> arg_strings(argc);
  std::vector<char*> argv_ptrs(argc);
  for (int a = 0; a < argc; a++) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wargv[a], -1, nullptr, 0, nullptr, nullptr);
    arg_strings[a].resize(len - 1);
    WideCharToMultiByte(CP_UTF8, 0, wargv[a], -1, &arg_strings[a][0], len, nullptr, nullptr);
    argv_ptrs[a] = &arg_strings[a][0];
  }
  LocalFree(wargv);
  char** argv = argv_ptrs.data();
#else
int main(int argc, char** argv) {
#endif  // _WIN32

  InstallSignalHandlers();
#ifdef _WIN32
  SetUnhandledExceptionFilter(CrashHandler);
#endif

  is_fast = false;
  is_verbose = false;
  iterations = 15;
  max_depth = INT_MAX;
  zopflipng_lossy_transparent = true;

  std::string library_path;
  bool quiet = false;
  bool keep_exif = false;
  bool keep_icc = false;
  bool jpeg_keep_all = false;
  bool jpeg_arithmetic = false;
  bool png_lossless_transparent = false;
  bool zip_deflate = false;
  std::vector<std::string> paths;

#ifdef _WIN32
  is_pause = !getenv("PROMPT");
#endif  // _WIN32

  CLI::App app{"Leanify " VERSION_STR "\nFork: https://github.com/doterax/Leanify\nBuilt with " COMPILER_STR " (" ARCH_STR ")"};
  app.get_formatter()->column_width(40);

  app.add_option("-i,--iteration", iterations,
                 "More iterations may produce better result, but\n"
                 "  use more time, default is 15.")
      ->check(CLI::PositiveNumber);

  app.add_option("-d,--max_depth", max_depth,
                 "Maximum recursive depth, unlimited by default.\n"
                 "  Set to 1 will disable recursive minifying.")
      ->check(CLI::PositiveNumber);

  app.add_flag("-f,--fastmode", is_fast, "Fast mode, no recompression.");
  app.add_flag("-q,--quiet", quiet, "No output to stdout.");
  app.add_flag("-v,--verbose", is_verbose, "Verbose output.");
  app.add_flag("-p,--parallel", parallel_processing, "Distribute all tasks to all CPUs.");

  app.add_option("-l,--library", library_path,
                 "Use library to store and reuse already compressed files.\n"
                 "  Set * will automatically use temporary folder.");

  app.add_flag("--keep-exif", keep_exif, "Do not remove Exif.");
  app.add_flag("--keep-icc", keep_icc, "Do not remove ICC profile.");

  // JPEG options
  app.add_flag("--jpeg-keep-all", jpeg_keep_all,
               "Do not remove any metadata or comments in JPEG.");
  app.add_flag("--jpeg-arithmetic", jpeg_arithmetic,
               "Use arithmetic coding for JPEG.");

  // PNG options
  app.add_flag("--png-lossless-transparent", png_lossless_transparent,
               "Prohibit altering hidden colors of fully transparent pixels.");

  // ZIP options
  app.add_flag("--zip-deflate", zip_deflate,
               "Try deflate even if not compressed originally.");

  app.add_option("paths", paths, "File or directory paths to process")
      ->required();

  CLI11_PARSE(app, argc, argv);

#ifdef _WIN32
  // Any options given -> do not pause
  if (argc > 1)
    is_pause = false;
#endif  // _WIN32

  // Apply parsed flags to statics
  if (quiet) {
    cout.setstate(std::ios::failbit);
    is_verbose = false;
  }

  if (parallel_processing && is_verbose) {
    cerr << "Verbose logs not supported in parallel mode." << endl;
    return 1;
  }

  Jpeg::keep_exif_ = keep_exif;
  Jpeg::keep_icc_profile_ = keep_icc;
  Png::keep_icc_profile_ = keep_icc;
  Jpeg::keep_all_metadata_ = jpeg_keep_all;
  Jpeg::force_arithmetic_coding_ = jpeg_arithmetic;
  zopflipng_lossy_transparent = !png_lossless_transparent;
  Zip::force_deflate_ = zip_deflate;

  cout << std::fixed;
  cout.precision(2);

  if (!library_path.empty()) {
    try {
#ifdef _WIN32
      // Convert UTF-8 library path back to wide string for Windows API
      int wlen = MultiByteToWideChar(CP_UTF8, 0, library_path.c_str(), -1, nullptr, 0);
      std::wstring wlibrary(wlen, 0);
      MultiByteToWideChar(CP_UTF8, 0, library_path.c_str(), -1, &wlibrary[0], wlen);
      Library::Initialize(wlibrary);
#else
      Library::Initialize(library_path);
#endif
      cout << "Library storage: " << Library::GetStorageName() << endl << endl;
    } catch (const std::runtime_error& e) {
      cerr << "Error: " << e.what() << endl << endl;
      return 1;
    }
  }

  // Process all input paths
  for (const auto& path : paths) {
#ifdef _WIN32
    // Convert UTF-8 path to wide string for Windows TraversePath
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
    TraversePath(wpath.c_str(), EnqueueProcessFileTask);
#else
    TraversePath(path.c_str(), EnqueueProcessFileTask);
#endif
  }

  size_t parallel_tasks = 1;
  if (parallel_processing)
    parallel_tasks = std::thread::hardware_concurrency();

  tf::Executor executor(parallel_tasks);
  executor.run(taskflow).wait();

  int errors = error_count.load();
  if (errors > 0)
    cout << "Finished with " << errors << " error(s)." << endl;
  else
    cout << "Finished." << endl;

  PauseIfNotTerminal();

  return errors > 0 ? 1 : 0;
}
