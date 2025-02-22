#pragma once

#include <bit>
#include <numeric>
#include <string>
#include <sstream>
#include <vector>

#ifdef LINUX
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "consts.h"
#include "log.h"
#include "winhdr.h"

enum FfiType {
    TYPE_INVALID = 0,
    TYPE_VOID = CONST_TYPE_VOID,
    TYPE_INTEGER = CONST_TYPE_INTEGER,
    TYPE_POINTER = CONST_TYPE_POINTER,
    TYPE_BOOL = CONST_TYPE_BOOL,
    TYPE_STRING = CONST_TYPE_STRING
};

namespace Utils {
    namespace Fs {
#ifdef LINUX
        FILE* open(std::string path, int mode);
        bool close(FILE* fp);
        std::vector<char>* read(FILE* fp, std::size_t sz);
        std::string* read_line(FILE* fp);
        std::vector<char>* read_all(FILE* fp);
        bool write(FILE* fp, std::vector<char>* data);
        bool seek(FILE* fp, int offset, int whence);
        bool eof(FILE* fp);
#endif
#ifdef WINDOWS
        HANDLE open(std::string path, int mode);
        bool close(HANDLE fp);
        std::vector<char>* read(HANDLE fp, std::size_t sz);
        std::string* read_line(HANDLE fp);
        std::vector<char>* read_all(HANDLE fp);
        bool write(HANDLE fp, std::vector<char>* data);
        bool seek(HANDLE fp, int offset, int whence);
        bool eof(HANDLE fp);
#endif
        bool delete_file(std::string path);
        bool file_exists(std::string path);
        bool dir_exists(std::string path);
        std::vector<std::string>* dir_contents(std::string path);
    }

    namespace Mem {
        void* alloc_heap(std::size_t sz);
        bool free_heap(void* ptr);
        void* alloc_pages(std::size_t sz);
        bool free_pages(void* ptr, std::size_t sz);
    }

    namespace Ffi {
        FfiType type_from_int(int val);
        void* load_library(std::string const& name);
        bool unload_library(void* handle);
        void* load_function(void* handle, std::string const& name);
    }

    std::string* run_cmd(std::string cmd, int* status);

    uint32_t ror13(std::string const& data);
    std::vector<std::string> get_lines(std::string const& data, char delim = '\n');
    std::string merge_lines(std::vector<std::string> const& lines, std::string const& delim = "\n");
    std::string format_string(const char* format...);
}