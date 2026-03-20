include ../../Makefile.conf

APP          := hello
APP_BUILD_DIR := $(USER_BUILD_ROOT)/$(APP)
APP_BIN      := $(BIN_DIR)/HELLO

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,$(APP_BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

ifdef USE_MUSL
# Musl build — use musl headers, crt1/crti/crtn, link against musl libc
CFLAGS  := $(MUSL_CFLAGS)
LDFLAGS := $(MUSL_LDFLAGS)

$(APP_BIN): $(OBJS) | $(APP_BUILD_DIR)
	$(CC) $(LDFLAGS) \
		$(MUSL_CRT1) $(MUSL_CRTI) \
		$(OBJS) \
		$(MUSL_LIBC) \
		$(MUSL_CRTN) \
		-o $@
else
# Standard build — use your own crt0 + libuser
$(APP_BIN): $(OBJS) | $(APP_BUILD_DIR)
	$(CC) $(LDFLAGS) $(CRT0) $(OBJS) $(LIB_ARCHIVE) $(LIBSYS) -o $@
endif

$(APP_BUILD_DIR)/%.o: %.c | $(APP_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(APP_BUILD_DIR):
	@mkdir -p $@

clean:
	rm -rf $(APP_BUILD_DIR)

-include $(DEPS)