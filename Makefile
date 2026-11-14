# Shay toolchain
# gshai = interpreter; gshaic = native object compiler.

CXX ?= c++
CC ?= cc
INSTALL ?= install

BUILD_DIR ?= build
INTERPRETER := $(BUILD_DIR)/gshai
COMPILER := $(BUILD_DIR)/gshaic
PROGRAM ?= examples/hello.shay

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?=

FRONTEND_SOURCES := src/lexer.cpp src/parser.cpp
FRONTEND_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(FRONTEND_SOURCES))
RUNTIME_OBJECT := $(BUILD_DIR)/runtime.o
INTERPRETER_OBJECT := $(BUILD_DIR)/gshai.o
COMPILER_OBJECT := $(BUILD_DIR)/gshaic.o
OBJECTS := $(FRONTEND_OBJECTS) $(RUNTIME_OBJECT) $(INTERPRETER_OBJECT) $(COMPILER_OBJECT)
DEPFILES := $(OBJECTS:.o=.d)

.PHONY: all build test check run compile clean install uninstall debug release help

all: build
build: $(INTERPRETER) $(COMPILER)

$(BUILD_DIR)/.dir:
	mkdir -p "$(BUILD_DIR)"
	touch "$@"

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)/.dir
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/runtime.o: src/runtime.c src/runtime.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(INTERPRETER): $(INTERPRETER_OBJECT) $(FRONTEND_OBJECTS) $(RUNTIME_OBJECT)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o "$@"

$(COMPILER): $(COMPILER_OBJECT) $(FRONTEND_OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o "$@"

-include $(DEPFILES)

test: build
	CXX="$(CXX)" sh tests/run.sh ./$(INTERPRETER) ./$(COMPILER)

check: $(INTERPRETER)
	./$(INTERPRETER) --check "$(PROGRAM)"

run: $(INTERPRETER)
	./$(INTERPRETER) "$(PROGRAM)"

compile: $(COMPILER)
	CXX="$(CXX)" ./$(COMPILER) -ob "$(PROGRAM)"

install: build
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 0755 "$(INTERPRETER)" "$(DESTDIR)$(BINDIR)/gshai"
	$(INSTALL) -m 0755 "$(COMPILER)" "$(DESTDIR)$(BINDIR)/gshaic"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/gshai" "$(DESTDIR)$(BINDIR)/gshaic"

debug:
	$(MAKE) BUILD_DIR=build/debug CXXFLAGS="-std=c++20 -O0 -g3 -Wall -Wextra -Wpedantic" CFLAGS="-std=c11 -O0 -g3 -Wall -Wextra -Wpedantic" build

release:
	$(MAKE) BUILD_DIR=build/release CXXFLAGS="-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic" CFLAGS="-std=c11 -O3 -DNDEBUG -Wall -Wextra -Wpedantic" build

clean:
	rm -rf build

help:
	@printf '%s\\n' \
	  'Shay toolchain' \
	  '  make                 Build interpreter (gshai) and compiler (gshaic)' \
	  '  make test            Build both tools and run regression tests' \
	  '  make check PROGRAM=examples/hello.shay' \
	  '  make run PROGRAM=examples/hello.shay' \
	  '  make compile PROGRAM=examples/hello.shay  # emits examples/hello.sho' \
	  '  make debug           Build debug versions' \
	  '  make release         Build optimized versions' \
	  '  make install         Install gshai and gshaic' \
	  '  make uninstall       Remove installed tools' \
	  '  make clean           Remove build outputs'
