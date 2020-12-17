# Makefile for gcc on Cygwin

WINSDKDIR = /cygdrive/c/Program Files (x86)/Windows Kits/10/Include
WINSDKINC = "$(shell find "$(WINSDKDIR)" -name WinHvPlatform.h -printf "%h\n" | tail -n 1)"

TARGET = main.exe
CC = gcc
CFLAGS = -I. -idirafter$(WINSDKINC) -Wall -Werror -Wno-parentheses
LDFLAGS = -L/cygdrive/c/Windows/System32 -lWinHvPlatform
SRCS = $(shell ls *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
INCS = $(shell ls *.h)

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

-include *.d

clean:
	rm -f $(TARGET) *.o
