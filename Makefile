# The compiler used to compile the kernel. We choose GNU gcc.
CC=gcc
# The flags used to compile all the source files of the kernel.
KERNEL_CFLAGS=-m32 -O0 -g -Wall -Wextra -Werror -ffreestanding -nostdlib -lgcc \
	-I./src
# Flags used by the GNU as assembler. For now we only need the --32 for 32bits.
AS_FLAGS=--32
# The name of the linker script used to build the kernel image.
LINKER_SCRIPT=linker.ld

# The name of the kernel image and ISO file.
KERNEL_IMG_NAME=kernel.bin
KERNEL_ISO_NAME=kernel.iso

# The build directory that will contain all the object files.
BUILD_DIR=./build
ISO_DIR=$(BUILD_DIR)/iso
SRC_DIR=./src

# Find all the source files recursively in the $(SRC_DIR) directory.
ASM_FILES:=$(shell find $(SRC_DIR) -type f -name "*.s")
SOURCE_FILES:=$(shell find $(SRC_DIR) -type f -name "*.c")
HEADER_FILES:=$(shell find $(SRC_DIR) -type f -name "*.h")
# This is the set of object files that will be used for the compilation. Note
# that the object files should be located in the $(BUILD_DIR), thus the
# substitution.
_OBJ_FILES:=$(SOURCE_FILES:.c=.o) $(ASM_FILES:.s=.o)
OBJ_FILES:=$(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(_OBJ_FILES))

# The main rule:
# 	0: Prepare the build directory.
# 	1: Compile the kernel image. 
# 	(2: Create the ISO.) Not anymore but still available.
all: $(BUILD_DIR) $(BUILD_DIR)/$(KERNEL_IMG_NAME)

# Create the build directory and all subdirectories.
SUBDIRS_:=$(shell find ./src -type d)
SUBDIRS:=$(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(SUBDIRS_))
$(BUILD_DIR):
	@mkdir -p $(SUBDIRS)

# Kernel image compilation. The kernel image depends on *all* object files.
$(BUILD_DIR)/$(KERNEL_IMG_NAME): $(OBJ_FILES)
	$(CC) -T $(LINKER_SCRIPT) -o $@ $(KERNEL_CFLAGS) $^

# Given a .o under the $(BUILD_DIR), it should depend on the *.c, or *.s, in the
# $(SRC_DIR).
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER_FILES)
	$(CC) -o $@ -c $< $(KERNEL_CFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	as $(AS_FLAGS) -o $@ $<

# Create the disk image containing the kernel.
$(KERNEL_ISO_NAME): $(BUILD_DIR)/$(KERNEL_IMG_NAME)
	@mkdir -p $(ISO_DIR)/boot/grub
	@# Copy the kernel image into the root of the ISO.
	@cp $< $(ISO_DIR)/boot/$(KERNEL_IMG_NAME)
	@# Prepare the grub entry for the kernel.
	@echo menuentry "ktzkernel" { > $(ISO_DIR)/boot/grub/grub.cfg
	@echo multiboot /boot/kernel.bin >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo } >> $(ISO_DIR)/boot/grub/grub.cfg
	@# Create the ISO disk image.
	grub-mkrescue -o $@ $(ISO_DIR)

.PHONY: clean
clean:
	rm -rf ./build
	rm -rf $(KERNEL_ISO_NAME)
