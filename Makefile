# Compiler and flags
CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS :=

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj

# Target executable
TARGET := $(BIN_DIR)/matching_engine

# Source and object files
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# Build modes
DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g -O0 -DDEBUG
    BUILD_TYPE := Debug
else
    CXXFLAGS += -O3 -march=native -DNDEBUG
    BUILD_TYPE := Release
endif

# Sanitizers (optional)
SANITIZE ?= 0
ifeq ($(SANITIZE), 1)
    CXXFLAGS += -fsanitize=address,undefined
    LDFLAGS += -fsanitize=address,undefined
endif

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[0;33m
BLUE := \033[0;34m
NC := \033[0m # No Color

# Default target
.PHONY: all
all: $(TARGET)

# Create directories
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "$(BLUE)[$(BUILD_TYPE)]$(NC) Compiling $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "$(GREEN)[$(BUILD_TYPE)]$(NC) Linking $(TARGET)..."
	@$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "$(GREEN)✓ Build successful!$(NC)"

# Run the program
.PHONY: run
run: $(TARGET)
	@echo "$(YELLOW)Running matching engine...$(NC)"
	@echo ""
	@$(TARGET)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "$(RED)Cleaning build artifacts...$(NC)"
	@rm -rf $(BUILD_DIR)
	@echo "$(GREEN)✓ Clean complete$(NC)"

# Build with debug symbols
.PHONY: debug
debug:
	@$(MAKE) DEBUG=1

# Build with sanitizers
.PHONY: sanitize
sanitize:
	@$(MAKE) DEBUG=1 SANITIZE=1

# Show build configuration
.PHONY: info
info:
	@echo "$(BLUE)Build Configuration:$(NC)"
	@echo "  Compiler:    $(CXX)"
	@echo "  Flags:       $(CXXFLAGS)"
	@echo "  Build Type:  $(BUILD_TYPE)"
	@echo "  Target:      $(TARGET)"
	@echo "  Sources:     $(words $(SOURCES)) files"
	@echo "  Objects:     $(words $(OBJECTS)) files"

# Install (optional)
.PHONY: install
install: $(TARGET)
	@echo "$(GREEN)Installing to /usr/local/bin...$(NC)"
	@sudo cp $(TARGET) /usr/local/bin/
	@echo "$(GREEN)✓ Installation complete$(NC)"

# Help message
.PHONY: help
help:
	@echo "$(BLUE)Matching Engine Makefile$(NC)"
	@echo ""
	@echo "$(YELLOW)Usage:$(NC)"
	@echo "  make           - Build the project (release mode)"
	@echo "  make run       - Build and run the executable"
	@echo "  make debug     - Build with debug symbols"
	@echo "  make sanitize  - Build with address/undefined sanitizers"
	@echo "  make clean     - Remove build artifacts"
	@echo "  make info      - Show build configuration"
	@echo "  make install   - Install to /usr/local/bin"
	@echo "  make help      - Show this help message"
	@echo ""
	@echo "$(YELLOW)Options:$(NC)"
	@echo "  DEBUG=1        - Enable debug mode"
	@echo "  SANITIZE=1     - Enable sanitizers"
	@echo "  CXX=g++        - Use different compiler"
	@echo ""
	@echo "$(YELLOW)Examples:$(NC)"
	@echo "  make                    # Release build"
	@echo "  make run                # Build and run"
	@echo "  make debug run          # Debug build and run"
	@echo "  make CXX=g++ DEBUG=1    # Use g++ with debug"

# Dependencies
-include $(OBJECTS:.o=.d)

# Generate dependency files
$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -MM -MT $(OBJ_DIR)/$*.o $< > $@