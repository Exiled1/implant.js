// THIS FILE IS DEPENDED ON BY /tests/e2e.py, MODIFY AT YOUR OWN RISK

import { uint8ArrToString, rawToBigintArr, rawToString } from "lib/utils.js";
import { TEST_STRICTEQ, TEST_ARR_STRICTEQ } from "lib/asserts.js";

ctx.output(uint8ArrToString(new Uint8Array([0x74, 0x72, 0x6f, 0x6e, 0x20, 0x3e, 0x3e, 0x20, 0x74, 0x72, 0x33, 0x6e])));

let sz = 5 * 8;
let ptr = malloc(sz);

// write dwords 0x1 to 0xa into the memory
ctx.mem.write(ptr, new Uint8Array([
    1, 0, 0, 0,
    2, 0, 0, 0,
    3, 0, 0, 0,
    4, 0, 0, 0,
    5, 0, 0, 0,
    6, 0, 0, 0,
    7, 0, 0, 0,
    8, 0, 0, 0,
    9, 0, 0, 0,
    10, 0, 0, 0,
]));

let want = new BigUint64Array([BigInt(0x200000001), BigInt(0x400000003), BigInt(0x600000005), BigInt(0x800000007), BigInt(0xa00000009)]);
let got = rawToBigintArr(ptr, 5);
TEST_ARR_STRICTEQ(want, got);

let arr = new Uint8Array([0x62, 0x69, 0x6e, 0x67, 0x62, 0x6f, 0x6e, 0x67, 0x0]);
ctx.mem.write(ptr, arr);
let s = rawToString(ptr);
TEST_STRICTEQ(s, "bingbong");