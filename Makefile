CC       ?= gcc
CPPFLAGS += -Iinclude
CFLAGS   ?= -Wall -Wextra -O2
LDFLAGS  ?=
LDLIBS   ?=

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
TARGET   ?= powerlog

SRCDIR := src
SRCS   := $(sort $(wildcard $(SRCDIR)/*.c))
OBJS   := $(SRCS:.c=.o)
DEPS   := $(OBJS:.o=.d)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

clean:
	$(RM) $(OBJS) $(DEPS) $(TARGET)

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"

-include $(DEPS)
