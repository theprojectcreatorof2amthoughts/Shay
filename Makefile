CXX ?= c++
CC ?= cc
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
BUILD := build
TARGET := $(BUILD)/gshai

.PHONY: all clean test
all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/runtime.o: src/runtime.c src/runtime.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): src/gshai.cpp src/lexer.cpp src/parser.cpp src/ast.hpp src/lexer.hpp src/parser.hpp $(BUILD)/runtime.o | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc src/gshai.cpp src/lexer.cpp src/parser.cpp $(BUILD)/runtime.o -o $@

test: $(TARGET)
	sh tests/run.sh ./$(TARGET)

clean:
	rm -rf $(BUILD)
