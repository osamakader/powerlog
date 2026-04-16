# Power Consumption Logger
# Requires: Linux with sysfs

CC     ?= gcc
# -MMD/-MP: emit .d files so rebuilds run when headers change, not only .c files
CFLAGS ?= -Wall -Wextra -O2 -I include -MMD -MP
LDFLAGS =

SRCDIR = src
SRCS   = $(SRCDIR)/main.c $(SRCDIR)/common.c $(SRCDIR)/cpufreq.c \
         $(SRCDIR)/cpuidle.c $(SRCDIR)/thermal.c $(SRCDIR)/battery.c \
         $(SRCDIR)/regulator.c $(SRCDIR)/alerts.c
OBJS   = $(SRCS:.c=.o)
DEPS   = $(OBJS:.o=.d)
TARGET = powerlog

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/

# After `all` so the default goal stays `all`, not a rule from a .d file
-include $(DEPS)
