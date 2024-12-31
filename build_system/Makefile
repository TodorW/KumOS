ASM := nasm
GCC := gcc
QEMU := qemu-system-i386
GRUB := grub-mkrescue

ISO_NAME := iso/zephyros.iso
BUILD_DIR := build_system/bin
ISO_DIR := iso
BOOTLOADER_SRC := bootloader/bootloader.asm
KERNEL_SRC := $(wildcard kernel/*.c)
KERNEL_OBJ := $(patsubst kernel/%.c, $(BUILD_DIR)/%.o, $(KERNEL_SRC))

.PHONY: all clean run

all: $(ISO_NAME)

$(BUILD_DIR)/boot.bin: $(BOOTLOADER_SRC)
	@mkdir -p $(BUILD_DIR)
	$(ASM) -f bin $< -o $@

$(BUILD_DIR)/%.o: kernel/%.c
	@mkdir -p $(BUILD_DIR)
	$(GCC) -m32 -c $< -o $@

$(BUILD_DIR)/kernel.bin: $(KERNEL_OBJ)
	$(GCC) -m32 -o $@ $^

$(ISO_NAME): $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin
	@mkdir -p $(ISO_DIR)/boot/grub
	echo "set timeout=0" > $(ISO_DIR)/boot/grub/grub.cfg
	echo "set default=0" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo 'menuentry "Zephyr OS" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "    multiboot /boot/kernel.bin" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "    boot" >> $(ISO_DIR)/boot/grub/grub.cfg
	echo "}" >> $(ISO_DIR)/boot/grub/grub.cfg
	cp $(BUILD_DIR)/boot.bin $(ISO_DIR)/boot/
	cp $(BUILD_DIR)/kernel.bin $(ISO_DIR)/boot/
	$(GRUB) -o $@ $(ISO_DIR)

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_NAME)

run: $(ISO_NAME)
	$(QEMU) -cdrom $(ISO_NAME)
