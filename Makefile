CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -pthread -Isrc
BIN_DIR ?= bin

SERVER_DB_SRCS = src/server/db/db.c src/server/db/password.c
SERVER_SRCS = src/server/main.c $(SERVER_DB_SRCS)
CLIENT_SRCS = src/client/main.c
TEST_DB_SRCS = tests/unit/test_db.c $(SERVER_DB_SRCS)

.PHONY: all test coverage clean

all: $(BIN_DIR)/server $(BIN_DIR)/client $(BIN_DIR)/test_db

$(BIN_DIR)/server: $(SERVER_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SERVER_SRCS) -lpq

$(BIN_DIR)/client: $(CLIENT_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CLIENT_SRCS)

$(BIN_DIR)/test_db: $(TEST_DB_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(TEST_DB_SRCS) -lpq

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
