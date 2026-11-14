CC ?= cc
CXX ?= c++
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
BUILD := build
RUNTIME_OBJ := $(BUILD)/shay_runtime.o
DRIVER := shay

.PHONY: all clean test
all: $(DRIVER)

$(BUILD):
	mkdir -p $@

$(RUNTIME_OBJ): src/runtime/shay_runtime.c src/runtime/shay_runtime.h | $(BUILD)
	$(CC) $(CFLAGS) -Isrc/runtime -c $< -o $@

$(DRIVER): src/frontend/main.cpp $(RUNTIME_OBJ) | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc/runtime $< $(RUNTIME_OBJ) -o $@

test: $(DRIVER)
	sh tests/smoke.sh

clean:
	rm -rf $(BUILD) $(DRIVER)
