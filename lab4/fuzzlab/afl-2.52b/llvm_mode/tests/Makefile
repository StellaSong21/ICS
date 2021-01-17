SRCS = $(wildcard *.c)
PROGS = $(patsubst %.c,%,$(SRCS))
CC = ../../afl-clang-fast

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS)  -o $@ $<

clean:
	rm simple_branch
