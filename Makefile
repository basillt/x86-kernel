# ============================================================
#  x86 kernel - build with host gcc in 32-bit freestanding mode
# ============================================================
CC      := gcc
AS      := nasm
LD      := ld

CFLAGS  := -m32 -std=gnu11 -O2 -Wall -Wextra -Iinclude \
           -ffreestanding -fno-builtin -nostdlib \
           -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fcf-protection=none
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib -z noexecstack

BUILD   := build
KERNEL  := $(BUILD)/kernel.bin

C_SRCS   := $(wildcard src/*.c)
ASM_SRCS := $(wildcard src/*.asm)
OBJS     := $(patsubst src/%.asm,$(BUILD)/%.o,$(ASM_SRCS)) \
            $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRCS))

.PHONY: all run run-nvme debug test clean

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: src/%.c include/*.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

# Boot the kernel directly (QEMU understands Multiboot, no GRUB ISO needed)
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL)

# Boot with a virtual NVMe SSD attached (16 MB empty disk image)
NVME_IMG := $(BUILD)/nvme.img
$(NVME_IMG): | $(BUILD)
	truncate -s 16M $@

run-nvme: $(KERNEL) $(NVME_IMG)
	qemu-system-i386 -kernel $(KERNEL) \
		-drive file=$(NVME_IMG),if=none,id=nvm,format=raw \
		-device nvme,serial=deadbeef,drive=nvm

# Same, but print every interrupt/exception and don't reboot on triple fault
debug: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -d int,cpu_reset -no-reboot

# Automated tests: boot in QEMU, type on the keyboard, check the screen
test: $(KERNEL)
	python3 tests/run_tests.py --no-build

clean:
	rm -rf $(BUILD)
