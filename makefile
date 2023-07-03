CC = /usr/bin/gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined,leak

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,obj/%.o,$(SRCS))

all: main

.PHONY: main
main: $(OBJS)
	mkdir -p bin
	$(CC) -o bin/$@ $^ -fsanitize=undefined,leak,address


obj/%.o: src/%.c
	mkdir -p obj
	$(CC) $(CFLAGS) -c -o $@ $<


.PHONY: clean
clean:
	rm -rf ./bin ./obj
