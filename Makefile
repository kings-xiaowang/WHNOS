# Makefile — SimpleOS 构建脚本
# Windows: WSL2 中直接 make run，或 Docker 一键构建
# 依赖: Clang(自带交叉编译) + NASM + QEMU + GRUB + xorriso

ASM    = nasm
CC     = clang     --target=i686-elf -m32
CXX    = clang++   --target=i686-elf -m32
LD     = ld.lld    -m elf_i386

CFLAGS   = -std=gnu11 -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-builtin
CXXFLAGS = -std=c++11 -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-exceptions -fno-rtti -fno-use-cxa-atexit -mno-sse -mno-mmx

BUILD_DIR = build
ISO_DIR   = $(BUILD_DIR)/iso
BOOT_DIR  = $(ISO_DIR)/boot/grub
OUTPUT    = $(BUILD_DIR)/whnos.bin

OBJS = $(BUILD_DIR)/boot.o       \
       $(BUILD_DIR)/isr_asm.o    \
       $(BUILD_DIR)/pci.o        \
       $(BUILD_DIR)/rtl8139.o    \
       $(BUILD_DIR)/e1000.o      \
       $(BUILD_DIR)/net.o        \
       $(BUILD_DIR)/kernel.o     \
       $(BUILD_DIR)/port.o       \
       $(BUILD_DIR)/vga.o        \
       $(BUILD_DIR)/serial.o     \
       $(BUILD_DIR)/gdt.o        \
       $(BUILD_DIR)/idt.o        \
       $(BUILD_DIR)/isr.o        \
       $(BUILD_DIR)/keyboard.o   \
       $(BUILD_DIR)/pit.o        \
       $(BUILD_DIR)/rtc.o        \
       $(BUILD_DIR)/cpu.o        \
       $(BUILD_DIR)/shell.o

all: iso

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 汇编
$(BUILD_DIR)/boot.o: boot/boot.asm | $(BUILD_DIR)
	$(ASM) -f elf32 -o $@ $<

$(BUILD_DIR)/isr_asm.o: kernel/isr_asm.asm | $(BUILD_DIR)
	$(ASM) -f elf32 -o $@ $<

# C++
$(BUILD_DIR)/kernel.o: kernel/kernel.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/port.o: kernel/port.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/vga.o: kernel/vga.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/serial.o: kernel/serial.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/gdt.o: kernel/gdt.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/idt.o: kernel/idt.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/isr.o: kernel/isr.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/keyboard.o: kernel/keyboard.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/shell.o: kernel/shell.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/pit.o: kernel/pit.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/rtc.o: kernel/rtc.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/cpu.o: kernel/cpu.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/pci.o: kernel/pci.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/rtl8139.o: kernel/rtl8139.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/e1000.o: kernel/e1000.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/net.o: kernel/net.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# 链接
$(OUTPUT): $(OBJS)
	$(LD) -T linker.ld -o $@ $^

# 制作 ISO
iso: $(OUTPUT)
	mkdir -p $(BOOT_DIR)
	cp $(OUTPUT) $(BOOT_DIR)
	cp grub.cfg $(BOOT_DIR)
	grub-mkrescue -o $(BUILD_DIR)/whnos.iso $(ISO_DIR) 2>/dev/null; \
	if [ $$? -ne 0 ]; then \
		xorriso -as mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot \
		-boot-load-size 4 -boot-info-table -o $(BUILD_DIR)/whnos.iso $(ISO_DIR); \
	fi

# QEMU 运行 (user 网络 + RTL8139 网卡)
run: iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/whnos.iso -m 64 -nic user,model=rtl8139 -display sdl

# 调试
debug: iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/whnos.iso -m 64 -s -S

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all iso run debug clean
