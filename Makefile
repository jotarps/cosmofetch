CC      ?= cc
CFLAGS  ?= -O2 -g0
WARN    := -Wall -Wextra -Wno-unused-parameter
CFLAGS  += -std=gnu11 -D_GNU_SOURCE $(WARN) -MMD -MP
LDLIBS  += -lm

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/cosmofetch

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
DEP := $(OBJ:.o=.d)
BIN := cosmofetch

.PHONY: all clean install uninstall debug test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

# build com AddressSanitizer + UBSan, util para desenvolvimento
debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined" all

test: all
	sh tests/smoke.sh ./$(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -Dm644 config/config.example.conf $(DESTDIR)$(DATADIR)/config.example.conf

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	rm -rf $(DESTDIR)$(DATADIR)

clean:
	rm -rf build $(BIN)

-include $(DEP)
