include ../../Makefile.conf

APP = guitest
APP_BUILD_DIR := $(USER_BUILD_ROOT)/$(APP)
APP_BIN := $(BIN_DIR)/$(APP)

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,$(APP_BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

all: $(APP_BIN)

$(APP_BIN): $(OBJS)
	$(CC) $(LDFLAGS) $(CRT0) $(OBJS) $(LIB_ARCHIVE) $(LIBSYS) -o $@

$(APP_BUILD_DIR)/%.o: %.c | $(APP_BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(APP_BUILD_DIR):
	@mkdir -p $@

clean:
	rm -rf $(APP_BUILD_DIR)

-include $(DEPS)
