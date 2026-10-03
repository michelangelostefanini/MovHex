CC = cc
CFLAGS ?= -O2 -std=gnu11 -Wall -Wextra
LDLIBS = -lm

.PHONY: all test test-all sanitize clean
all: movhex

movhex: movhex.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

test: movhex
	python3 tests/run_tests.py ./movhex

test-all: test

sanitize:
	$(CC) $(CPPFLAGS) -std=gnu11 -Wall -Wextra -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -o movhex-sanitize movhex.c $(LDLIBS)
	python3 tests/run_tests.py ./movhex-sanitize

clean:
	$(RM) movhex movhex-sanitize
