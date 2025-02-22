#!/usr/bin/env python3

import os
import platform
import random
import shlex
import signal
import subprocess
import time

def FAIL(msg: str):
    print("### TEST FAILED ###")
    raise AssertionError(msg)

class Process:
    def __init__(self, cmd: str):
        self._cmd = cmd
        self._popen = subprocess.Popen(
            args=shlex.split(self._cmd),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0
        )

        self._buf: bytes = b""

    def write_stdin(self, data: str):
        self._popen.stdin.write(data.encode())
    
    def write_line(self, data: str):
        self.write_stdin(data + "\n")

    def check_output(self, data: str, clear=True, chunk_sz=4096, read_count=1):
        # print(self._buf.decode())
        if data.encode() not in self._buf:
            for _ in range(read_count):
                self._buf += self._popen.stdout.read(chunk_sz)

            if data.encode() not in self._buf:
                FAIL(f"not in output: '{data}'\nfull buffer:\n{self._buf.decode()}")

        if clear:
            self._buf = b""

    def check_not_in_output(self, data: str, clear=True, chunk_sz=4096, read_count=1):
        # print(self._buf.decode())
        if data.encode() not in self._buf:
            for _ in range(read_count):
                self._buf += self._popen.stdout.read(chunk_sz)

            if data.encode() in self._buf:
                FAIL(f"in output: '{data}'\nfull buffer:\n{self._buf.decode()}")
        else:
            FAIL(f"in output: '{data}'\nfull buffer:\n{self._buf.decode()}")

        if clear:
            self._buf = b""

    def __repr__(self) -> str:
        return f"<Process {self._cmd}>"
    
    def __del__(self):
        self._popen.terminate()

def sigalrm_handler(*_):
    raise AssertionError("### TEST TIMEOUT, output was probably messy and blocked ###")

def main():
    if os.path.realpath(".").endswith("tests"):
        FAIL("please run e2e.py from the implant.js repo root: ./tests/e2e.py")

    if platform.system() != "Linux":
        FAIL("unsupported operating system, please run the e2e suite on linux")

    timeout = 15
    print(f"### RUNNING E2E TEST SUITE, TIMEOUT IS {timeout} SECONDS ###")

    signal.signal(signal.SIGALRM, sigalrm_handler)
    signal.alarm(timeout)

    port = str(random.randint(30000,50000))

    server = Process(f"python -u ./server/server.py -p {port}")
    server.check_output(f"server listening on port ")

    client = Process(f"./client/build/client localhost {port}")
    client.check_output("successfully connected to localhost:")

    time.sleep(1)

    server.check_output("new connection from 127.0.0.1:", clear=False)
    server.check_output("client is running Linux")

    server.write_line("lsmod")
    server.check_output("available modules:", clear=False)
    server.check_output("- tests/e2e", clear=False)
    server.check_output("- tests/lib")

    server.write_line("run tests/e2e")
    server.check_output("running module tests/e2e", clear=False, read_count=2)

    time.sleep(2)

    client.check_output("executing js module", clear=False)
    client.check_output("this is a successful output!")
    server.check_output("output from the client", clear=False)
    server.check_output("this is a successful output!")

    server.write_line("run tests/lib")
    server.check_output("running module tests/lib", clear=False, read_count=2)

    time.sleep(2)

    client.check_output("executing js module", clear=False)
    client.check_output("tron >> tr3n", clear=False)
    server.check_output("output from the client", clear=False)
    server.check_output("tron >> tr3n")
    client.check_not_in_output("module threw an exception", read_count=0)

    # try to gracefully clean stuff up
    server.write_line("dc")
    server.write_line("exit")

    time.sleep(1)

    del client
    del server

    print("### E2E TEST SUCCESSFULLY PASSED ###")

if __name__ == "__main__":
    main()