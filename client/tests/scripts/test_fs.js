let path, dir, root, root_ent;
if (ctx.os() == OS_WINDOWS) {
    path = "C:\\ProgramData\\asdfasdf";
    dir = "C:\\ProgramData";
    root = "C:\\";
    root_ent = "ProgramData";
} else {
    path = "/tmp/asdfasdf";
    dir = "/tmp";
    root = "/";
    root_ent = "tmp";
}
let dir_ent = "asdfasdf";

TEST_STRICTFALSE(ctx.fs.file_exists(dir));
TEST_STRICTTRUE(ctx.fs.dir_exists(dir));

let h = ctx.fs.open(path, MODE_W);
ctx.fs.write(h, "hello world\n");
ctx.fs.seek(h, 0, SEEK_SET);
ctx.fs.write(h, new Uint8Array([119, 111, 114, 108, 100])); // "world"
ctx.fs.close(h);

TEST_STRICTFALSE(ctx.fs.dir_exists(path));
TEST_STRICTTRUE(ctx.fs.file_exists(path));

let dir_contents = ctx.fs.dir_contents(root);

TEST_ARRCONTAINS(dir_contents, root_ent);
TEST_ARRNOTCONTAINS(dir_contents, ".");
TEST_ARRNOTCONTAINS(dir_contents, "..");
TEST_ARRCONTAINS(ctx.fs.dir_contents(dir), dir_ent);

h = ctx.fs.open(path, MODE_R);
let s = ctx.fs.read_line(h);
const expected = "world world";
TEST_STRICTEQ(s, expected);
ctx.fs.close(h);

ctx.fs.delete_file(path);
TEST_STRICTFALSE(ctx.fs.file_exists(path));

function TEST_WINDOWS() {
    // make sure the \r\n stuff works properly
    let p = "C:\\windows\\temp\\wintest.txt";
    let f = ctx.fs.open(p, MODE_W);
    ctx.fs.write(f, "hello world\r\nthis is another line\r\n\r\n");
    ctx.fs.close(f);

    f = ctx.fs.open(p, MODE_R);
    let l = ctx.fs.read_line(f);
    TEST_STRICTEQ(l, "hello world");
    l = ctx.fs.read_line(f);
    TEST_STRICTEQ(l, "this is another line");
    l = ctx.fs.read_line(f);
    TEST_STRICTEQ(l, "");
    TEST_STRICTTRUE(ctx.fs.eof(f));
}
