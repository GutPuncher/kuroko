CFLAGS = -g -Wall -Wextra -pedantic -Wno-unused-parameter
OBJS = $(patsubst %.c, %.o, $(sort $(wildcard *.c)))
TARGET = kuroko

all: ${TARGET}

kuroko: ${OBJS}

.PHONY: clean
clean:
	@rm -f ${OBJS} ${TARGET}

tags: $(wildcard *.c) $(wildcard *.h)
	@ctags --c-kinds=+lx *.c *.h
