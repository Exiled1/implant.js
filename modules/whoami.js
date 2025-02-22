if (ctx.os() === OS_WINDOWS) {
    ctx.output(ctx.system("whoami"));
} else {
    ctx.output(ctx.system("id"));
}