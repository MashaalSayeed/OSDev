CFLAGS?=

LIBC_SRC=$(wildcard *.c)
LIBC_OBJ=$(LIBC_SRC:%.c=../build/$(ARCH)/libc/%.o)

all: $(LIBC_OBJ)

../build/$(ARCH)/libc/%.o: %.c
	@echo "Compiling libc source file $<..."
	$(CC) $(CFLAGS) -c $< -o $@
