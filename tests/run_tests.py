#!/usr/bin/env python3
"""
Automated tests for the kernel.

Each test boots the kernel in QEMU with no window, talks to QEMU's
monitor to press keys (sendkey) and to copy VGA text memory out of the
virtual machine (pmemsave 0xB8000), then checks what's on the screen.

Reading video memory directly means the tests see exactly the 80x25
characters the kernel wrote: no screenshots, no OCR.

Usage:
    python3 tests/run_tests.py              build, then run every test
    python3 tests/run_tests.py --no-build   use the existing build/kernel.bin
    python3 tests/run_tests.py -k nvme      only tests whose name contains "nvme"
"""
import argparse
import os
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KERNEL = ROOT / "build" / "kernel.bin"
QEMU = os.environ.get("QEMU", "qemu-system-i386")

VGA_ADDR, COLS, ROWS = 0xB8000, 80, 25


# --------------------------------------------------------------------------
#  A running kernel, driven through the QEMU monitor
# --------------------------------------------------------------------------

class Kernel:
    def __init__(self, nvme=False):
        self.tmp = tempfile.TemporaryDirectory()
        self.tmpdir = Path(self.tmp.name)
        args = [QEMU, "-kernel", str(KERNEL), "-display", "none",
                "-monitor", "stdio", "-no-reboot"]
        if nvme:
            img = self.tmpdir / "nvme.img"
            with open(img, "wb") as f:
                f.truncate(16 * 1024 * 1024)
            args += ["-drive", f"file={img.as_posix()},if=none,id=nvm,format=raw",
                     "-device", "nvme,serial=deadbeef,drive=nvm"]

        self.proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, bufsize=0)
        self.out = b""
        self.lock = threading.Lock()
        threading.Thread(target=self._reader, daemon=True).start()
        self._wait_for_prompt(0)

    def _reader(self):
        while True:
            chunk = self.proc.stdout.read(4096)
            if not chunk:
                return
            with self.lock:
                self.out += chunk

    def _wait_for_prompt(self, start, timeout=10):
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                if b"(qemu)" in self.out[start:]:
                    return
            if self.proc.poll() is not None:
                raise RuntimeError("QEMU exited:\n" + self.out.decode(errors="replace"))
            time.sleep(0.01)
        raise TimeoutError("QEMU monitor did not answer")

    def monitor(self, command):
        with self.lock:
            start = len(self.out)
        self.proc.stdin.write((command + "\n").encode())
        self.proc.stdin.flush()
        self._wait_for_prompt(start)
        with self.lock:
            reply = self.out[start:]
        if b"Try \"help" in reply or b"unknown command" in reply:
            raise RuntimeError(f"monitor rejected {command!r}: {reply.decode(errors='replace')}")

    # ---- screen ----

    def screen(self):
        """Return the 25 screen lines, read straight from VGA memory."""
        dump = self.tmpdir / "vga.bin"
        if dump.exists():
            dump.unlink()
        # The file name must be quoted: unquoted, "4000 /tmp/..." parses as a division
        self.monitor(f'pmemsave {VGA_ADDR:#x} {COLS * ROWS * 2} "{dump.as_posix()}"')
        for _ in range(100):                       # file may take a moment to land
            if dump.exists() and dump.stat().st_size == COLS * ROWS * 2:
                break
            time.sleep(0.01)
        data = dump.read_bytes()
        return ["".join(chr(data[(r * COLS + c) * 2]) for c in range(COLS)).rstrip()
                for r in range(ROWS)]

    def text(self):
        return "\n".join(self.screen())

    def wait_for(self, needle, timeout=10):
        deadline = time.time() + timeout
        while True:
            text = self.text()
            if needle in text:
                return text
            if time.time() > deadline:
                raise AssertionError(f"timed out waiting for {needle!r}", text)
            time.sleep(0.1)

    def wait_until_ready(self):
        return self.wait_for("Type something!")

    # ---- keyboard ----

    KEYS = {" ": "spc", "\n": "ret", "\b": "backspace", "!": "shift-1", ",": "comma",
            ".": "dot", "-": "minus", "/": "slash", ";": "semicolon"}

    def type(self, text):
        for ch in text:
            if ch in self.KEYS:
                key = self.KEYS[ch]
            elif ch.isupper():
                key = "shift-" + ch.lower()
            else:
                key = ch
            self.press(key)

    def press(self, key):
        self.monitor(f"sendkey {key}")
        time.sleep(0.02)

    def settle(self):
        time.sleep(0.3)

    def close(self):
        try:
            self.proc.stdin.write(b"quit\n")
            self.proc.stdin.flush()
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()
        self.tmp.cleanup()


def check(condition, message, screen):
    if not condition:
        raise AssertionError(message, screen)


def prompt_lines(text):
    return [line for line in text.splitlines() if line.startswith("> ")]


# --------------------------------------------------------------------------
#  Tests
# --------------------------------------------------------------------------

TESTS = []

def test(name, nvme=False):
    def register(fn):
        TESTS.append((name, fn, nvme))
        return fn
    return register


@test("boot log shows every step OK")
def _(k):
    s = k.wait_until_ready()
    check("Hello from my kernel!" in s, "banner missing", s)
    check("[FAIL]" not in s, "a boot step failed", s)
    for step in ["Multiboot magic 0x2BADB002", "GDT loaded", "IDT loaded",
                 "PIC remapped", "Keyboard driver"]:
        line = next((l for l in s.splitlines() if step in l), None)
        check(line is not None and line.startswith("[ OK ]"), f"step not OK: {step}", s)


@test("GDT: CPU uses selectors 0x08 / 0x10")
def _(k):
    s = k.wait_until_ready()
    check("CS=0x00000008 DS=0x00000010 SS=0x00000010" in s, "wrong segment selectors", s)


@test("PCI scan finds the standard QEMU devices")
def _(k):
    s = k.wait_until_ready()
    for name in ["8086:1237  Host bridge", "VGA display controller", "Ethernet controller"]:
        check(name in s, f"missing PCI device: {name}", s)
    check("NVMe" not in s, "NVMe reported but none attached", s)


@test("NVMe controller is detected and its registers read", nvme=True)
def _(k):
    s = k.wait_until_ready()
    check("1B36:0010  NVMe controller" in s, "NVMe not in PCI list", s)
    check("NVMe controller at" in s, "NVMe probe did not run", s)
    check("Version 1." in s, "NVMe version register not read", s)
    check("max queue" in s and "doorbell stride 4B" in s, "CAP register not decoded", s)


@test("keyboard: typed text appears after the prompt")
def _(k):
    k.wait_until_ready()
    k.type("hello world")
    k.settle()
    s = k.text()
    check("> hello world" in prompt_lines(s), "typed text not on screen", s)


@test("keyboard: Shift and Caps Lock")
def _(k):
    k.wait_until_ready()
    k.type("Hi ")
    k.press("caps_lock")
    k.type("abc")
    k.press("caps_lock")
    k.type(" ok!")
    k.settle()
    s = k.text()
    check("> Hi ABC ok!" in prompt_lines(s), "Shift / Caps Lock handled wrong", s)


@test("keyboard: Backspace erases the last character")
def _(k):
    k.wait_until_ready()
    k.type("abcd\b\bx")
    k.settle()
    s = k.text()
    check("> abx" in prompt_lines(s), "backspace did not erase correctly", s)


@test("keyboard: Backspace can't erase the prompt")
def _(k):
    k.wait_until_ready()
    k.type("a\b\b\b\bz")
    k.settle()
    s = k.text()
    check("> z" in prompt_lines(s), "backspace went past the prompt", s)


@test("keyboard: Enter starts a new prompt line")
def _(k):
    k.wait_until_ready()
    k.type("one\ntwo")
    k.settle()
    lines = prompt_lines(k.text())
    check(lines[-2:] == ["> one", "> two"], "Enter did not start a new line", k.text())


@test("exception: F1 shows a divide-by-zero kernel panic")
def _(k):
    k.wait_until_ready()
    k.press("f1")
    s = k.wait_for("KERNEL PANIC", timeout=5)
    check("Divide by zero" in s and "exception 0x00000000" in s, "wrong exception reported", s)
    check("The system has been halted." in s, "no halt message", s)
    k.type("x")                                # a halted kernel must not react
    k.settle()
    check(k.text() == s, "kernel kept running after the panic", k.text())


# --------------------------------------------------------------------------
#  Runner
# --------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--no-build", action="store_true", help="don't run make first")
    parser.add_argument("-k", metavar="TEXT", help="only run tests whose name contains TEXT")
    args = parser.parse_args()

    if not args.no_build:
        subprocess.run(["make", "-s"], cwd=ROOT, check=True)
    if not KERNEL.exists():
        sys.exit(f"{KERNEL} not found, build it first")

    selected = [t for t in TESTS if not args.k or args.k.lower() in t[0].lower()]
    print(f"Running {len(selected)} tests against {KERNEL.relative_to(ROOT)}\n")

    failed = 0
    for name, fn, nvme in selected:
        start = time.time()
        k = Kernel(nvme=nvme)
        try:
            fn(k)
            print(f"  PASS  {name:<55} {time.time() - start:4.1f}s")
        except AssertionError as e:
            failed += 1
            message, screen = (e.args + ("", ""))[:2]
            print(f"  FAIL  {name}\n        {message}\n        screen was:")
            for line in str(screen).splitlines():
                print(f"        | {line}")
        finally:
            k.close()

    print(f"\n{len(selected) - failed} passed, {failed} failed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
