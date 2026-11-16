CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -pthread
BIN_DIR ?= bin

.PHONY: all test coverage clean

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(BIN_DIR)/server: src/server/main.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lpq

$(BIN_DIR)/client: src/client/main.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(BIN_DIR):
	mkdir -p $@

test: all
	sh tests/unit/test_main.sh "$(BIN_DIR)"

coverage:
	$(RM) -r build/coverage
	$(MAKE) BIN_DIR=build/coverage CFLAGS="$(CFLAGS) --coverage" test
	gcovr --filter src/ build/coverage

clean:
	$(RM) -r bin build
