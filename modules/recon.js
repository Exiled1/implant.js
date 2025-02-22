import { ON_LINUX, ON_WINDOWS } from "lib/utils.js";

function user_dirs() {
    let dirs;
    if (ON_WINDOWS) {
        dirs = ctx.fs.dir_contents("C:\\Users").filter(x => !["desktop.ini", "All Users", "Default", "Default User"].includes(x));
    }

    if (ON_LINUX) {
        dirs = ctx.fs.dir_contents("/home");
    }

    ctx.output(dirs.join("\n"));
}

function root_dir() {
    let dirs;
    if (ON_WINDOWS) {
        dirs = ctx.fs.dir_contents("C:\\");
    }
    if (ON_LINUX) {
        dirs = ctx.fs.dir_contents("/");
    }

    ctx.output(dirs.join("\n"));
}

function are_we_admin() {
    if (ON_WINDOWS) {
        let output = ctx.system("whoami /groups");
        return [
            "BUILTIN\\Administrators",
            "Mandatory Label\\High Mandatory Level"
        ].find(s => output.includes(s)) !== undefined;
    }

    if (ON_LINUX) {
        let output = ctx.system("id");
        return [
            "(sudo)",
            "(wheel)",
            "(admin)",
        ].find(s => output.includes(s)) !== undefined;
    }
}

function are_we_system() {
    if (ON_WINDOWS) {
        return ctx.system("whoami").toLowerCase().trim() === "nt authority\\system";
    }

    if (ON_LINUX) {
        return ctx.system("whoami").toLowerCase().trim() === "root";
    }
}

function user() {
    let username = ctx.system('whoami').trim();
    if (ON_WINDOWS) {
        username = username.split("\\")[1];
    }

    ctx.output(`Current username: ${username}`);
    ctx.output(`Have ${ON_WINDOWS ? 'SYSTEM' : 'root'} privileges? ${are_we_system()}`);
    ctx.output(`Have admin privileges? ${are_we_admin()}`);
    ctx.output(`Full user info:\n${ctx.system(ON_WINDOWS ? 'whoami /all' : 'id')}`)
}

function system() {
    if (ON_WINDOWS) {
        ctx.output("Current OS: Windows");
    }
    if (ON_LINUX) {
        ctx.output("Current OS: Linux");
    }

    ctx.output(`Hostname: ${ctx.system('hostname').trim()}`)

    if (ON_WINDOWS) {
        // https://stackoverflow.com/a/11606994
        ctx.output(`Uptime: ${ctx.system('powershell "$operatingSystem = Get-WmiObject Win32_OperatingSystem; write-host $((Get-Date) - ([Management.ManagementDateTimeConverter]::ToDateTime($operatingSystem.LastBootUpTime)))"').trim()}`);
    }
    if (ON_LINUX) {
        ctx.output(`Uptime: ${ctx.system('uptime').trim()}`)
    }
}

ctx.output("---- System info");
system();

ctx.output("\n---- User info");
user();

ctx.output("\n---- User directories identified");
user_dirs();

ctx.output("\n---- Entries in root directory");
root_dir();