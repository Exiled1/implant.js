ctx.output("hello world");

TEST_TRUE([OS_LINUX, OS_WINDOWS].includes(ctx.os()));

function TEST_LINUX() {
    let id = ctx.system("id");
    TEST_CONTAINS(id, "uid=");
    TEST_CONTAINS(id, "gid=");
    TEST_CONTAINS(id, "groups=");
}

function TEST_WINDOWS() {
    let out = ctx.system("whoami /all");
    TEST_CONTAINS(out, "USER INFORMATION");
    TEST_CONTAINS(out, "GROUP INFORMATION");
    TEST_CONTAINS(out, "PRIVILEGES INFORMATION");
}