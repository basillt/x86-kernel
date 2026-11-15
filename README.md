# x86 Kernel from Scratch

A 32-bit x86 kernel written from scratch in C and NASM Assembly, built one section at a time.
Every section comes with a PDF write-up: what I learned, the code that matters, and what it looked like when it worked.

Right now it boots, sets up its own GDT and IDT, shows a panic screen on CPU exceptions, and has a working keyboard driver you can type into.

![Latest result](docs/04-keyboard-qemu.png)

## Progress

| # | Section | Status | Write-up |
|---|---------|--------|----------|
| 1 | Booting (Multiboot) & VGA text output | ✅ Done | [PDF](docs/01-boot-and-vga.pdf) |
| 2 | GDT (Global Descriptor Table) | ✅ Done | [PDF](docs/02-gdt.pdf) |
| 3 | IDT & CPU exceptions | ✅ Done | [PDF](docs/03-idt.pdf) |
| 4 | PIC, IRQs & keyboard driver | ✅ Done | [PDF](docs/04-keyboard.pdf) |
| 5 | Timer (PIT) | ⏳ Next | |
| 6 | Paging | | |
| 7 | Memory allocator | | |
| 8 | Simple shell | | |

## Build & run

Requirements (Ubuntu / WSL):

```bash
sudo apt install build-essential gcc-multilib nasm qemu-system-x86 make
```

```bash
make          # build build/kernel.bin
make run      # boot it in QEMU
make debug    # log interrupts, don't reboot on triple fault
make clean
```

On Windows without WSL, install QEMU (`winget install SoftwareFreedomConservancy.QEMU`) and boot a built `kernel.bin` directly:

```powershell
& "C:\Program Files\qemu\qemu-system-i386.exe" -kernel kernel.bin
```

## Layout

```
src/boot.asm     Multiboot header, stack, _start entry point
src/kernel.c     kernel_main()
src/vga.c        VGA text-mode driver
src/gdt.c        Global Descriptor Table setup
src/gdt_flush.asm  lgdt + segment register reload
src/idt.c        Interrupt Descriptor Table
src/interrupts.asm  entry stubs for exceptions and IRQs
src/isr.c        exception handler / kernel panic screen
src/pic.c        8259 PIC remapping, masking, EOI
src/irq.c        IRQ handler registration and dispatch
src/keyboard.c   PS/2 keyboard driver (scancodes, Shift, Caps Lock)
include/         headers
linker.ld        memory layout (kernel loaded at 1 MB)
docs/            per-section write-ups (PDF)
```
