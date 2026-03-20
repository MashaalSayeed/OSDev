include ./Makefile.conf

# Split apps into musl and non-musl
MUSL_APP_DIRS := $(wildcard core/*/USE_MUSL)
MUSL_APPS     := $(patsubst %/USE_MUSL,%,$(MUSL_APP_DIRS))

STD_APP_DIRS  := $(wildcard core/*/Makefile)
STD_APPS      := $(filter-out $(MUSL_APPS),$(patsubst %/Makefile,%,$(STD_APP_DIRS)))

LIB_C_SRCS := $(shell find lib -name '*.c' 2>/dev/null)
LIB_OBJS   := $(patsubst lib/%.c,$(LIB_BUILD_DIR)/%.o,$(LIB_C_SRCS))

.PHONY: all clean $(STD_APPS) $(MUSL_APPS)

all: $(STD_APPS) $(MUSL_APPS)

$(USER_BUILD_ROOT):
	@mkdir -p $@

# crt0 and libuser for non-musl apps
$(CRT0): ./crt0.s | $(USER_BUILD_ROOT)
	@mkdir -p $(dir $@)
	$(AS) -f elf32 -o $@ $<

$(LIB_BUILD_DIR)/%.o: lib/%.c | $(USER_BUILD_ROOT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_ARCHIVE): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	@if [ -n "$(LIB_OBJS)" ]; then ar rcs $@ $^; else touch $@; fi

# Standard apps use crt0 + libuser
$(STD_APPS): | $(CRT0) $(LIB_ARCHIVE) $(LIBSYS)
	@echo "--- Building $@ ---"
	@mkdir -p $(BIN_DIR)
	@$(MAKE) -C $@

# Musl apps use musl's crt1/crti/crtn
$(MUSL_APPS):
	@echo "--- Building $@ (musl) ---"
	@mkdir -p $(BIN_DIR)
	@$(MAKE) -C $@ USE_MUSL=1

clean:
	@for dir in $(STD_APPS) $(MUSL_APPS); do $(MAKE) -C $$dir clean; done
	rm -rf $(USER_BUILD_ROOT)