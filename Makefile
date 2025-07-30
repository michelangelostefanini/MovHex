
# ----------------------------------------------------------
CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -pedantic

SRC     := movhex.c
BIN     := movhex

TEST_FILES := $(wildcard tests/*.txt)

TEST_TARGETS := $(patsubst tests/%.txt,run-%,$(TEST_FILES))

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: test
test: $(BIN) $(TEST_TARGETS)

.PHONY: $(TEST_TARGETS)
run-%: $(BIN) tests/%.txt
	@echo "Running test $*.txt"
	@./$(BIN) < tests/$*.txt
	@echo "--------------------------------------------------"

.PHONY: clean
clean:
	$(RM) $(BIN)