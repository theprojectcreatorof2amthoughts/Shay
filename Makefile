# Shay toolchain
# gshai = compiler; shaii = interpreter.

CXX ?= c++
CC ?= cc
INSTALL ?= install
BUILD_DIR ?= build
INTERPRETER := $(BUILD_DIR)/shaii
COMPILER := $(BUILD_DIR)/gshai
SHUI := $(BUILD_DIR)/shui
PROGRAM ?= examples/hello.shay
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
SHAREDIR ?= $(PREFIX)/share/shay
DESTDIR ?=

CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?=

FRONTEND_SOURCES := src/lexer.cpp src/parser.cpp src/shui.cpp src/shuil.cpp
FRONTEND_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(FRONTEND_SOURCES))
RUNTIME_OBJECT := $(BUILD_DIR)/runtime.o
INTERPRETER_OBJECT := $(BUILD_DIR)/shaii.o
COMPILER_OBJECT := $(BUILD_DIR)/gshai.o
SHUI_OBJECT := $(BUILD_DIR)/shui_main.o
ifeq ($(OS),Windows_NT)
SHUI_PLATFORM_OBJECTS := $(BUILD_DIR)/shui_win32.o
SHUI_PLATFORM_LIBS := -luser32 -lgdi32
endif
DEPFILES := $(FRONTEND_OBJECTS:.o=.d) $(RUNTIME_OBJECT:.o=.d) $(INTERPRETER_OBJECT:.o=.d) $(COMPILER_OBJECT:.o=.d) $(SHUI_OBJECT:.o=.d) $(SHUI_PLATFORM_OBJECTS:.o=.d)

.PHONY: all build test check run compile executable clean install uninstall debug release help
all: build
build: $(INTERPRETER) $(COMPILER) $(SHUI)

$(BUILD_DIR)/.dir:
	mkdir -p "$(BUILD_DIR)"
	touch "$@"

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)/.dir
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/runtime.o: src/runtime.c src/runtime.h | $(BUILD_DIR)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(INTERPRETER): $(INTERPRETER_OBJECT) $(FRONTEND_OBJECTS) $(RUNTIME_OBJECT)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o "$@"

$(COMPILER): $(COMPILER_OBJECT) $(FRONTEND_OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o "$@"

$(SHUI): $(SHUI_OBJECT) $(FRONTEND_OBJECTS) $(SHUI_PLATFORM_OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) $(SHUI_PLATFORM_LIBS) -o "$@"

-include $(DEPFILES)

test: build
	CXX="$(CXX)" sh tests/run.sh ./$(INTERPRETER) ./$(COMPILER) ./$(SHUI)

check: $(INTERPRETER)
	./$(INTERPRETER) --check "$(PROGRAM)"

run: $(INTERPRETER)
	./$(INTERPRETER) "$(PROGRAM)"

compile: $(COMPILER)
	CXX="$(CXX)" ./$(COMPILER) -ob "$(PROGRAM)"

executable: $(COMPILER)
	CXX="$(CXX)" ./$(COMPILER) -o "$(PROGRAM)"

install: build
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 0755 "$(COMPILER)" "$(DESTDIR)$(BINDIR)/gshai"
	$(INSTALL) -m 0755 "$(SHUI)" "$(DESTDIR)$(BINDIR)/shui"
	$(INSTALL) -m 0755 "$(INTERPRETER)" "$(DESTDIR)$(BINDIR)/shaii"
	$(INSTALL) -d "$(DESTDIR)$(SHAREDIR)/siil"
	$(INSTALL) -m 0644 siil/*.shaii "$(DESTDIR)$(SHAREDIR)/siil/"
	$(INSTALL) -m 0644 siil/README.md "$(DESTDIR)$(SHAREDIR)/siil/README.md"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/gshai" "$(DESTDIR)$(BINDIR)/shaii" "$(DESTDIR)$(BINDIR)/shui"
	@for lib in siil/*.shaii; do rm -f "$(DESTDIR)$(SHAREDIR)/siil/${lib##*/}"; done
	rm -f "$(DESTDIR)$(SHAREDIR)/siil/README.md"
	-rmdir "$(DESTDIR)$(SHAREDIR)/siil" "$(DESTDIR)$(SHAREDIR)"

debug:
	$(MAKE) BUILD_DIR=build/debug CXXFLAGS="-std=c++20 -O0 -g3 -Wall -Wextra -Wpedantic" CFLAGS="-std=c11 -O0 -g3 -Wall -Wextra -Wpedantic" build
release:
	$(MAKE) BUILD_DIR=build/release CXXFLAGS="-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic" CFLAGS="-std=c11 -O3 -DNDEBUG -Wall -Wextra -Wpedantic" build

clean:
	rm -rf build

help:
	@printf '%s\n' \
	  'Shay toolchain' \
	  '  make                 Build compiler (gshai), interpreter (shaii), and UI tool (shui)' \
	  '  make test            Run compiler/interpreter regression tests' \
	  '  make check PROGRAM=examples/hello.shay' \
	  '  make run PROGRAM=examples/hello.shay' \
	  '  make compile PROGRAM=examples/hello.shay  # emits .sho' \
	  '  make executable PROGRAM=examples/hello.shay  # emits executable' \
	  '  make install         Install gshai and shaii' \
	  '  make clean           Remove build outputs'
