#include "state.h"

static State* __current_state = nullptr;

State* __get_state() {
    return __current_state;
}

State::State(Net* net) {
    this->_net = net;
    this->errord = false;

    std::random_device rd;
    this->rnd_gen = std::mt19937(rd());
    this->rnd_dist = std::uniform_int_distribution(1, INT32_MAX);
}

State::~State() {
    std::size_t sz = this->allocd_mem.size();
    if (sz > 0) {
        LOG_DEBUG("cleaning up %d leftover mem allocations", sz);
        for (auto it = this->allocd_mem.begin(); it != this->allocd_mem.end(); it++) {
            this->mem_free(it->second);
        }
    }

    sz = this->file_handles.size();
    if (sz > 0) {
        LOG_DEBUG("cleaning up %d leftover file handles", sz);
        for (auto it = this->file_handles.begin(); it != this->file_handles.end(); it++) {
            Utils::Fs::close(it->second);
        }
    }

    sz = this->library_handles.size();
    if (sz > 0) {
        LOG_DEBUG("cleaning up %d leftover library handles", sz);
        for (auto it = this->library_handles.begin(); it != this->library_handles.end(); it++) {
            Utils::Ffi::unload_library(it->second);
        }
    }

    sz = this->function_handles.size();
    if (sz > 0) {
        LOG_DEBUG("cleaning up %d function handles", sz);
        for (auto it = this->function_handles.begin(); it != this->function_handles.end(); it++) {
            delete it->second;
        }
    }
}

void State::Initialize(Net* net) {
    if (__current_state) delete __current_state;
    __current_state = new State(net);
}

Net* State::net() {
    return this->_net;
}

int State::rand() {
    return this->rnd_dist(this->rnd_gen);
}

jshandle_t State::generate_handle() {
    int base = this->rand() % 0xfffff;

    return (0xabc << 20) | base;
}

jshandle_t State::generate_handle(std::string const& data) {
    // this *probably* won't collide, yolo
    uint32_t hash = Utils::ror13(data);
    return (0xa1 << 24) | (hash & 0xffffff);
}

void State::add_output(std::string msg) {
    if (msg.size() == 0) return;

    if (msg.at(msg.size() - 1) != '\n') {
        msg.append("\n");
    }

    this->output.append(msg);
    if (this->output_cb) this->output_cb(msg);
}

void State::set_output_callback(void (*cb)(const std::string&)) {
    this->output_cb = cb;
}

void State::set_errord() {
    this->errord = true;
}

std::string State::get_output() {
    return this->output;
}

bool State::get_errord() {
    return this->errord;
}

void* State::mem_alloc(std::size_t sz, uint8_t perms) {
    auto ma = new MemAlloc();
    ma->sz = sz;

    switch (perms) {
    case CONST_MEM_RW:
        ma->type = HEAP;
        ma->ptr = Utils::Mem::alloc_heap(sz);
        if (!ma->ptr) {
            goto err;
        }
        LOG_DEBUG("allocated heap memory @ %p", ma->ptr);
        break;
    case CONST_MEM_RWX:
        ma->type = PAGE;
        ma->ptr = Utils::Mem::alloc_pages(sz);
        if (!ma->ptr) {
            goto err;
        }
        LOG_DEBUG("allocated page memory @ %p", ma->ptr);
        break;
    default:
        LOG_ERROR("invalid memory permission param provided: 0x%x", perms);
        return nullptr;
    }

    this->allocd_mem.emplace(ma->ptr, ma);

    return ma->ptr;

err:
    delete ma;
    return nullptr;
}

bool State::mem_free(MemAlloc* ma) {
    if (!ma || !ma->ptr) {
        LOG_ERROR("invalid allocation provided: %p", ma);
        return false;
    }

    switch (ma->type) {
    case HEAP:
        LOG_DEBUG("freeing heap memory @ %p", ma->ptr);
        return Utils::Mem::free_heap(ma->ptr);
    case PAGE:
        LOG_DEBUG("freeing page memory @ %p", ma->ptr);
        return Utils::Mem::free_pages(ma->ptr, ma->sz);
    }

    LOG_ERROR("reached unreachable code section in State::mem_free, this is a client bug");
    return false;
}

bool State::mem_free(void* ptr) {
    auto ma = this->allocd_mem.at(ptr);

    bool ret = this->mem_free(ma);

    this->allocd_mem.erase(ptr);
    delete ma;
    return ret;
}

jshandle_t State::open_file(std::string path, int mode) {
    jshandle_t h = this->generate_handle();

    auto fp = Utils::Fs::open(path, mode);
    if (!fp) {
        return INVALID_HANDLE;
    }
    this->file_handles.emplace(h, fp);

    return h;
}

bool State::close_file(jshandle_t handle) {
    if (!this->file_handles.contains(handle)) {
        LOG_ERROR("handle doesnt exist: %p", handle);
        return false;
    }

    auto f = this->file_handles.at(handle);
    this->file_handles.erase(handle);

    return Utils::Fs::close(f);
}

std::vector<char>* State::read_file(jshandle_t handle, std::size_t sz) {
    if (!this->file_handles.contains(handle)) {
        LOG_ERROR("handle doesnt exist: %p", handle);
        return nullptr;
    }

    return Utils::Fs::read(this->file_handles.at(handle), sz);
}

std::string* State::read_line(jshandle_t handle) {
    if (!this->file_handles.contains(handle)) {
        LOG_ERROR("handle doesnt exist: %p", handle);
        return nullptr;
    }

    return Utils::Fs::read_line(this->file_handles.at(handle));
}

std::vector<char>* State::read_all(jshandle_t handle) {
    if (!this->file_handles.contains(handle)) {
        LOG_ERROR("handle doesnt exist: %p", handle);
        return nullptr;
    }

    return Utils::Fs::read_all(this->file_handles.at(handle));
}

bool State::write_file(jshandle_t handle, std::vector<char>* data) {
    if (!this->file_handles.contains(handle)) {
        LOG_ERROR("handle doesnt exist: %p", handle);
        return false;
    }

    return Utils::Fs::write(this->file_handles.at(handle), data);
}

bool State::seek_file(jshandle_t handle, int offset, int whence) {
    if (!this->file_handles.contains(handle)) {
        LOG_ERROR("handle doesnt exist: %p", handle);
        return false;
    }

    return Utils::Fs::seek(this->file_handles.at(handle), offset, whence);
}

bool State::eof(jshandle_t handle) {
    if (!this->file_handles.contains(handle)) {
        LOG_ERROR("handle doesnt exist: %p", handle);
        return false;
    }

    return Utils::Fs::eof(this->file_handles.at(handle));
}

jshandle_t State::resolve_function(std::string const& library, std::string const& symbol, FfiType return_type, const std::vector<FfiType>* arg_types) {
    // see if the symbol has already been resolved
    jshandle_t h = this->generate_handle(library + "!" + symbol);
    if (this->function_handles.contains(h)) {
        return h;
    }

    // need to resolve the function
    // first, make sure the library is loaded
    if (!this->library_handles.contains(library)) {
        void* lib_ptr = Utils::Ffi::load_library(library);
        if (!lib_ptr) {
            return INVALID_HANDLE;
        }

        this->library_handles.emplace(library, lib_ptr);
    }

    // now fetch the function ptr
    void* func_ptr = Utils::Ffi::load_function(this->library_handles.at(library), symbol);
    if (!func_ptr) {
        return INVALID_HANDLE;
    }

    ForeignFunc* ff = new ForeignFunc;
    ff->ptr = func_ptr;
    ff->library = library;
    ff->symbol = symbol;
    ff->return_type = return_type;
    if (arg_types && arg_types->size() > 0) {
        ff->arg_types.reserve(arg_types->size());
        ff->arg_types.insert(ff->arg_types.end(), arg_types->begin(), arg_types->end());
    }
    this->function_handles.emplace(h, ff);

    return h;
}

jshandle_t State::define_function(void* ptr, FfiType return_type, const std::vector<FfiType>* arg_types) {
    jshandle_t h = this->generate_handle();

    ForeignFunc* ff = new ForeignFunc;
    ff->ptr = ptr;
    ff->return_type = return_type;
    if (arg_types && arg_types->size() > 0) {
        // make sure there isnt a VOID type being specified
        for (auto it = arg_types->begin(); it != arg_types->end(); it++) {
            if (*it == TYPE_VOID) {
                LOG_ERROR("a TYPE_VOID argument type was specified, this is not allowed. for functions with no arguments, omit specifying arg types entirely");
                goto bail;
            }
        }
        ff->arg_types.reserve(arg_types->size());
        ff->arg_types.insert(ff->arg_types.end(), arg_types->begin(), arg_types->end());
    }
    this->function_handles.emplace(h, ff);

    return h;

bail:
    delete ff;
    return INVALID_HANDLE;
}

ForeignFunc* State::get_function(jshandle_t handle) {
    if (!this->function_handles.contains(handle)) {
        LOG_ERROR("function for handle %p not found", handle);
        return nullptr;
    }

    return this->function_handles.at(handle);
}