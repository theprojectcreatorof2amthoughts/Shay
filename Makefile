# Shay / gshai build
# Quick start: make && make test
# Run the example: make run
# Check another source file: make check PROGRAM=path/to/file.shay

CXX ?= c++
CC ?= cc
INSTALL ?= install

BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/gshai
PROGRAM ?= examples/hello.shay
ARGS ?=

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?=

CXX_SOURCES := src/gshai.cpp src/lexer.cpp src/parser.cpp
CXX_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(CXX_SOURCES))
C_OBJECTS := $(BUILD_DIR)/runtime.o
OBJECTS := $(CXX_OBJECTS) $(C_OBJECTS)
DEPFILES := $(OBJECTS:.o=.d)

.PHONY: all build test check run clean install uninstall debug release help

all: build

build: $(TARGET)

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/runtime.o: src/runtime.c src/runtime.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o "$@"

-include $(DEPFILES)

test: $(TARGET)
	sh tests/run.sh ./$(TARGET)

check: $(TARGET)
	@test -n "$(PROGRAM)" || { echo "Set PROGRAM=path/to/file.shay" >&2; exit 2; }
	./$(TARGET) --check "$(PROGRAM)"

run: $(TARGET)
	./$(TARGET) "$(PROGRAM)" $(ARGS)

install: $(TARGET)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/gshai"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/gshai"

debug:
	$(MAKE) BUILD_DIR=build/debug CXXFLAGS="-std=c++20 -O0 -g3 -Wall -Wextra -Wpedantic" CFLAGS="-std=c11 -O0 -g3 -Wall -Wextra -Wpedantic" build

release:
	$(MAKE) BUILD_DIR=build/release CXXFLAGS="-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic" CFLAGS="-std=c11 -O3 -DNDEBUG -Wall -Wextra -Wpedantic" build

clean:
	rm -rf build

help:
	@printf '%s\\n' \
	  'Shay / gshai Makefile' \
	  '  make                 Build gshai' \
	  '  make test            Build and run regression tests' \
	  '  make check PROGRAM=examples/hello.shay' \
	  '  make run PROGRAM=examples/hello.shay' \
	  '  make debug           Build a debug binary in build/debug' \
	  '  make release         Build an optimized binary in build/release' \
	  '  make install         Install gshai under PREFIX (default /usr/local)' \
	  '  make uninstall       Remove the installed gshai' \
	  '  make clean           Remove build outputs'
