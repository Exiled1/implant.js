#pragma once

#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "consts.h"
#include "net.h"
#include "utils.h"
#include "winhdr.h"

enum AllocType {
    // rwx memory allocated via mmap/virtualalloc
    PAGE,

    // rw memory allocated via malloc/heapalloc
    HEAP
};

struct MemAlloc {
    void *ptr;
    std::size_t sz;
    AllocType type;
};

struct ForeignFunc {
    void *ptr;
    FfiType return_type;
    std::string library;
    std::string symbol;
    std::vector<FfiType> arg_types;

    std::string name() {
        std::stringstream ss;

        if (this->library.length() > 0 && this->symbol.length() > 0) {
            ss << this->library << "!" << this->symbol;
        } else {
            ss << "dyn@" << std::hex << this->ptr;
        }
        
        return ss.str();
    }
};

typedef uint32_t jshandle_t;
#ifndef INVALID_HANDLE
#define INVALID_HANDLE 0xffffffff
#endif

class State {
private:
    State(Net *net);
    Net *_net;

    std::map<void *, MemAlloc*> allocd_mem;
#ifdef LINUX
    std::map<jshandle_t, FILE*> file_handles;
#endif
#ifdef WINDOWS
    std::map<jshandle_t, HANDLE> file_handles;
#endif
    std::map<std::string, void *> library_handles;
    std::map<jshandle_t, ForeignFunc*> function_handles;

    std::string output;
    void (*output_cb)(const std::string&) = nullptr;
    bool errord;

    std::mt19937 rnd_gen;
    std::uniform_int_distribution<> rnd_dist;
    
    bool mem_free(MemAlloc *ma);

    int rand();

    jshandle_t generate_handle();
    jshandle_t generate_handle(std::string const& data);
public:
    ~State();

    static void Initialize(Net *net);

    Net *net();

    void add_output(std::string msg);
    void set_output_callback(void (*cb)(const std::string&));
    void set_errord();

    std::string get_output();
    bool get_errord();

    void* mem_alloc(std::size_t sz, uint8_t perms);
    bool mem_free(void *ptr);

    jshandle_t open_file(std::string path, int mode);
    bool close_file(jshandle_t handle);
    std::vector<char>* read_file(jshandle_t handle, std::size_t sz);
    std::string* read_line(jshandle_t handle);
    std::vector<char>* read_all(jshandle_t handle);
    bool write_file(jshandle_t handle, std::vector<char>* data);
    bool seek_file(jshandle_t handle, int offset, int whence);
    bool eof(jshandle_t handle);

    jshandle_t resolve_function(std::string const& library, std::string const& symbol, FfiType return_type, const std::vector<FfiType> *arg_types);
    jshandle_t define_function(void *ptr, FfiType return_type, const std::vector<FfiType> *arg_types);
    ForeignFunc *get_function(jshandle_t handle);
};

State* __get_state();
#define STATE (__get_state())
#define NET (STATE->net())