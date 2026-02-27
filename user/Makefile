include ./Makefile.conf

# Find all directories in core/ that contain a Makefile
APP_DIRS := $(wildcard core/*/Makefile)
# Strip the "/Makefile" part to get the directory paths
APPS := $(patsubst %/Makefile,%,$(APP_DIRS))

LIB_C_SRCS := $(shell find lib -name '*.c' 2>/dev/null)
LIB_OBJS := $(patsubst lib/%.c,$(LIB_BUILD_DIR)/%.o,$(LIB_C_SRCS))

.PHONY: all clean $(APPS)

all: $(APPS)

$(CRT0): ./crt0.s | $(USER_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(AS) -f elf32 -o $@ $<

$(LIB_BUILD_DIR)/%.o: lib/%.c | $(USER_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_ARCHIVE): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	@if [ -n "$(LIB_OBJS)" ]; then \
		ar rcs $@ $^; \
	else \
		touch $@; \
	fi

# This rule runs 'make' inside each app directory
$(APPS): | $(CRT0) $(LIB_ARCHIVE) $(LIBSYS)
	@echo "--- Building $@ ---"
	@mkdir -p $(BIN_DIR)
	@$(MAKE) -C $@

clean:
	@for dir in $(APPS); do \
		$(MAKE) -C $$dir clean; \
	done