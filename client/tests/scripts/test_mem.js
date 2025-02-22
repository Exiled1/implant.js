let ptr1 = ctx.mem.alloc(500, MEM_RWX);
TEST_NE(ptr1, null);

let ptr2 = ctx.mem.alloc(500, MEM_RW);
TEST_NE(ptr2, null);

ctx.mem.write(ptr1, new Uint8Array([0xaa, 0xbb, 0xcc, 0xdd]));
ctx.mem.copy(ptr2, ptr1, 4);

let data = ctx.mem.read(ptr1, 4);

TEST_STRICTTRUE(ctx.mem.equal(ptr1, ptr2, 4))
TEST_STRICTTRUE(ctx.mem.equal(ptr1, new Uint8Array([0xaa, 0xbb, 0xcc, 0xdd])));
TEST_STRICTFALSE(ctx.mem.equal(ptr1, new Uint8Array([0xff, 0xbb, 0xcc, 0xdd])));

TEST_STRICTTRUE(ctx.mem.equal(data, new Uint8Array([0xaa, 0xbb, 0xcc, 0xdd])));

ctx.mem.free(ptr1);
ctx.mem.free(ptr2);

let ptr3 = ctx.mem.alloc(0x10, MEM_RW);

ctx.mem.write_dword(ptr3, 0x1337);
ctx.mem.write_dword(ptr3 + BigInt(2), 0x7331);
TEST_STRICTEQ(ctx.mem.read_dword(ptr3), 0x73311337);

ctx.mem.write_qword(ptr3 + BigInt(8), BigInt(0x12345678abcd0987));
TEST_STRICTEQ(ctx.mem.read_qword(ptr3 + BigInt(8)), BigInt(0x12345678abcd0987));