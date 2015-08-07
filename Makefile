PROGRAM = pong
SOURCES = pong.c support.c

CFLAGS ?= -g -O2
CFLAGS += -Wall -Wextra -pedantic
LDLIBS += -pthread -lncurses

INSTALL     = install
INSTALL_BIN = $(INSTALL) -D -m 755

PREFIX = /usr/local

bin_dir = $(PREFIX)/bin

.PHONY: all
all: $(PROGRAM)

$(PROGRAM): $(SOURCES)

.PHONY: clean
clean:

.PHONY: clobber
clobber: clean
	$(RM) $(PROGRAM)

.PHONY: install
install: $(PROGRAM)
	$(INSTALL_BIN) $(PROGRAM) $(DESTDIR)$(bin_dir)/$(PROGRAM)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(bin_dir)/$(PROGRAM)
