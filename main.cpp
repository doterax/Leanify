#include "main.h"

#include <climits>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#ifndef _WIN32
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


int ProcessFile(const std::filesystem::path& file_path) {
  std::string filename = file_path.string();

  try {

  if (!parallel_processing)
    cout << "Processing: " << filename << endl;

  auto data_opt = ReadFile(file_path);
  if (!data_opt || data_opt->empty())
    return 0;

  auto& data = *data_opt;
  size_t original_size = data.size();

  size_t new_size(0);
  string libraryTag = ToString(iterations) + (zopflipng_lossy_transparent ? "lossy" : "lossless") +
                      (is_fast ? "no_zopflipng" : "zopflipng");

  bool reusedFromLibrary = false;
  auto libraryEntry = Library::GetEntry(data.data(), original_size, libraryTag.c_str());
  if (!libraryEntry || !libraryEntry->isExists()) {
    new_size = LeanifyFile(data.data(), original_size, 0, filename);
    if (libraryEntry)
      libraryEntry->Save(data.data(), new_size);
  } else {
    new_size = libraryEntry->Load(data.data(), original_size);
    if (new_size == 0) {
      // Library load failed (race condition or corrupt cache), process normally
      new_size = LeanifyFile(data.data(), original_size, 0, filename);
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

  if (new_size < original_size) {
    if (!WriteFile(file_path, data.data(), new_size))
      cerr << "Error writing file: " << filename << endl;
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


int main(int argc, char** argv) {
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
      Library::Initialize(library_path);
      cout << "Library storage: " << Library::GetStorageName() << endl << endl;
    } catch (const std::runtime_error& e) {
      cerr << "Error: " << e.what() << endl << endl;
      return 1;
    }
  }

  // Phase 1: Collect all file paths
  std::vector<std::filesystem::path> work_items;
  for (const auto& path : paths) {
    auto files = CollectFiles(std::filesystem::path(path));
    work_items.insert(work_items.end(),
                      std::make_move_iterator(files.begin()),
                      std::make_move_iterator(files.end()));
  }

  // Phase 2: Create tasks and execute
  tf::Taskflow taskflow;
  for (auto& file_path : work_items) {
    taskflow.emplace([fp = std::move(file_path)]() {
      ProcessFile(fp);
    });
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
