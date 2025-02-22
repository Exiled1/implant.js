// memory permission constants
const MEM_RW = 0x1;
const MEM_RWX = 0x2;

// file modes
const MODE_R = 0x1;
const MODE_W = 0x2;
const MODE_RW = 0x4;

// seek params
const SEEK_SET = 0x1;
const SEEK_END = 0x2;
const SEEK_CUR = 0x3;

// ffi types
const TYPE_VOID = 0x1;
const TYPE_INTEGER = 0x2;
const TYPE_POINTER = 0x3;
const TYPE_BOOL = 0x4;
const TYPE_STRING = 0x5;

// os
const OS_LINUX = 0xc1;
const OS_WINDOWS = 0xc2;

// type aliases
type handle = number;
type pointer = BigInt;

let ctx = {
    /**
     *  Write to the module output buffer
     */
    output: function (msg: string): void { },

    /**
     *  Execute a shell command. Returns the combined stdout+stderr of the command.
     *  
     *  Raises an exception if command has a non-zero return status and ignore_status is false.
     */
    system: function (cmd: string, ignore_status: boolean = false): string { },

    /**
     *  Identify the current operating system
     */
    os: function (): number { },

    /**
     *  Virtual memory-related operations
     */
    mem = {
        /**
         *  Allocate a region of memory
         */
        alloc: function (size: number, perms: number): pointer { },

        /**
         *  Free a region of memory
         */
        free: function (ptr: pointer): void { },

        /**
         *  Read from arbitrary memory
         */
        read: function (ptr: pointer, size: number): Uint8Array { },

        /**
         *  Write an unsigned DWORD from an arbitrary location
         */
        read_dword: function (ptr: pointer): number { },

        /**
         *  Read an **signed** QWORD from an arbitrary location. Use BigInt.asUintN(64, val).
         */
        read_qword: function (ptr: pointer): BigInt { },

        /**
         *  Write to arbitrary memory
         */
        write: function (ptr: pointer, data: Uint8Array): boolean { },

        /**
         *  Write an unsigned DWORD to an arbitrary location
         */
        write_dword: function (ptr: pointer, data: number): boolean { },

        /**
         *  Write an unsigned QWORD to an arbitrary location
         */
        write_qword: function (ptr: pointer, data: BigInt): boolean { },

        /**
         *  Copy memory from src to dst
         */
        copy: function (dst: pointer, src: pointer, size: number): boolean { },

        /**
         *  Compare two arbitrary regions of memory. Returns true if they are equivalent.
         *  If a Uint8Array is passed to data1 or data2, size is optional and the smaller
         *  length will be used. size can still be specified to override the array length
         *  if only part of the array should be used in a comparison.
         */
        equal: function (data1: pointer | Uint8Array, data2: pointer | Uint8Array, size?: number): boolean { },
    },

    /**
     *  Filesystem-related operations
     */
    fs = {
        /**
         *  Open a file. Raises an exception if the file couldn't be opened in the requested mode.
         */
        open: function (path: string, mode: number): handle { },

        /**
         *  Close the file
         */
        close: function (handle: handle): void { },

        /**
         *  Read data from the file
         */
        read: function (handle: handle, size: number): Uint8Array { },

        /**
         *  Read in a single line from the file, and strip trailing newlines/carriage returns.
         *  If at the end of the file, returns null
         */
        read_line: function (handle: handle): string | null { },

        /**
         *  Read in the entire (remaining) file
         */
        read_all: function (handle: handle): Uint8Array { },

        /**
         *  Write data to the file
         */
        write: function (handle: handle, data: string | Uint8Array): void { },

        /**
         *  Seek into the file
         */
        seek: function (handle: handle, offset: number, whence: number): void { },

        /**
         *  Check if at the end of the file
         */
        eof: function (handle: handle): boolean { },

        /**
         *  Delete a file. Raises an exception if the file couldn't be deleted
         */
        delete_file: function (path: string): void { },

        /**
         *  Checks if the path exists and is a file
         */
        file_exists: function (path: string): boolean { },

        /**
         *  Checks if the path exists and is a directory
         */
        dir_exists: function (path: string): boolean { },

        /**
         *  Enumerate the entries of a directory. Returns a list of file/directory names.
         */
        dir_contents: function (path: string): string[] { },
    },

    /**
     *  Function-related operations.
     */
    ffi = {
        /**
         *  Resolve a function with a given name from the specified library
         */
        resolve: function (library: string, symbol: string, return_type: number, arg_types?: number[]): function | undefined { },

        /**
         *  Define a function by pointer. Defining the same function multiple times is wasteful, pls dont))
         */
        define: function (addr: pointer, return_type: number, arg_types?: number[]): function | undefined { },
    }
};
declare ctx;