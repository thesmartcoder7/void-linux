CC          := cc
PKG_CONFIG  ?= pkg-config

TARGET      := minlogin
SRC         := src/main.c

CFLAGS      := -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS    += -D_DEFAULT_SOURCE -I.
CPPFLAGS    += $(shell $(PKG_CONFIG) --cflags x11 xrandr xft imlib2)
LDLIBS      += $(shell $(PKG_CONFIG) --libs x11 xrandr xft imlib2) -lpam

PREFIX      := /usr/local
BINDIR      := $(PREFIX)/bin

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SRC) config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET)

