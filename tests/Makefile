# This is the top makefile for test programs and script.
# A .c test is composed of a single .c file with a main. When compiling a .c
# test the kernel static library is statically linked against the test program.
# Moreover the include path contains the /src directory of the kernel.
#
# This Makefile will compile the test programs and run them using the
# runtests.sh script.
# All of the above is accomplished simply using `make` in the test directory.

CC=gcc
CFLAGS=-I../src -L../build_dir -lkernel -m32 -g -O0 -fno-builtin

TEST_DIR:=.

C_TEST_FILES:=$(shell find $(TEST_DIR) -name "*.c")
# The test programs are compiled directly in their directory.
C_TEST_PROGS:=$(C_TEST_FILES:.c=)

# The default rule is to produce all test programs and run them
.PHONY: all clean
all: $(C_TEST_PROGS)
	@echo "Running all tests ..."
	@./runtests.sh $^

# Rule generating a test executable from its source file.
%: %.c
	$(CC) -o $@ $< $(CFLAGS)

clean:
	rm -rf $(C_TEST_PROGS)
