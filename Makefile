CC       ?= cc
CFLAGS   ?= -std=c11 -D_POSIX_C_SOURCE=200809L -O3 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS  ?=

SRC_DIR  := src
BUILD_DIR:= build
BIN      := streamstat

SOURCES  := $(SRC_DIR)/main.c $(SRC_DIR)/arena.c $(SRC_DIR)/csv_parser.c $(SRC_DIR)/stats.c
OBJECTS  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean gen_test_data test

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

gen_test_data: tools/gen_test_csv.c
	$(CC) -std=c11 -O2 -Wall -o tools/gen_test_csv tools/gen_test_csv.c

# Quick correctness smoke test against a tiny, hand-checkable CSV.
test: all
	./$(BIN) tests/sample.csv 2 3 , --header

clean:
	rm -rf $(BUILD_DIR) $(BIN) tools/gen_test_csv
