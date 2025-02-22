function TEST_LINUX() {
    let system = ctx.ffi.resolve("libc.so.6", "system", TYPE_INTEGER, [TYPE_STRING]);
    TEST_TRUE(system);
    TEST_STRICTEQ(system("id"), 0);

    let malloc = ctx.ffi.resolve("libc.so.6", "malloc", TYPE_POINTER, [TYPE_INTEGER]);
    TEST_TRUE(malloc);
    var ret = malloc(0x300);
    TEST_TRUE(ret);

    let free = ctx.ffi.resolve("libc.so.6", "free", TYPE_VOID, [TYPE_POINTER]);
    TEST_TRUE(free);
    free(ret);
}

function TEST_WINDOWS() {
    let GetCurrentProcess = ctx.ffi.resolve("kernel32.dll", "GetCurrentProcess", TYPE_INTEGER);
    TEST_TRUE(GetCurrentProcess);
    var ret = GetCurrentProcess();
    TEST_TRUE(ret);

    // TODO: expand
}