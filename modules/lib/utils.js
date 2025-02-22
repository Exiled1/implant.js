const ON_WINDOWS = ctx.os() === OS_WINDOWS;
const ON_LINUX = ctx.os() === OS_LINUX;

function hex(v) {
    return `0x${v.toString(16)}`;
}

function hexdump(ptr, sz) {
    const data = ctx.mem.read(ptr, sz);

    let buf = `dump of ${hex(sz)} bytes from ${hex(ptr)}: `;
    data.forEach(c => buf += c.toString(16) + " ");

    return buf;
}

function malloc(sz) {
    return ctx.mem.alloc(sz, MEM_RW);
}

function free(ptr) {
    ctx.mem.free(ptr);
}

function uint8ArrToString(arr) {
    let s = "";
    arr.forEach(c => s += String.fromCharCode(c));
    return s;
}

// Convert raw memory with sz QWORDs into a JS bigint array
function rawToBigintArr(ptr, sz) {
    let a = new BigUint64Array(sz);
    for (let i = 0; i < sz; i++) {
        a[i] = ctx.mem.read_qword(ptr + BigInt(i * 8));
    }
    return a;
}

// Read a null-terminated string from raw mem into a JS string
function rawToString(ptr) {
    let out = "";
    while (true) {
        let v = ctx.mem.read(ptr++, 1)[0];
        if (v == 0) break;
        out += String.fromCharCode(v);
    }
    return out;
}