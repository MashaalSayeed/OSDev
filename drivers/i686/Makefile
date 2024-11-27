ARCH?=i686
CFLAGS?=-m32

DRIVER_SRC=$(wildcard *.c)
DRIVER_OBJ=$(DRIVER_SRC:%.c=../../build/$(ARCH)/drivers/%.o)

all: $(DRIVER_OBJ)

../../build/$(ARCH)/drivers/%.o: %.c
	@echo "Compiling driver source file $<..."
	$(CC) $(CFLAGS) -c $< -o $@