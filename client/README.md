# implant.js - Client

## DISCLAIMER

- I am not a C++ engineer
- I do not like C++
- Therefore, this C++ code is garbage

_However,_ it does technically work (the best kind), and should hopefully serve as a good reference for embedding V8 in your own projects

## Setting up V8

First, download v8 using [these instructions](https://v8.dev/docs/source-code#using-git).

Checkout version 12.9, then follow the instructions to setup a V8 target compatible with this project:

```
$ cd /path/to/v8/root
$ git checkout branch-heads/12.9 -b implantjs -t
$ python3 tools/dev/v8gen.py x64.release.sample
$ gn args out.gn/x64.release.sample
    # see below for this file
$ ninja -C out.gn/x64.release.sample v8_monolith
    # go get some coffee or read a book
```

The build arguments for the `gn args` command on **Linux** should be set to:
```
dcheck_always_on = false
is_component_build = false
is_debug = true
target_cpu = "x64"
use_custom_libcxx = false
v8_monolithic = true
v8_use_external_startup_data = false
```

The build arguments for the `gn args` command on **Windows** should be set to (h/t MakotoE via [this SO post](https://stackoverflow.com/a/67041879)):
```
is_debug = true
target_cpu = "x64"
v8_enable_backtrace = true
v8_enable_slow_dchecks = true
v8_optimized_debug = false
v8_monolithic = true
v8_use_external_startup_data = false
is_component_build = false
is_clang = false
```

NOTE: you may need to make some modifications to the v8 source code to patch out some DCHECK calls that don't compile properly for some reason

Reference: https://v8.dev/docs/embed#run-the-example

## Building

Building the implant.js client requires cmake 3.15+

On Linux:
```
$ mkdir build
$ cmake -S . -B build -DV8_ROOT:STRING=/path/to/v8/root -DBUILD_DEBUG=True
$ cmake --build build
```

On Windows (PowerShell):
```
PS> mkdir build
PS> cmake -S . -B build -DV8_ROOT:STRING=C:\path\to\v8\root -DBUILD_DEBUG=True
PS> cmake --build build
```

The client binary is built at `./build/client` on Linux, and `.\build\Debug\client.exe` on Windows.

## Usage

```
$ ./client localhost 1337
```

## Testing

implant.js uses GoogleTest for unit testing. To build and run the tests:

On Linux:
```
$ mkdir -p build_tests
$ cmake -S . -B build_tests -DV8_ROOT:STRING=/path/to/v8/root -DBUILD_TESTS=True -DBUILD_DEBUG=True
$ cmake --build build_tests
$ (cd build_tests && ctest --output-on-failure)
```

On Windows (PowerShell):
```
PS> mkdir build_tests
PS> cmake -S . -B build_tests -DV8_ROOT:STRING=C:\path\to\v8\root -DBUILD_TESTS=True -DBUILD_DEBUG=True
PS> cmake --build build_tests
PS> powershell -c "cd build_tests; ctest --output-on-failure"
```

For JavaScript-based tests, the contents of `tests/scripts/lib.js` are made available to each script registered in `tests/jseng_test.cc`. For VS Code Intellisense, as long as you have the `lib.js` file open in the editor, it should be aware of the symbols in that library while writing test cases in another file.

OS-specific functionality can be wrapped in `TEST_LINUX()` or `TEST_WINDOWS()` function definitions in each JS test case.

## Resources

- https://v8.dev/docs/embed
- https://chromedevtools.github.io/devtools-protocol/v8/
- https://v8.dev/docs/inspector
- https://github.com/ahmadov/v8_inspector_example
- https://github.com/mutable-org/mutable/blob/main/src/backend/V8Engine.cpp
- https://web.archive.org/web/20210622022956/http://hyperandroid.com/2020/02/12/v8-inspector-from-an-embedder-standpoint/
- https://github.com/ruby0x1/v8-tutorials
- https://github.com/v8/v8/tree/main/samples