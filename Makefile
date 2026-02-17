# Power Consumption Logger
# Requires: Linux with sysfs

CC     ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -I include
LDFLAGS =

SRCDIR = src
SRCS   = $(SRCDIR)/main.c $(SRCDIR)/common.c $(SRCDIR)/cpufreq.c \
         $(SRCDIR)/cpuidle.c $(SRCDIR)/thermal.c $(SRCDIR)/battery.c \
         $(SRCDIR)/regulator.c
OBJS   = $(SRCS:.c=.o)
TARGET = powerlog

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/
