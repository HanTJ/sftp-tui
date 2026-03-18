CC = gcc
CFLAGS = -Iinclude -Wall -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600
LDFLAGS = -lssh2 -lncursesw -lpthread -ltinfo

# Check for pkg-config
PKG_CONFIG := $(shell command -v pkg-config 2> /dev/null)

# OS detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS paths for Homebrew
    CFLAGS += -I/usr/local/opt/libssh2/include -I/opt/homebrew/opt/libssh2/include
    LDFLAGS += -L/usr/local/opt/libssh2/lib -L/opt/homebrew/opt/libssh2/lib
else
    ifdef PKG_CONFIG
        CFLAGS += $(shell pkg-config --cflags libssh2 ncursesw 2>/dev/null)
        # Combine existing LDFLAGS with pkg-config output
        LDFLAGS += $(shell pkg-config --libs libssh2 ncursesw 2>/dev/null)
    endif
endif

OBJDIR = obj
BINDIR = bin

SRC = src/main.c src/config.c src/sftp_client.c src/tui.c
OBJ = $(OBJDIR)/main.o $(OBJDIR)/config.o $(OBJDIR)/sftp_client.o $(OBJDIR)/tui.o

TEST_SRC = tests/test_unit.c src/config.c src/sftp_client.c
TEST_OBJ = $(OBJDIR)/test_unit.o $(OBJDIR)/config.o $(OBJDIR)/sftp_client.o

TARGET = $(BINDIR)/sftp-tui
TEST_TARGET = $(BINDIR)/test_unit

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BINDIR)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJ)
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_OBJ) -o $(TEST_TARGET) $(LDFLAGS)

$(OBJDIR)/main.o: src/main.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/config.o: src/config.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/sftp_client.o: src/sftp_client.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/tui.o: src/tui.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/test_unit.o: tests/test_unit.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean test
