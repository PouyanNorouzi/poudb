# Project Settings
TARGET = poudb
CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -MMD
LDFLAGS =
SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin

# Test Settings
TEST_DIR = test
TEST_LDFLAGS = -lcriterion

# Source and Object Files
SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/db/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

# Default Target
all: $(BIN_DIR)/$(TARGET)

# Link the executable
$(BIN_DIR)/$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ -o $@

# Compile source files and generate dependency files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Include auto-generated dependency files
-include $(DEPS)

# Create build output directories
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@ $(OBJ_DIR)/db

# Test client targets
test_client: $(BIN_DIR)/client

$(BIN_DIR)/client: $(TEST_DIR)/client.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@
	@echo "Test client built successfully. Run with './$(BIN_DIR)/client'"

# Unit test targets (Criterion)
# Filter out main.o to avoid duplicate main symbol
TEST_OBJS := $(filter-out $(OBJ_DIR)/main.o, $(OBJS))

test: test_parser test_operations test_persistence
	@echo "All tests completed."

test_parser: $(BIN_DIR)/test_parser
	@echo "Running parser tests..."
	./$(BIN_DIR)/test_parser

test_operations: $(BIN_DIR)/test_operations
	@echo "Running operations tests..."
	./$(BIN_DIR)/test_operations

test_persistence: $(BIN_DIR)/test_persistence
	@echo "Running persistence tests..."
	./$(BIN_DIR)/test_persistence

$(BIN_DIR)/test_parser: $(TEST_DIR)/test_parser.c $(TEST_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(TEST_OBJS) $(TEST_LDFLAGS) -o $@
	@echo "Parser tests built successfully."

$(BIN_DIR)/test_operations: $(TEST_DIR)/test_operations.c $(TEST_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(TEST_OBJS) $(TEST_LDFLAGS) -o $@
	@echo "Operations tests built successfully."

$(BIN_DIR)/test_persistence: $(TEST_DIR)/test_persistence.c $(TEST_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(TEST_OBJS) $(TEST_LDFLAGS) -o $@
	@echo "Persistence tests built successfully."

# Run commands
run_server: all
	@echo "Starting server..."
	./$(BIN_DIR)/$(TARGET)

run_client: test_client
	@echo "Starting client..."
	./$(BIN_DIR)/client

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean test test_client run_server run_client run_test test_persistence
