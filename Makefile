CC		:= gcc
AS		:= nasm
LD		:= ld
QEMU	:= qemu-system-i386

CFLAGS  := -m32 -std=gnu11 -O2 -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fcf-protection=none -fno-stack-clash-protection -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-80387
LDFLAGS := -m elf_i386 -T linker.ld -z noexecstack --no-warn-rwx-segments -nostdlib

C_SRC	:= $(wildcard src/*.c)
ASM_SRC	:= $(wildcard src/*.asm)
OBJ		:= $(patsubst src/%.c,build/%.o,$(C_SRC)) $(patsubst src/%.asm,build/%.ao,$(ASM_SRC))
KERNEL	:= build/kernel.elf

all: $(KERNEL)

$(KERNEL): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

build/%.o: src/%.c $(wildcard include/*.h) | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.ao: src/%.asm | build
	$(AS) -f elf32 $< -o $@

build:
	mkdir -p build

run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -m 64M -serial stdio -no-reboot

clean:
	rm -rf build

.PHONY: all run clean