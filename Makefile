# Compiler and flags
CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS :=

# Directories
SRC_DIR := src
INC_DIR := include
EXAMPLES_DIR := examples
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj

# Target executables
TARGET := $(BIN_DIR)/matching_engine
DEMO_TARGET := $(BIN_DIR)/simulator_demo

# Source and object files
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# Demo source (separate from main)
DEMO_SOURCES := $(EXAMPLES_DIR)/simulator_demo.cpp
DEMO_OBJECTS := $(patsubst $(EXAMPLES_DIR)/%.cpp,$(OBJ_DIR)/examples_%.o,$(DEMO_SOURCES))

# Shared object subsets
MAIN_OBJECT := $(OBJ_DIR)/main.o
OBJECTS_NO_MAIN := $(filter-out $(MAIN_OBJECT),$(OBJECTS))

# Demo build toggle
BUILD_DEMO ?= 0
ifeq ($(BUILD_DEMO),1)
    FULL_TARGETS := $(TARGET) $(DEMO_TARGET)
else
    FULL_TARGETS := $(TARGET)
endif

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
MAGENTA := \033[0;35m
CYAN := \033[0;36m
NC := \033[0m # No Color

# Default target
.PHONY: all
all: $(TARGET)

# Build both main and demo
.PHONY: full
full: $(FULL_TARGETS)
ifeq ($(BUILD_DEMO),1)
	@echo "$(GREEN)✓ Full build complete (main + demo)!$(NC)"
else
	@echo "$(GREEN)✓ Full build complete (main only).$(NC)"
endif

# Create directories
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "$(BLUE)[$(BUILD_TYPE)]$(NC) Compiling $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile demo sources
$(OBJ_DIR)/examples_%.o: $(EXAMPLES_DIR)/%.cpp | $(OBJ_DIR)
	@echo "$(CYAN)[$(BUILD_TYPE)]$(NC) Compiling demo $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Link main executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "$(GREEN)[$(BUILD_TYPE)]$(NC) Linking $(TARGET)..."
	@$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "$(GREEN)✓ Build successful!$(NC)"

# Link demo executable
$(DEMO_TARGET): $(DEMO_OBJECTS) $(OBJECTS_NO_MAIN) | $(BIN_DIR)
	@echo "$(MAGENTA)[$(BUILD_TYPE)]$(NC) Linking simulator demo..."
	@$(CXX) $(DEMO_OBJECTS) $(OBJECTS_NO_MAIN) -o $(DEMO_TARGET) $(LDFLAGS)
	@echo "$(GREEN)✓ Demo build successful!$(NC)"

# Run the main program
.PHONY: run
run: $(TARGET)
	@echo "$(YELLOW)Running matching engine...$(NC)"
	@echo ""
	@$(TARGET)

# Build and run the simulator demo
.PHONY: demo
demo: $(DEMO_TARGET)
	@echo "$(CYAN)═══════════════════════════════════════════════════════$(NC)"
	@echo "$(CYAN)    Running Trading Simulator Demo$(NC)"
	@echo "$(CYAN)═══════════════════════════════════════════════════════$(NC)"
	@echo ""
	@$(DEMO_TARGET)

# Interactive demo menu
.PHONY: demo-interactive
demo-interactive: $(DEMO_TARGET)
	@echo "$(CYAN)Choose demo to run:$(NC)"
	@echo "  1) Momentum vs Mean Reversion (Multi-Strategy)"
	@echo "  2) Simple Backtest (Single Strategy)"
	@echo "  3) Both demos"
	@echo ""
	@read -p "Enter choice [1-3]: " choice; \
	$(DEMO_TARGET) $$choice

# Quick demo (runs both)
.PHONY: demo-quick
demo-quick: $(DEMO_TARGET)
	@$(DEMO_TARGET) 3

# Clean build artifacts
.PHONY: clean
clean:
	@echo "$(RED)Cleaning build artifacts...$(NC)"
	@rm -rf $(BUILD_DIR)
	@echo "$(GREEN)✓ Clean complete$(NC)"

# Clean and rebuild
.PHONY: rebuild
rebuild: clean all

# Clean and rebuild with demo
.PHONY: rebuild-full
rebuild-full: clean full

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
	@echo "  Demo Target: $(DEMO_TARGET)"
	@echo "  Sources:     $(words $(SOURCES)) files"
	@echo "  Objects:     $(words $(OBJECTS)) files"
	@echo ""
	@echo "$(CYAN)Available Targets:$(NC)"
	@echo "  - matching_engine  (main executable)"
	@echo "  - simulator_demo   (trading simulator)"

# List all source files
.PHONY: list-sources
list-sources:
	@echo "$(BLUE)Source Files:$(NC)"
	@for src in $(SOURCES); do echo "  $$src"; done
	@echo ""
	@echo "$(CYAN)Demo Files:$(NC)"
	@for src in $(DEMO_SOURCES); do echo "  $$src"; done

# Check if all required source files exist
.PHONY: check-sources
check-sources:
	@echo "$(YELLOW)Checking required source files...$(NC)"
	@missing=0; \
	for file in strategy.cpp strategies.cpp trading_simulator.cpp; do \
		if [ ! -f "$(SRC_DIR)/$$file" ]; then \
			echo "$(RED)✗ Missing: $(SRC_DIR)/$$file$(NC)"; \
			missing=$$((missing + 1)); \
		else \
			echo "$(GREEN)✓ Found: $(SRC_DIR)/$$file$(NC)"; \
		fi \
	done; \
	if [ ! -f "$(EXAMPLES_DIR)/simulator_demo.cpp" ]; then \
		echo "$(RED)✗ Missing: $(EXAMPLES_DIR)/simulator_demo.cpp$(NC)"; \
		missing=$$((missing + 1)); \
	else \
		echo "$(GREEN)✓ Found: $(EXAMPLES_DIR)/simulator_demo.cpp$(NC)"; \
	fi; \
	if [ $$missing -eq 0 ]; then \
		echo "$(GREEN)✓ All required files present!$(NC)"; \
	else \
		echo "$(RED)✗ Missing $$missing file(s)$(NC)"; \
		exit 1; \
	fi

# Install (optional)
.PHONY: install
install: $(TARGET)
ifeq ($(BUILD_DEMO),1)
	@$(MAKE) BUILD_DEMO=1 $(DEMO_TARGET)
endif
	@echo "$(GREEN)Installing to /usr/local/bin...$(NC)"
	@sudo cp $(TARGET) /usr/local/bin/
ifeq ($(BUILD_DEMO),1)
	@sudo cp $(DEMO_TARGET) /usr/local/bin/
endif
	@echo "$(GREEN)✓ Installation complete$(NC)"

# Create examples directory if it doesn't exist
.PHONY: setup-examples
setup-examples:
	@mkdir -p $(EXAMPLES_DIR)
	@echo "$(GREEN)✓ Examples directory created$(NC)"

# Help message
.PHONY: help
help:
	@echo "$(BLUE)═══════════════════════════════════════════════════════$(NC)"
	@echo "$(BLUE)         Matching Engine & Simulator Makefile$(NC)"
	@echo "$(BLUE)═══════════════════════════════════════════════════════$(NC)"
	@echo ""
	@echo "$(YELLOW)Basic Targets:$(NC)"
	@echo "  make              - Build matching engine (release mode)"
	@echo "  make full         - Build both engine and simulator demo"
	@echo "  make run          - Build and run matching engine"
	@echo "  make demo         - Build and run simulator demo (both demos)"
	@echo "  make demo-quick   - Quick run of all demos"
	@echo "  make demo-interactive - Interactive demo selection"
	@echo ""
	@echo "$(YELLOW)Build Modes:$(NC)"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make sanitize     - Build with address/undefined sanitizers"
	@echo "  make rebuild      - Clean and rebuild"
	@echo "  make rebuild-full - Clean and rebuild everything"
	@echo ""
	@echo "$(YELLOW)Utilities:$(NC)"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make info         - Show build configuration"
	@echo "  make list-sources - List all source files"
	@echo "  make check-sources - Verify all required files exist"
	@echo "  make install      - Install to /usr/local/bin"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "$(YELLOW)Build Options:$(NC)"
	@echo "  DEBUG=1        - Enable debug mode"
	@echo "  SANITIZE=1     - Enable sanitizers"
	@echo "  CXX=g++        - Use different compiler"
	@echo ""
	@echo "$(YELLOW)Examples:$(NC)"
	@echo "  make full              # Build everything"
	@echo "  make demo              # Run simulator demo"
	@echo "  make demo-interactive  # Choose which demo to run"
	@echo "  make run               # Run matching engine"
	@echo "  make debug demo        # Debug build + run demo"
	@echo "  make CXX=g++ full      # Use g++ for everything"
	@echo ""
	@echo "$(CYAN)Project Structure:$(NC)"
	@echo "  src/               - Source files"
	@echo "  include/           - Header files"
	@echo "  examples/          - Demo programs"
	@echo "  build/             - Build artifacts"
	@echo "    ├── bin/         - Executables"
	@echo "    └── obj/         - Object files"

# Progress indicator for long builds
.PHONY: progress
progress:
	@total=$(words $(SOURCES)); \
	current=0; \
	for src in $(SOURCES); do \
		current=$$((current + 1)); \
		echo -ne "Progress: $$current/$$total ($$((current * 100 / total))%)\r"; \
		sleep 0.1; \
	done; \
	echo ""

# Dependencies
-include $(OBJECTS:.o=.d)

# Generate dependency files
$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -MM -MT $(OBJ_DIR)/$*.o $< > $@

# Pretty print build summary
.PHONY: summary
summary: all
	@echo ""
	@echo "$(BLUE)═══════════════════════════════════════════════════════$(NC)"
	@echo "$(BLUE)                   Build Summary$(NC)"
	@echo "$(BLUE)═══════════════════════════════════════════════════════$(NC)"
	@echo ""
	@echo "$(GREEN)✓ Matching Engine:$(NC) $(TARGET)"
	@if [ -f "$(DEMO_TARGET)" ]; then \
		echo "$(GREEN)✓ Simulator Demo:$(NC)  $(DEMO_TARGET)"; \
	else \
		echo "$(YELLOW)⚠ Simulator Demo:$(NC)  Not built (run 'make full')"; \
	fi
	@echo ""
	@echo "$(CYAN)Quick Commands:$(NC)"
	@echo "  ./$(TARGET)       # Run matching engine"
	@if [ -f "$(DEMO_TARGET)" ]; then \
		echo "  ./$(DEMO_TARGET)  # Run simulator demo"; \
	fi
	@echo ""

# Phony targets to avoid conflicts with files
.PHONY: all full run demo demo-interactive demo-quick clean rebuild rebuild-full \
        debug sanitize info list-sources check-sources install setup-examples \
        help progress summary
