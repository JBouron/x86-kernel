# Build and run the kernel. Targets:
# 	- all/default: Build the kernel in debug mode.
# 	- run: Build the kernel in debug mode and start it using qemu.
# 	- runs: Build the kernel in debug mode and start it using qemu. Qemu is
# 	starts with the -S flag, e.g. waiting for gdb to start the VM's execution.
# 	- debug: Build the kernel in debug mode with -O0 and -g.
# 	- release: Build the kernel in release mode with -O2
#	- baremetal_debug: Build the kernel in debug mode and create an .iso file
#	that is meant to be run on a physical machine.
#	- baremetal_release: Build the kernel in release mode and create an .iso
#	file that is meant to be run on a physical machine.
#	- clean: Remove all compilation artifacts.
# All compilation happens within a docker container which contains a
# cross-compiler. The docker image is automatically created by this make file if
# it does not already exist.

# The docker image containing the cross-compiler used to build the kernel.
DOCKER_IMAGE:=kernel_builder

# VM configuration:
# Number of virtual cpus to use in the VM.
VM_CPUS:=16
# Size of the VM's RAM in MiB.
VM_RAMSIZE=2048

# The compiler used to compile the kernel. The docker image contains the
# cross-compiler i686-gcc and assembler i686-as which are installed in the
# root's home directory.
CC=/root/opt/cross/bin/i686-elf-gcc

# The flags used to compile all the source files of the kernel.
# Note: The -Wno-array-bounds is necessary as some physical pointer arithmetic
# would trigger warning messages about an out-of-bounds access.
KERNEL_CFLAGS=-Wall -Wextra -Werror -ffreestanding -nostdlib -I./src \
	-static-libgcc -lgcc -Wno-array-bounds -Wno-unused-parameter
# The name of the linker script used to build the kernel image.
LINKER_SCRIPT=linker.ld

# Number of jobs for kernel compilation.
NJOBS:=$(shell lscpu | grep '^CPU(s):' | awk '{print $$2}')

# The name of the kernel image.
KERNEL_IMG_NAME=kernel.bin
# The name of the kernel static library used for testing.
KERNEL_LIB_NAME=libkernel.a

# The build directory that will contain all the object files.
BUILD_DIR=./build_dir
ISO_DIR=$(BUILD_DIR)/iso
SRC_DIR=./src

# Find all the source files recursively in the $(SRC_DIR) directory.
ASM_GCC_FILES:=$(shell find $(SRC_DIR) -type f -name "*.S")
SOURCE_FILES:=$(shell find $(SRC_DIR) -type f -name "*.c")
HEADER_FILES:=$(shell find $(SRC_DIR) -type f -name "*.h")
# This is the set of object files that will be used for the compilation. Note
# that the object files should be located in the $(BUILD_DIR), thus the
# substitution.
_OBJ_FILES:=$(SOURCE_FILES:.c=.o) $(ASM_GCC_FILES:.S=.o)
OBJ_FILES:=$(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(_OBJ_FILES))

.PHONY: clean build release debug

# Get rid of builtin rules. For some reasons, when compiling for baremetal, make
# tries to outsmart us with its builtin rules and tries to compile a .test.S
# file only to fail spectacularly.
.SUFFIXES:

all: debug

QEMU_OPTIONS=-smp $(VM_CPUS) -s -m $(VM_RAMSIZE) -no-shutdown -no-reboot \
			 -nographic  -enable-kvm

run: debug
	qemu-system-i386 -kernel $(BUILD_DIR)/$(KERNEL_IMG_NAME) $(QEMU_OPTIONS)

runs: debug
	qemu-system-i386 -kernel $(BUILD_DIR)/$(KERNEL_IMG_NAME) $(QEMU_OPTIONS) -S

release: CONT_RULE = release_in_cont
release: OUTPUT=SERIAL
release: build

debug: CONT_RULE = debug_in_cont
debug: OUTPUT=SERIAL
debug: build

baremetal_release: CONT_RULE = release_in_cont
baremetal_release: OUTPUT = VGA
baremetal_release: build create_iso

baremetal_debug: CONT_RULE = debug_in_cont
baremetal_debug: OUTPUT = VGA
baremetal_debug: build create_iso

# This is the main rule. This rule will start the builder docker container that
# will build the kernel for us.
build:
	[ "$$(sudo docker images $(DOCKER_IMAGE) | wc -l)" -gt 1 ] || sudo docker build -t $(DOCKER_IMAGE) ./docker/
	@# We link the current directory to the same point in the container so that
	@# debug info in the kernel image are correct. (Eg. the paths for the source
	@# files are the same).
	@# We use the -t to get colored output.
	@# The -r flag is of outmost importance: it turns out that not using -r
	@# (i.e. using implicit rules) the build will fail on .test.S files as it
	@# will not follow the .S rule below. This could be a `make` bug.
	sudo docker run -v $(PWD):$(PWD) -t $(DOCKER_IMAGE) make -r -C $(PWD) -j $(NJOBS) OUTPUT=$(OUTPUT) $(CONT_RULE)
	@# Since the user in the docker container is root, we need to change the
	@# owner once the build is complete.
	sudo chown $(USER):$(USER) $(BUILD_DIR) -R

create_iso: $(ISO_DIR) $(BUILD_DIR)/$(KERNEL_IMG_NAME)
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(BUILD_DIR)/$(KERNEL_IMG_NAME) $(ISO_DIR)/boot/kernel.bin
	@echo "menuentry \"mykernel\" { multiboot /boot/kernel.bin }" > $(ISO_DIR)/boot/grub/grub.cfg
	@grub-mkrescue -o $(BUILD_DIR)/kernel.iso $(ISO_DIR)

$(ISO_DIR): $(BUILD_DIR)
	@mkdir -p $(ISO_DIR)


release_in_cont: KERNEL_CFLAGS += -O2 -D$(OUTPUT)
release_in_cont: build_in_cont

debug_in_cont: KERNEL_CFLAGS += -O0 -g -DKMALLOC_DEBUG -D$(OUTPUT)
debug_in_cont: build_in_cont

# This rule is to be used *within* the builder docker container. It performs the
# following:
# 	0: Prepare the build directory.
# 	1: Compile the kernel image. 
# 	2: Compile the kernel lib.
build_in_cont: $(BUILD_DIR) $(BUILD_DIR)/$(KERNEL_IMG_NAME) $(BUILD_DIR)/$(KERNEL_LIB_NAME)

# Create the build directory and all subdirectories.
SUBDIRS_:=$(shell find ./src -type d)
SUBDIRS:=$(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(SUBDIRS_))
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(SUBDIRS)

# Kernel image compilation. The kernel image depends on *all* object files.
$(BUILD_DIR)/$(KERNEL_IMG_NAME): $(OBJ_FILES)
	$(CC) -T $(LINKER_SCRIPT) -o $@ $^ $(KERNEL_CFLAGS)

$(BUILD_DIR)/$(KERNEL_LIB_NAME): $(OBJ_FILES)
	@# We use a static library that we can link to an arbitrary executable to
	@# test functions.
	ar rcs $@ $^

# This rule is for .c files that have an associated .test file. This is because
# the .test is included in the .c and therefore any change to the .test should
# trigger a recompilation of the .c file.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.test $(HEADER_FILES)
	$(CC) -o $@ -c $< $(KERNEL_CFLAGS)

# Rule for .c file that do not have an associated .test file.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER_FILES)
	$(CC) -o $@ -c $< $(KERNEL_CFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	$(CC) -o $@ -c $< $(KERNEL_CFLAGS)

clean:
	rm -rf $(BUILD_DIR)
