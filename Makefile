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

$(TARGET): src/gshai.cpp $(BUILD)/runtime.o | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc $^ -o $@

test: $(TARGET)
	$(TARGET) --check examples/hello.shay
	$(TARGET) examples/hello.shay

clean:
	rm -rf $(BUILD)
