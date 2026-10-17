CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude
BUILD    := build

LIB_SRC  := src/Account.cpp src/Bank.cpp
LIB_OBJ  := $(LIB_SRC:src/%.cpp=$(BUILD)/%.o)

all: $(BUILD)/bank

$(BUILD)/bank: $(LIB_OBJ) $(BUILD)/main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/test_bank: $(LIB_OBJ) tests/test_bank.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: $(BUILD)/test_bank
	./$(BUILD)/test_bank

run: all
	./$(BUILD)/bank

clean:
	rm -rf $(BUILD)

.PHONY: all test run clean
