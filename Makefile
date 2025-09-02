# ----------------------------------------------------------
CC       := /usr/bin/gcc
SRC      := movhex.c
BIN      := movhex

# directory containing test case input files
TEST_DIR := test

# Directory where results will be stored
RESULT_DIR := result

# Gather all .txt test files and deduce their base names (without extension)
TEST_FILES := $(wildcard $(TEST_DIR)/*.txt)
TEST_NAMES := $(basename $(notdir $(TEST_FILES)))

# Build (compila con le opzioni richieste, ignorando CFLAGS standard)
$(BIN): $(SRC)
	$(CC) -DEVAL -std=gnu11 -Wall -Werror -O2 -pipe -static -s -o $(BIN) $(SRC) -lm

# Ensure result directory exists before running any test
$(RESULT_DIR):
	mkdir -p $(RESULT_DIR)

.PHONY: $(TEST_NAMES)
# Esegue un singolo test: salva output e tempo/memoria, poi stampa un riepilogo
$(TEST_NAMES): %: $(BIN) $(RESULT_DIR) $(TEST_DIR)/%.txt
	@echo "Running test $*.txt"
	@/usr/bin/time -v ./$(BIN) < "$(TEST_DIR)/$*.txt" \
	 1> "$(RESULT_DIR)/$*_RESULT" \
	 2> "$(RESULT_DIR)/$*_TIME"
	@echo "---- summary ($*) -------------------------------------"
	@awk -F': ' '/User time|System time|Elapsed \\(wall clock\\) time|Maximum resident set size/ {print}' \
	 "$(RESULT_DIR)/$*_TIME"
	@echo "Output  -> $(RESULT_DIR)/$*_RESULT"
	@echo "Profile -> $(RESULT_DIR)/$*_TIME"
	@echo "--------------------------------------------------------"

.PHONY: test-all
# Esegue tutti i .txt in TEST_DIR con timing/memoria
test-all: $(BIN) $(RESULT_DIR)
	@for f in $(TEST_DIR)/*.txt; do \
	  name=$$(basename $$f .txt); \
	  echo "Running test $$name.txt"; \
	  /usr/bin/time -v ./$(BIN) < "$$f" \
	    1> "$(RESULT_DIR)/$${name}_RESULT" \
	    2> "$(RESULT_DIR)/$${name}_TIME"; \
	  echo "---- summary ($$name) --------------------------------"; \
	  awk -F': ' '/User time|System time|Elapsed \\(wall clock\\) time|Maximum resident set size/ {print}' \
	    "$(RESULT_DIR)/$${name}_TIME"; \
	  echo "Output  -> $(RESULT_DIR)/$${name}_RESULT"; \
	  echo "Profile -> $(RESULT_DIR)/$${name}_TIME"; \
	  echo "--------------------------------------------------------"; \
	done

.PHONY: clean
clean:
	$(RM) $(BIN)
