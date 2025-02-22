#include "utils.h"

#ifdef LINUX 
FILE* Utils::Fs::open(std::string path, int mode) {
    const char* fm;

    switch (mode) {
    case CONST_MODE_R:
        fm = "r";
        break;
    case CONST_MODE_W:
        fm = "w";
        break;
    case CONST_MODE_RW:
        fm = "r+";
        break;
    default:
        LOG_ERROR("invalid mode value for open: %d", mode);
        return nullptr;
    }

    FILE* fp = fopen(path.c_str(), fm);
    if (!fp) {
        LOG_DEBUG_ERRNO("failed to open file '%s'", path.c_str());
        return nullptr;
    }
    return fp;
}

bool Utils::Fs::close(FILE* fp) {
    if (!fp) {
        LOG_ERROR("got a null fp, can't close that");
        return false;
    }

    fclose(fp);
    return true;
}

std::vector<char>* Utils::Fs::read(FILE* fp, std::size_t sz) {
    if (!fp) {
        LOG_ERROR("got a null fp, can't use that");
        return nullptr;
    }

    std::vector<char>* v = new std::vector<char>;
    v->reserve(sz);

    int ret = fread(v->data(), 1, sz, fp);
    if (ret == 0) {
        int err = ferror(fp);
        if (err != 0) {
            LOG_ERROR("error reading file chunk: %d", err);
            goto bail;
        }
    }
    else if ((uint)ret < sz) {
        if (!feof(fp)) {
            LOG_WARN("didn't get enough bytes from file, got %d instead of %d without hitting eof. continuing execution.", ret, sz);
        }
    }

    return v;

bail:
    delete v;
    return nullptr;
}

std::string* Utils::Fs::read_line(FILE* fp) {
    if (!fp) {
        LOG_ERROR("got a null fp, can't use that");
        return nullptr;
    }

    const int chunk_size = 1024;
    std::string* s = new std::string();

    // read in chunks until a newline is found or fgets eofs
    char* chunk = (char*)Utils::Mem::alloc_heap(chunk_size);

    while (true) {
        const char* ret = fgets(chunk, chunk_size, fp);

        if (!ret) {
            // make sure we didn't error
            int err = ferror(fp);
            if (err != 0) {
                LOG_ERROR("failed to read line from file: %d", err);
                goto bail;
            }
            goto ret;
        }

        int len = strlen(ret);
        if (ret[len - 1] == '\n') {
            // found the end, remove trailing newlines
            char c;
            while ((c = ret[len--]) && (c == '\n' || c == '\r'));
            *s += std::string(ret, len);
            goto ret;
        }
    }

ret:
    Utils::Mem::free_heap(chunk);
    return s;

bail:
    Utils::Mem::free_heap(chunk);
    delete s;
    return nullptr;
}

std::vector<char>* Utils::Fs::read_all(FILE* fp) {
    if (!fp) {
        LOG_ERROR("got a null fp, can't use that");
        return nullptr;
    }

    const int chunk_size = 1024;

    char* buf = (char*)Utils::Mem::alloc_heap(chunk_size);
    std::vector<char>* output = new std::vector<char>;

    while (!feof(fp)) {
        int ret = fread(buf, 1, chunk_size, fp);
        if (ret == 0) {
            if ((ret = ferror(fp)) > 0) {
                LOG_ERROR("failed to read in file chunk");
                goto bail;
            }
        }
        else {
            output->reserve(output->size() + ret);
            output->insert(output->end(), buf, buf + ret);
        }
    }

    Utils::Mem::free_heap(buf);
    return output;

bail:
    Utils::Mem::free_heap(buf);
    delete output;
    return nullptr;
}

bool Utils::Fs::write(FILE* fp, std::vector<char>* data) {
    if (!fp) {
        LOG_ERROR("got a null fp, can't use that");
        return false;
    }

    std::size_t ret;
    if ((ret = fwrite(data->data(), 1, data->size(), fp)) != data->size()) {
        LOG_ERROR("didn't write enough bytes to file, only did %d instead of %d", ret, data->size());
        return false;
    }

    return true;
}

bool Utils::Fs::seek(FILE* fp, int offset, int whence) {
    if (!fp) {
        LOG_ERROR("got a null fp, can't use that");
        return false;
    }

    int w;
    switch (whence) {
    case CONST_SEEK_SET:
        w = SEEK_SET;
        break;
    case CONST_SEEK_END:
        w = SEEK_END;
        break;
    case CONST_SEEK_CUR:
        w = SEEK_CUR;
        break;
    default:
        LOG_ERROR("invalid seek whence specified: %d", whence);
        return false;
    }

    int ret = fseek(fp, offset, w);
    if (ret == -1) {
        LOG_ERROR_ERRNO("fseek operation failed");
        return false;
    }

    return true;
}

bool Utils::Fs::eof(FILE* fp) {
    if (!fp) {
        LOG_ERROR("got a null fp, can't use that");
        return false;
    }

    return feof(fp) != 0;
}
#endif // LINUX

#ifdef WINDOWS
HANDLE Utils::Fs::open(std::string path, int mode) {
    int access;
    switch (mode) {
    case CONST_MODE_R:
        access = GENERIC_READ;
        break;
    case CONST_MODE_W:
        access = GENERIC_WRITE;
        break;
    case CONST_MODE_RW:
        access = GENERIC_READ | GENERIC_WRITE;
        break;
    default:
        LOG_ERROR("invalid file mode: %d", mode);
        return nullptr;
    }

    HANDLE ret = CreateFileA(path.c_str(), access, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (ret == INVALID_HANDLE_VALUE) {
        LOG_ERROR_ERRNO("failed to create file: %s", path.c_str());
        return nullptr;
    }

    return ret;
}

bool Utils::Fs::close(HANDLE fp) {
    if (!CloseHandle(fp)) {
        LOG_ERROR_ERRNO("failed to close file");
        return false;
    }

    return true;
}

std::vector<char>* Utils::Fs::read(HANDLE fp, std::size_t sz) {
    auto data = new std::vector<char>();
    data->reserve(sz);

    DWORD num_read;
    BOOL ret = ReadFile(fp, data->data(), sz, &num_read, NULL);
    if (!ret) {
        LOG_ERROR_ERRNO("failed to read %d bytes from file", sz);
        delete data;
        return nullptr;
    }
    if (num_read != sz) {
        LOG_ERROR_ERRNO("failed to read %d bytes from file, only got %d bytes", sz, num_read);
        delete data;
        return nullptr;
    }

    return data;
}

std::string* Utils::Fs::read_line(HANDLE fp) {
    // this is absolutely garbage but i am lazy

    auto s = new std::string();
    char c;
    DWORD num_read;
    while (true) {
        if (!ReadFile(fp, &c, 1, &num_read, NULL)) {
            LOG_ERROR_ERRNO("failed to read byte from file");
            delete s;
            return nullptr;
        }

        if (c == '\n' || num_read == 0) {
            break;
        }

        s->push_back(c);
    }

    // remove a trailing carriage return
    if (s->length() > 0) {
        if (s->at(s->length() - 1) == '\r') {
            s->erase(s->length() - 1);
        }
    }

    return s;
}

std::vector<char>* Utils::Fs::read_all(HANDLE fp) {
    const int chunk_size = 1024;

    char* buf = (char*)Utils::Mem::alloc_heap(chunk_size);
    std::vector<char>* output = new std::vector<char>;

    DWORD num_read;
    while (true) {
        if (!ReadFile(fp, buf, chunk_size, &num_read, NULL)) {
            // check if the pipe was finished
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                break;
            }
            LOG_ERROR_ERRNO("failed to read in file chunk");
            goto bail;
        }
        if (num_read == 0) break;
        output->reserve(output->size() + num_read);
        output->insert(output->end(), buf, buf + num_read);
    }

    Utils::Mem::free_heap(buf);
    return output;

bail:
    Utils::Mem::free_heap(buf);
    delete output;
    return nullptr;
}

bool Utils::Fs::write(HANDLE fp, std::vector<char>* data) {
    DWORD num_written;

    if (!WriteFile(fp, data->data(), data->size(), &num_written, NULL)) {
        LOG_ERROR_ERRNO("failed to write to file");
        return false;
    }

    if (num_written != data->size()) {
        LOG_ERROR("failed to write to file, only wrote %d of %d bytes", num_written, data->size());
        return false;
    }

    return true;
}

bool Utils::Fs::seek(HANDLE fp, int offset, int whence) {
    int method;
    switch (whence) {
    case CONST_SEEK_SET:
        method = FILE_BEGIN;
        break;
    case CONST_SEEK_END:
        method = FILE_END;
        break;
    case CONST_SEEK_CUR:
        method = FILE_CURRENT;
        break;
    default:
        LOG_ERROR("invalid seek whence: %d", whence);
        return false;
    }

    if (SetFilePointer(fp, offset, NULL, method) == INVALID_SET_FILE_POINTER) {
        LOG_ERROR_ERRNO("failed to set file pointer to %d (whence: %d)", offset, whence);
        return false;
    }

    return true;
}

bool Utils::Fs::eof(HANDLE fp) {
    // get the current file offset
    // yes this says setfilepointer, thank you redmond
    DWORD off = SetFilePointer(fp, 0, NULL, FILE_CURRENT);
    if (off == INVALID_SET_FILE_POINTER) {
        LOG_ERROR_ERRNO("failed to get current file offset");
    }
    DWORD sz = GetFileSize(fp, NULL);
    if (off == INVALID_FILE_SIZE) {
        LOG_ERROR_ERRNO("failed to get file size");
    }

    return off == sz;
}
#endif // WINDOWS

bool Utils::Fs::delete_file(std::string path) {
#ifdef LINUX
    int ret = unlink(path.c_str());
    if (ret != 0) {
        LOG_ERROR_ERRNO("failed to delete file: %s", path.c_str());
        return false;
    }

    return true;
#endif
#ifdef WINDOWS
    if (!DeleteFileA(path.c_str())) {
        LOG_ERROR_ERRNO("failed to delete file: %s", path.c_str());
        return false;
    }

    return true;
#endif
    return false;
}

bool Utils::Fs::file_exists(std::string path) {
#ifdef LINUX
    struct stat st;

    int ret = stat(path.c_str(), &st);
    if (ret != 0) {
        LOG_ERROR_ERRNO("stat check on %s failed", path.c_str());
        return false;
    }

    return st.st_mode & (S_IFREG | S_IFLNK);
#endif
#ifdef WINDOWS
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#endif
    return false;
}

bool Utils::Fs::dir_exists(std::string path) {
#ifdef LINUX
    struct stat st;

    int ret = stat(path.c_str(), &st);
    if (ret != 0) {
        LOG_ERROR_ERRNO("stat check on %s failed", path.c_str());
        return false;
    }

    return st.st_mode & S_IFDIR;
#endif
#ifdef WINDOWS
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#endif
    return false;
}

std::vector<std::string>* Utils::Fs::dir_contents(std::string path) {
    auto vec = new std::vector<std::string>();

#ifdef LINUX
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        LOG_ERROR_ERRNO("failed to open dir: %s", path.c_str());
        goto err;
    }

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string s(ent->d_name);
        if (s == "." || s == "..") {
            continue;
        }
        vec->push_back(std::string(ent->d_name));
    }
    closedir(dir);
#endif
#ifdef WINDOWS
    WIN32_FIND_DATA fd;
    path += "\\*";
    HANDLE hFind = FindFirstFileA(path.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_ERROR_ERRNO("failed to open dir: %s", path.c_str());
        goto err;
    }

    do {
        std::string s(fd.cFileName);
        if (s == "." || s == "..") {
            continue;
        }
        vec->push_back(s);
    } while (FindNextFileA(hFind, &fd) != 0);

    FindClose(hFind);
#endif

    return vec;

err:
    delete vec;
    return nullptr;
}

void* Utils::Mem::alloc_heap(std::size_t sz) {
    void* ptr = nullptr;

#ifdef LINUX
    ptr = malloc(sz);
    if (!ptr) {
        LOG_ERROR_ERRNO("failed to allocate memory via malloc");
        return nullptr;
    }
#endif
#ifdef WINDOWS
    ptr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz);
    if (!ptr) {
        LOG_ERROR("failed to allocate memory via HeapAlloc");
        return nullptr;
    }
#endif

    return ptr;
}

bool Utils::Mem::free_heap(void* ptr) {
    if (!ptr) {
        LOG_ERROR("got a null ptr to free_heap, can't free that");
        return false;
    }

#ifdef LINUX
    ::free(ptr);
#endif
#ifdef WINDOWS
    if (!HeapFree(GetProcessHeap(), 0, ptr)) {
        LOG_ERROR_ERRNO("failed to free heap memory via HeapFree");
        return false;
    }
#endif

    return true;
}

void* Utils::Mem::alloc_pages(std::size_t sz) {
    void* ptr = nullptr;

    // page align the size
    sz = (sz + 0x1000) & 0xfffffffffffff000;

#ifdef LINUX
    ptr = mmap(nullptr, sz, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr == MAP_FAILED) {
        LOG_ERROR_ERRNO("failed to allocate memory via mmap");
        return nullptr;
    }
#endif
#ifdef WINDOWS
    ptr = VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!ptr) {
        LOG_ERROR_ERRNO("failed to allocate memory via VirtualAlloc");
        return nullptr;
    }
#endif

    return ptr;
}

bool Utils::Mem::free_pages(void* ptr, std::size_t sz) {
    if (!ptr) {
        LOG_ERROR("got a null ptr to free_pages, can't free that");
        return false;
    }

#ifdef LINUX
    if (munmap(ptr, sz) == -1) {
        LOG_ERROR_ERRNO("failed to free memory via munmap");
        return false;
    }
#endif
#ifdef WINDOWS
    if (!VirtualFree(ptr, 0, MEM_RELEASE)) {
        LOG_ERROR_ERRNO("failed to free memory via VirtualFree");
        return false;
    }
#endif

    return true;
}

FfiType Utils::Ffi::type_from_int(int val) {
    switch (val) {
    case CONST_TYPE_VOID:
        return TYPE_VOID;
    case CONST_TYPE_INTEGER:
        return TYPE_INTEGER;
    case CONST_TYPE_POINTER:
        return TYPE_POINTER;
    case CONST_TYPE_BOOL:
        return TYPE_BOOL;
    case CONST_TYPE_STRING:
        return TYPE_STRING;
    default:
        return TYPE_INVALID;
    }
}

void* Utils::Ffi::load_library(std::string const& name) {
    LOG_DEBUG("loading library: %s", name.c_str());

    void* handle = nullptr;

#ifdef LINUX
    handle = dlopen(name.c_str(), RTLD_LAZY);
#endif
#ifdef WINDOWS
    handle = LoadLibraryA(name.c_str());
#endif

    if (!handle) {
        LOG_ERROR_FFI("failed to load library '%s'", name.c_str());
        return nullptr;
    }

    return handle;
}

bool Utils::Ffi::unload_library(void* handle) {
    bool status = false;

#ifdef LINUX
    status = dlclose(handle) == 0;
#endif
#ifdef WINDOWS
    status = FreeLibrary((HMODULE)handle);
#endif

    if (!status) {
        LOG_ERROR_FFI("failed to unload library @ %p", handle);
        return false;
    }

    return true;
}

void* Utils::Ffi::load_function(void* handle, std::string const& name) {
    void* ptr = nullptr;

#ifdef LINUX
    ptr = dlsym(handle, name.c_str());
#endif
#ifdef WINDOWS
    ptr = GetProcAddress((HMODULE)handle, name.c_str());
#endif

    if (ptr == 0) {
        LOG_ERROR_FFI("failed to find function '%s'", name.c_str());
        return nullptr;
    }

    LOG_DEBUG("resolved function %s @ %p", name.c_str(), ptr);

    return ptr;
}

std::string* Utils::run_cmd(std::string cmd, int* status) {
#ifdef LINUX
    FILE* fp = popen(cmd.c_str(), "r");
    std::vector<char>* output_vec = Utils::Fs::read_all(fp);
    int ret = pclose(fp);
    if (!output_vec) {
        return nullptr;
    }

    std::string* output = new std::string(output_vec->begin(), output_vec->end());
    delete output_vec;

    if (status) *status = ret;
    return output;
#endif
#ifdef WINDOWS
    std::string* ret = nullptr;
    std::vector<char>* output_vec = nullptr;
    char* cmdline = nullptr;
    HANDLE hRead = nullptr, hWrite = nullptr;
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };

    // init a pipe to capture the process output
    SECURITY_ATTRIBUTES secattr;
    secattr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secattr.bInheritHandle = TRUE;
    secattr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hRead, &hWrite, &secattr, 0)) {
        LOG_ERROR_ERRNO("failed to init pipe to read cmd output");
        goto cleanup;
    }
    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
        LOG_ERROR_ERRNO("failed to disable inheritance for output read handle");
        goto cleanup;
    }

    // init structs for createprocessa
    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    // clone the cmdline to writable memory
    cmdline = new char[cmd.length() + 1];
    memcpy(cmdline, cmd.data(), cmd.length());
    cmdline[cmd.length()] = '\0';

    // spawn the process
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        LOG_ERROR_ERRNO("failed to spawn process: '%s'", cmdline);
        goto cleanup;
    }
    CloseHandle(hWrite);

    // wait for the process to finish
    if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED) {
        LOG_ERROR_ERRNO("failed to wait for process to finish");
        goto cleanup;
    }

    if (status) {
        if (!GetExitCodeProcess(pi.hProcess, (LPDWORD)status)) {
            LOG_ERROR_ERRNO("failed to get process status code");
            goto cleanup;
        }
    }

    // read in the output from the pipe
    output_vec = Utils::Fs::read_all(hRead);
    if (!output_vec) goto cleanup;
    ret = new std::string(output_vec->begin(), output_vec->end());

cleanup:
    if (output_vec) delete output_vec;
    if (cmdline) delete cmdline;
    if (hRead) CloseHandle(hRead);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);

    return ret;
#endif
    return nullptr;
}

uint32_t Utils::ror13(std::string const& data) {
    uint32_t hash = 0;
    for (unsigned int i = 0; i < data.size(); i++) {
        hash = std::rotr(hash, 13) + data[i];
    }
    return hash;
}

std::vector<std::string> Utils::get_lines(std::string const& data, char delim) {
    std::stringstream ss(data);
    std::string line;
    std::vector<std::string> lines;

    while (std::getline(ss, line, delim)) {
        lines.push_back(line);
    }

    return lines;
}

std::string Utils::merge_lines(std::vector<std::string> const& lines, std::string const& delim) {
    // mostly based on https://stackoverflow.com/a/58468879
    return std::accumulate(
        std::next(lines.begin()),
        lines.end(),
        lines[0],
        [&](const std::string& a, const std::string& b) {
            return a + delim + b;
        }
    );
}

std::string Utils::format_string(const char* format...) {
    va_list args;
    char buf[2] = { 0 };

    // get the size required
    va_start(args, format);
    int sz = vsnprintf(buf, 1, format, args) + 1;
    va_end(args);

    std::vector<char> data;
    data.resize(sz);
    va_start(args, format);
    vsnprintf(data.data(), sz, format, args);
    va_end(args);

    return std::string(data.begin(), data.end() - 1);
}