#include "fileio.h"

#include <cstdio>
#include <iostream>

#include <shlwapi.h>

using std::cerr;
using std::endl;

namespace {

void PrintErrorMessage(const char* msg) {
  char* error_msg = nullptr;
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, GetLastError(), 0,
                 reinterpret_cast<char*>(&error_msg), 0, nullptr);
  cerr << msg << endl;
  if (error_msg) {
    cerr << error_msg << endl;
    LocalFree(error_msg);
  }
}

bool IsDirectory(const wchar_t* path) {
  DWORD fa = GetFileAttributes(path);
  if (fa == INVALID_FILE_ATTRIBUTES)
    return false;
  return (fa & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

}  // namespace

// Traverse directory or glob pattern and call Callback() for each file.
void TraversePath(const wchar_t* dir, int callback(const wchar_t* file_path)) {
  bool is_dir = IsDirectory(dir);

  WIN32_FIND_DATA FindFileData;
  wchar_t DirSpec[MAX_PATH];
  lstrcpy(DirSpec, dir);
  if (is_dir)
    lstrcat(DirSpec, L"\\*");
  HANDLE hFind = FindFirstFile(DirSpec, &FindFileData);

  if (hFind == INVALID_HANDLE_VALUE) {
    PrintErrorMessage("FindFirstFile error!");
    return;
  }

  do {
    if (FindFileData.cFileName[0] == '.') {
      if (FindFileData.cFileName[1] == 0 ||                // "."
          lstrcmp(FindFileData.cFileName + 1, L".") == 0)  // ".."
        continue;
    }

    wchar_t DirAdd[MAX_PATH];
    lstrcpy(DirAdd, dir);
    if (!is_dir) {
      PathRemoveFileSpec(DirAdd);
    }

    if (DirAdd[0] != 0)
      lstrcat(DirAdd, L"\\");
    lstrcat(DirAdd, FindFileData.cFileName);

    if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // directory
      TraversePath(DirAdd, callback);
    } else {
      // file
      callback(DirAdd);
    }
  } while (FindNextFile(hFind, &FindFileData) != 0);

  FindClose(hFind);
}

File::File(const wchar_t* filepath) {
  hFile_ = CreateFile(filepath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (hFile_ == INVALID_HANDLE_VALUE) {
    PrintErrorMessage("Open file error!");
    return;
  }
  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(hFile_, &file_size)) {
    PrintErrorMessage("GetFileSize error!");
    CloseHandle(hFile_);
    hFile_ = INVALID_HANDLE_VALUE;
    return;
  }
  size_ = static_cast<size_t>(file_size.QuadPart);
  hMap_ = CreateFileMapping(hFile_, nullptr, PAGE_READWRITE, 0, 0, nullptr);
  if (hMap_ == nullptr) {
    PrintErrorMessage("Map file error!");
    CloseHandle(hFile_);
    hFile_ = INVALID_HANDLE_VALUE;
    return;
  }
  fp_ = MapViewOfFile(hMap_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (fp_ == nullptr) {
    PrintErrorMessage("MapViewOfFile error!");
    CloseHandle(hMap_);
    hMap_ = nullptr;
    CloseHandle(hFile_);
    hFile_ = INVALID_HANDLE_VALUE;
  }
}

void File::UnMapFile(size_t new_size) {
  if (!FlushViewOfFile(fp_, 0))
    PrintErrorMessage("Write file error!");

  if (!UnmapViewOfFile(fp_))
    PrintErrorMessage("UnmapViewOfFile error!");

  CloseHandle(hMap_);
  hMap_ = nullptr;

  if (new_size) {
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(new_size);
    if (!SetFilePointerEx(hFile_, li, nullptr, FILE_BEGIN))
      PrintErrorMessage("SetFilePointer error!");
    else if (!SetEndOfFile(hFile_))
      PrintErrorMessage("SetEndOfFile error!");
  }
  CloseHandle(hFile_);
  hFile_ = INVALID_HANDLE_VALUE;
  fp_ = nullptr;
}
