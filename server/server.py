#!/usr/bin/env python3

import argparse
import copy
import dataclasses
import glob
import logging
import os
import queue
import re
import socket
import socketserver
import struct
import sys
import threading
import time

logging.basicConfig(
    format="%(asctime)s [%(levelname)s] %(message)s",
    level=logging.INFO,
    datefmt="%Y-%m-%d %H:%M:%S",
)
LOG = logging.getLogger("implant.js")

HELP_TEXT = """\
implant.js commands:

lsmod           - list available modules
reload          - reload modules from disk
run <module>    - run the specified module 
debug <module>  - run the specified module in interactive debug mode
dc              - disconnect from the client
exit            - terminate the server
"""

DBG_HELP_TEXT = """\
implant.js debugger commands:

c, continue     - continue execution
s, step         - step into
n, next         - step over
so, stepout     - step out of (finish function)
k               - show current call stack

bp, breakset    - set breakpoint
bl, breaklist   - list breakpoints
bc, breakclear  - clear breakpoint

l, list         - show source code
e, eval         - show a js var/expression value

q, quit         - end debugging session
"""

HS_SYN = b"\x13\x37"
HS_ACK = b"\x73\x31"

PKT_FETCH = b"\x80"
PKT_MODULE = b"\x81"
PKT_RESP = b"\x82"
PKT_NOOP = b"\x90"
PKT_BYE = b"\x91"

PKT_DBG = 0xdd

DBG_CMD_CONTINUE = 0xe0
DBG_CMD_QUIT = 0xe1
DBG_CMD_STEP = 0xe2
DBG_CMD_NEXT = 0xe3
DBG_CMD_STEPOUT = 0xe4
DBG_CMD_BREAKSET = 0xe5
DBG_CMD_BREAKCLEAR = 0xe6
DBG_CMD_EVAL = 0xe7

DBG_RESP_READY = 0xf0
DBG_RESP_CONTEXT = 0xf1
DBG_RESP_OUTPUT = 0xf2
DBG_RESP_BREAKSET = 0xf3
DBG_RESP_EVAL = 0xf4

STATUS_SUCCESS = 0xa0
STATUS_FAILURE = 0xa1
STATUS_TERM = 0xa2
STATUS_RUNNING = 0xa3

OS_LINUX = 0xc1
OS_WINDOWS = 0xc2

IMPORT_RE = re.compile(r"^import[ \t]+(\*|(\{([ \t]*[a-zA-Z0-9_]+[ \t]*,?)+\}))[ \t]+from[ \t]+('|\")(?P<path>[a-zA-Z0-9\/\.]+\.js)('|\")[ \t]*;?$")

class Module:
    def __init__(self, name: str, code: str):
        self.name = name
        self.code = code

        self._lines: list[str] | None = None

        # filename->(starting line index, num lines)
        self._map: dict[str, tuple[int, int]] = {}
        
        self._mod_start_idx = -1

    def pack(self) -> bool:
        mod_base_dir = get_module_base_dir()
        orig_lines = [x.rstrip() for x in self.code.splitlines()]
        new_lines = []
        for l in orig_lines:
            l = l.rstrip()

            if m := IMPORT_RE.match(l):
                if self._mod_start_idx != -1:
                    LOG.error("invalid module, library imports must be at the beginning")
                    return False
                lib_path = m.group("path")
                try:
                    with open(os.path.join(mod_base_dir, lib_path), "r") as f:
                        lib_code = [x.rstrip() for x in f.readlines()]

                        filename = os.path.basename(lib_path)
                        if filename.endswith(".js"):
                            filename = filename[:-3]
                        self._map[filename] = (len(new_lines), len(lib_code))
                        new_lines.extend(lib_code)
                except FileNotFoundError:
                    LOG.error("failed to pack %s, couldn't read library %s", self.name, lib_path)
                    return False
            else:
                if self._mod_start_idx == -1 and not (l.startswith("//") or len(l) == 0):
                    self._mod_start_idx = len(new_lines)
                new_lines.append(l)

        self.code = "\n".join(new_lines)
        return True
    
    def true_line_for_mod_line(self, filename: str | None, line: int) -> int | None:
        """
        Get the line for a module. If filename is none, line number is assumed to be for the mod itself. Otherwise, it needs to be the name of a library.
        
        Returns None if the mod wasn't found or the line number is invalid.
        """

        if filename and filename.endswith(".js"):
            filename = filename[:-3]

        if filename is None or os.path.basename(filename) == os.path.basename(self.name):
            return self._mod_start_idx+line

        if filename not in self._map:
            return None
        start, numlines = self._map[filename]
        if line > numlines:
            return None
        return start+line
    
    def dump(self, cur_lineno: int) -> str:
        lines = self.lines
        num_lines = len(lines)
        width = len(str(num_lines))

        out_lines: list[str] = []

        for i in range(num_lines):
            i = i+1
            padding = width-len(str(i))

            prefix = "===> " if i == cur_lineno else "     "

            prefix += " "*padding + str(i)

            out_lines.append(prefix + " " + lines[i-1])
        
        return "\n".join(out_lines)
    
    @property
    def lines(self) -> list[str]:
        if not self._lines:
            self._lines = [x.rstrip() for x in self.code.split("\n")]
        return self._lines

@dataclasses.dataclass
class ModuleExec:
    mod: Module
    debug: bool = False

@dataclasses.dataclass
class CallFrame:
    lineno: int
    symbol: str


class DebugPacket():
    def __init__(self, t: int):
        self.t = t

    def pack(self) -> bytes:
        return bytes([PKT_DBG, self.t])

    @staticmethod
    def fetch(_: socket.socket) -> "DebugPacket":
        raise NotImplementedError()


class ClientState:
    def __init__(self):
        self._mut_lock = threading.Lock()

        self._os: str
        self._active = False
        self._blocked = False
        self._module_queue: queue.Queue[ModuleExec]

        self._debugging = False
        self._dbg_err = False
        self._dbg_status = 0
        self._cur_frames: list[CallFrame]
        self._dbg_mod: Module
        self._dbg_pkt_queue: queue.Queue[DebugPacket]
        self._dbg_pkt_sender: threading.Thread
        self._bps: dict[int, str]  # line number->id
        self._bp_map: dict[int, int]  # bp 'number' -> line number
        self._bp_ctr = 0

        self.reset()
        self.disconnect()

    @property
    def active(self) -> bool:
        return self._active
    
    @property
    def os(self) -> str:
        return self._os
    
    @property
    def is_blocked(self) -> bool:
        return self._blocked
    
    @property
    def debugging(self) -> bool:
        return self._debugging
    
    @property
    def dbg_paused(self) -> bool:
        return self._dbg_paused
    
    @property
    def dbg_mod(self) -> Module:
        return self._dbg_mod

    def set_os(self, os: int) -> bool:
        if os == OS_LINUX:
            self._os = "Linux"
        elif os == OS_WINDOWS:
            self._os = "Windows"
        else:
            LOG.error("unknown operating system byte from client: 0x%x", os)
            return False
        
        return True

    def disconnect(self):
        with self._mut_lock:
            self._active = False

    def add_module(self, mod: Module, debug: bool = False):
        self._module_queue.put(ModuleExec(mod, debug))

    def get_module(self) -> ModuleExec | None:
        try:
            return self._module_queue.get_nowait()
        except queue.Empty:
            return None

    def reset(self):
        with self._mut_lock:
            self._active = True
            self._blocked = False
            self._module_queue: queue.Queue[ModuleExec] = queue.Queue()

    def block(self):
        with self._mut_lock:
            self._blocked = True

    def unblock(self):
        with self._mut_lock:
            self._blocked = False

    def start_debugging(self, s: socket.socket, mod: Module):
        with self._mut_lock:
            self._debugging = True
            self._dbg_paused = True
            self._dbg_status = 0
            self._cur_frames = []
            self._dbg_mod = mod
            self._dbg_pkt_queue = queue.Queue()
            self._bps = {}
            self._bp_map = {}
            self._bp_ctr = 1

            def __worker():
                while self._debugging:
                    while (pkt := STATE.get_debug_pkt()):
                        LOG.debug("sending dbg pkt: %s", pkt)
                        s.sendall(pkt.pack())
                    time.sleep(0.2)

            self._dbg_pkt_sender = threading.Thread(target=__worker)
            self._dbg_pkt_sender.start()

    def update_dbg_ctx(self, ctx: "DbgContext") -> bool:
        """Return True if the debugger should continue executing, otherwise False"""

        no_frames = len(self._cur_frames) == 0

        if ctx.exc:
            print("execution interrupted due to unhandled exception:")
            print(ctx.exc)

        with self._mut_lock:
            self._cur_frames = ctx.frames

            if ctx.status == STATUS_RUNNING:
                self._dbg_paused = True
            else:
                self._dbg_status = ctx.status
        
        if STATE.is_blocked and no_frames:
            STATE.unblock()

        return ctx.status == STATUS_RUNNING
    
    def stop_debugging(self):
        self.block()
        with self._mut_lock:
            # wait for all queued packets to be sent
            while self._dbg_pkt_queue.qsize() > 0:
                time.sleep(0.5)
            self._debugging = False
            self._dbg_pkt_sender.join(1)
        self.unblock()

    def prompt(self) -> str:
        if self._debugging:
            return f"{self.dbg_context()}\ndebug({self._dbg_mod.name})> "
        else:
            return "cmd> "
        
    def dbg_context(self) -> str:
        if not self._debugging:
            raise RuntimeError("not in debug mode, can't get a context")
        
        while len(self._cur_frames) == 0:
            time.sleep(0.2)
        
        # TODO: need to unpack the mods and put the mod context so that the filename+line shown here is what it looked like before packing
        f = self._cur_frames[0]
        return f"L{f.lineno} - {f.symbol or '<global>'}: {self._dbg_mod.lines[f.lineno-1].strip()}"
        
    def queue_debug_pkt(self, pkt: DebugPacket):
        self._dbg_pkt_queue.put(pkt)

        # all of these packet types resume execution on the client
        if any(isinstance(pkt, x) for x in (DbgContinue, DbgStep, DbgNext, DbgStepOut)):
            self._dbg_paused = False

    def get_debug_pkt(self) -> DebugPacket | None:
        try:
            return self._dbg_pkt_queue.get_nowait()
        except queue.Empty:
            return None
        
    def record_bp(self, lineno: int, id: str):
        """Record a line number as having a breakpoint set as returned by the server."""

        # convert the line number
        # maybe?
        self._bps[lineno] = id
        self._bp_map[self._bp_ctr] = lineno
        self._bp_ctr += 1

    def has_bp_for_line(self, lineno: int) -> bool:
        return lineno in self._bps
    
    def print_bps(self):
        if len(self._bps) == 0:
            print("no breakpoints yet")
            return

        for num, lineno in self._bp_map.items():
            print(f"#{num} - line {lineno}: {self._dbg_mod.lines[lineno-1]}")

    def print_callstack(self):
        # TODO: enrich these
        for f in self._cur_frames:
            print(f)

    def print_source(self):
        cur_lineno = self._cur_frames[0].lineno

        print(self._dbg_mod.dump(cur_lineno))

    def get_bp_id_for_num(self, num: int) -> str | None:
        return self._bps.get(self._bp_map.get(num))
    
    def remove_bp(self, num: int):
        lineno = self._bp_map[num]
        del self._bp_map[num]
        del self._bps[lineno]

STATE = ClientState()

class DbgReady(DebugPacket):
    def __init__(self):
        super().__init__(DBG_RESP_READY)
    
    @staticmethod
    def fetch(_: socket.socket):
        return DbgReady()


class DbgContext(DebugPacket):
    def __init__(self, status: int, frames: list[CallFrame], exc: str | None = None):
        super().__init__(DBG_RESP_CONTEXT)

        self.status = status
        self.frames = frames
        self.exc = exc

    @staticmethod
    def fetch(s: socket.socket):
        d = struct.unpack("!BI", s.recv(5))
        status = d[0]
        num_frames = d[1]

        frames: list[CallFrame] = []
        for _ in range(num_frames):
            d = struct.unpack("!II", s.recv(8))
            lineno = d[0]
            l = d[1]

            sym = s.recv(l).decode()

            frames.append(CallFrame(lineno, sym))
        
        p = DbgContext(status, frames)

        l = struct.unpack("!I", s.recv(4))[0]
        if l > 0:
            p.exc = s.recv(l).decode()

        return p


class DbgOutput(DebugPacket):
    def __init__(self, output: str):
        super().__init__(DBG_RESP_OUTPUT)
        self.output = output
    
    @staticmethod
    def fetch(s: socket.socket):
        sz = struct.unpack("!I", s.recv(4))[0]
        output = s.recv(sz).decode()

        return DbgOutput(output)

class DbgStep(DebugPacket):
    def __init__(self):
        super().__init__(DBG_CMD_STEP)

class DbgNext(DebugPacket):
    def __init__(self):
        super().__init__(DBG_CMD_NEXT)


class DbgStepOut(DebugPacket):
    def __init__(self):
        super().__init__(DBG_CMD_STEPOUT)


class DbgContinue(DebugPacket):
    def __init__(self):
        super().__init__(DBG_CMD_CONTINUE)


class DbgQuit(DebugPacket):
    def __init__(self):
        super().__init__(DBG_CMD_QUIT)


class DbgBreakSet(DebugPacket):
    def __init__(self, lineno: int):
        super().__init__(DBG_CMD_BREAKSET)

        self.lineno = lineno
    
    def pack(self) -> bytes:
        return super().pack() + struct.pack("!I", self.lineno)

class DbgBreakSetResp(DebugPacket):
    def __init__(self, success: bool, lineno: int, id: str):
        super().__init__(DBG_RESP_BREAKSET)
        self.sucess = success
        self.lineno = lineno
        self.id = id
    
    @staticmethod
    def fetch(s: socket.socket):
        d = struct.unpack("!?II", s.recv(9))
        success = d[0]
        lineno = d[1]
        l = d[2]
        
        id = s.recv(l).decode()

        return DbgBreakSetResp(success, lineno, id)


class DbgBreakClear(DebugPacket):
    def __init__(self, id: str):
        super().__init__(DBG_CMD_BREAKCLEAR)

        self.id = id
    
    def pack(self) -> bytes:
        return super().pack() + struct.pack("!I", len(self.id)) + self.id.encode()


class DbgEval(DebugPacket):
    def __init__(self, expr: str):
        super().__init__(DBG_CMD_EVAL)

        self.expr = expr
    
    def pack(self) -> bytes:
        return super().pack() + struct.pack("!I", len(self.expr)) + self.expr.encode()


class DbgEvalResp(DebugPacket):
    def __init__(self, output: str, error: bool):
        super().__init__(DBG_RESP_EVAL)
        self.output = output
        self.error = error

    @staticmethod
    def fetch(s: socket.socket):
        l = struct.unpack("!I", s.recv(4))[0]
        output = s.recv(l).decode()
        err = bool(struct.unpack("!?", s.recv(1))[0])

        return DbgEvalResp(output, err)


class ServerHandler(socketserver.BaseRequestHandler):
    request: socket.socket

    def _get_dbg_pkt(self) -> DebugPacket | None:
        pkt = self.request.recv(1)[0]

        if pkt != PKT_DBG:
            LOG.error("didn't get a debug packet, got something else: %x", pkt)
            return None
        
        t = self.request.recv(1)[0]

        if t == DBG_RESP_READY:
            pt = DbgReady
        elif t == DBG_RESP_CONTEXT:
            pt = DbgContext
        elif t == DBG_RESP_OUTPUT:
            pt = DbgOutput
        elif t == DBG_RESP_BREAKSET:
            pt = DbgBreakSetResp
        elif t == DBG_RESP_EVAL:
            pt = DbgEvalResp
        else:
            LOG.error("unknown debug packet type: %x", t)
            return None

        return pt.fetch(self.request)

    def handle(self):
        LOG.info("new connection from %s:%d", *self.client_address)

        syn = self.request.recv(2)
        if syn != HS_SYN:
            LOG.error("client failed handshake, disconnecting")
            STATE.disconnect()
            return
        os = self.request.recv(1)[0]
        if not STATE.set_os(os):
            STATE.disconnect()
            return
        LOG.info("client is running %s", STATE.os)
        self.request.sendall(HS_ACK)
        
        STATE.reset()

        while STATE.active:
            if STATE.debugging:
                pkt = self._get_dbg_pkt()
                if pkt is None:
                    STATE.disconnect()
                    return
                
                match pkt:
                    case DbgReady():
                        LOG.debug("client is ready")
                        pass
                    case DbgOutput():
                        print(pkt.output, end="")
                    case DbgContext():
                        if not STATE.update_dbg_ctx(pkt):
                            if pkt.status == STATUS_SUCCESS:
                                print("[module execution completed successfully]")
                            elif pkt.status == STATUS_FAILURE:
                                print("[module execution failed]")
                                # TODO: show some error context
                            elif pkt.status == STATUS_TERM:
                                print("[module execution terminated]")

                            STATE.stop_debugging()
                    case DbgBreakSetResp():
                        if pkt.sucess:
                            print("breakpoint set")
                            STATE.record_bp(pkt.lineno, pkt.id)
                        else:
                            print("failed to set breakpoint")
                        STATE.unblock()
                    case DbgEvalResp():
                        if pkt.error:
                            print("error while evaluating expression:")
                        print(pkt.output)
                        STATE.unblock()
                    case _:
                        LOG.error("bad state transition in debugger, can't handle a %s packet currently", type(pkt))
            else:
                pkt = self.request.recv(1)
                if pkt == PKT_FETCH:
                    mod = STATE.get_module()
                    if mod:
                        self.request.sendall(PKT_MODULE + struct.pack("!?I", mod.debug, len(mod.mod.code)) + mod.mod.code.encode())

                        if mod.debug:
                            STATE.start_debugging(self.request, mod.mod)
                            LOG.info("starting debug session")
                        else:
                            if self.request.recv(1) != PKT_RESP:
                                LOG.error("didn't get a response packet back from the client, disconnecting")
                                STATE.disconnect()
                                return

                            status = struct.unpack("!B", self.request.recv(1))
                            if status == STATUS_FAILURE:
                                LOG.error("module failed to be executed")
                            elif status == STATUS_SUCCESS:
                                LOG.info("module was executed")
                            sz = struct.unpack("!I", self.request.recv(4))[0]
                            if sz > 0:
                                data = self.request.recv(sz).decode()
                                LOG.info("output from the client:")
                                print(data.rstrip())

                        STATE.unblock()
                    else:
                        self.request.sendall(PKT_NOOP)
                elif pkt == PKT_RESP:
                    pass
                elif len(pkt) == 0:
                    # client probably is disconnected
                    break
                else:
                    LOG.error("unrecognized pkt from client: %s", pkt.hex())

        self.request.sendall(PKT_BYE)
        
        LOG.info("waiting for next client")

def run_server(port: int, stop_event: threading.Event):
    """Server thread entrypoint"""
    
    with socketserver.TCPServer(("0.0.0.0", port), ServerHandler) as server:
        def _shutdown():
            stop_event.wait()
            server.shutdown()
        t = threading.Thread(target=_shutdown)
        t.daemon = True
        t.start()

        LOG.info("server listening on port %d", port)
        server.serve_forever()
        t.join(1)

MOD_ENV_KEY = "IMPJS_MODULE_DIR"
def get_module_base_dir() -> str:
    if MOD_ENV_KEY in os.environ:
        p = os.environ[MOD_ENV_KEY]
        LOG.info("using modules from $%s (%s)", MOD_ENV_KEY, p)
        return p

    return str(os.path.join(os.path.dirname(__file__), "..", "modules"))

def load_modules() -> dict[str, Module]:
    mods = {}

    base_path = get_module_base_dir()
    for f in glob.glob(pathname="**/*.js", root_dir=base_path, recursive=True):
        name = f[:-3]
        with open(os.path.join(base_path, f), "r") as f:
            code = f.read()

        m = Module(name, code)
        if m.pack():
            mods[name] = m

    return mods

def cmd_repl():
    mods = load_modules()

    while True:
        if not STATE.active:
            time.sleep(1)
            continue

        if STATE.is_blocked or (STATE.debugging and not STATE.dbg_paused):
            time.sleep(0.2)
            continue

        inp = input(STATE.prompt()).strip()
        if len(inp) == 0:
            continue

        parts = inp.split(" ")
        cmd = parts[0]
        args = parts[1:]

        if STATE.debugging:
            match cmd:
                case "c" | "continue":
                    STATE.queue_debug_pkt(DbgContinue())
                case "s" | "step":
                    STATE.queue_debug_pkt(DbgStep())
                case "n" | "next":
                    STATE.queue_debug_pkt(DbgNext())
                case "so" | "stepout":
                    STATE.queue_debug_pkt(DbgStepOut())
                case "bp" | "breakset":
                    if len(args) == 1:
                        mod = None
                        lineno = args[0]
                    elif len(args) == 2:
                        mod = args[0]
                        lineno = args[1]
                    else:
                        print("usage: `breakset <line num>` or `breakset <module> <line num>`")
                        continue

                    try:
                        lineno = int(lineno)
                    except ValueError:
                        print("invalid line number")
                        continue

                    lineno = STATE.dbg_mod.true_line_for_mod_line(mod, lineno)
                    if lineno is not None:
                        if STATE.has_bp_for_line(lineno):
                            print("this breakpoint already exists, ignoring")
                            continue
                        STATE.queue_debug_pkt(DbgBreakSet(lineno))
                        STATE.block()
                case "bl" | "breaklist":
                    STATE.print_bps()
                case "bc" | "breakclear":
                    if len(args) != 1:
                        print("usage: `breakclear <num>`")
                        continue

                    try:
                        num = int(args[0])
                    except ValueError:
                        print("invalid breakpoint number")
                        continue

                    id = STATE.get_bp_id_for_num(num)
                    if id is None:
                        print("invalid breakpoint number")
                        continue

                    STATE.queue_debug_pkt(DbgBreakClear(id))
                    STATE.remove_bp(num)
                case "l" | "list":
                    STATE.print_source()
                case "e" | "eval":
                    if len(args) < 1:
                        print("usage: `eval <expression>`")
                        continue

                    expr = " ".join(args)
                    STATE.queue_debug_pkt(DbgEval(expr))
                    STATE.block()
                case "k":
                    STATE.print_callstack()
                case "q" | "quit":
                    STATE.queue_debug_pkt(DbgQuit())
                    STATE.stop_debugging()
                case "h" | "help" | "?":
                    print(DBG_HELP_TEXT)
                case _:
                    print("unknown command, run \"help\" for available commands")
        else:
            match cmd:
                case "help":
                    print(HELP_TEXT, end="")
                case "exit":
                    print("byebye!")
                    return
                case "dc":
                    STATE.disconnect()
                case "lsmod":
                    if len(mods) == 0:
                        print("no modules available")
                    else:
                        print("available modules:")
                        # list the modules at the root directory before the ones in directories
                        names = sorted(list(mods.keys()), key=lambda x: "/" + x if "/" not in x else x)
                        print("\n".join("- " + x for x in names))
                case "reload":
                    print("reloading modules...")
                    mods = load_modules()
                    print(f"loaded {len(mods)} modules")
                case "run":
                    if len(args) != 1:
                        print("usage: run <module name>")
                        continue
                    mod_name = args[0]
                    if mod_name not in mods:
                        LOG.error("module %s not found", mod_name)
                        continue
                    print(f"running module {mod_name}")
                    STATE.add_module(mods[mod_name])
                    STATE.block()
                case "debug":
                    if len(args) != 1:
                        print("usage: debug <module name>")
                        continue
                    mod_name = args[0]
                    if mod_name not in mods:
                        LOG.error("module %s not found", mod_name)
                        continue
                    print(f"running module {mod_name} in debug mode")
                    STATE.add_module(mods[mod_name], debug=True)
                    STATE.block()
                case _:
                    print("unknown command, run \"help\" for available commands")

def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="implant.js server")
    parser.add_argument("-p", "--port", type=int, default=1337, help="Server port (default: 1337)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable debug logging")

    return parser.parse_args(argv)

def main(argv: list[str]) -> int:
    args = parse_args(argv)

    if args.verbose:
        LOG.setLevel(logging.DEBUG)

    stop_event = threading.Event()
    server_thread = threading.Thread(target=run_server, args=(args.port, stop_event))
    server_thread.daemon = True
    server_thread.start()

    try:
        cmd_repl()
    except (KeyboardInterrupt, EOFError):
        print()

    LOG.info("stopping server")
    stop_event.set()
    server_thread.join(1)

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
