
# ----------------------------------------------------------
CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -pedantic

SRC     := movhex.c
BIN     := movhex

# directory containing test case input files
TEST_DIR := test

# Directory where results will be stored
RESULT_DIR := result

# Gather all .txt test files and deduce their base names (without extension)
TEST_FILES := $(wildcard $(TEST_DIR)/*.txt)

# Base names (edge_cases, ecc.)
TEST_NAMES := $(basename $(notdir $(TEST_FILES)))

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

# Ensure result directory exists before running any test
$(RESULT_DIR):
	mkdir -p $(RESULT_DIR)

# Each base name becomes a phony target that builds the binary (if needed)
# e poi esegue il test corrispondente
.PHONY: $(TEST_NAMES)
$(TEST_NAMES): %: $(BIN) $(RESULT_DIR) $(TEST_DIR)/%.txt
	@echo "Running test $*.txt"
	@./$(BIN) < $(TEST_DIR)/$*.txt | tee $(RESULT_DIR)/$*_RESULT
	@echo "--------------------------------------------------"

# Run all .txt test cases in $(TEST_DIR)
.PHONY: test-all
test-all: $(BIN) $(RESULT_DIR)
	@for f in $(TEST_DIR)/*.txt; do \
		name=$$(basename $$f .txt); \
		echo "Running test $$name.txt"; \
		./$(BIN) < $$f | tee $(RESULT_DIR)/$${name}_RESULT; \
		echo "--------------------------------------------------"; \
	done

.PHONY: clean
clean:
	$(RM) $(BIN)