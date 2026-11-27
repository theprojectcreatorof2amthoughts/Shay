# Shay toolchain
# gshai = compiler; shaii = interpreter.

CXX ?= c++
CC ?= cc
INSTALL ?= install
BUILD_DIR ?= build
INTERPRETER := $(BUILD_DIR)/shaii
COMPILER := $(BUILD_DIR)/gshai
ENTRY_PACKER := $(BUILD_DIR)/shentrypak
PROGRAM ?= examples/hello.shay
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
SHAREDIR ?= $(PREFIX)/share/shay
DESTDIR ?=
SHAY_HOME ?= $(HOME)/.shay
ENTRY_HOME ?= $(SHAY_HOME)/entries
ENTRY_DIR ?= $(word 2,$(MAKECMDGOALS))

CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?=

FRONTEND_SOURCES := src/lexer.cpp src/parser.cpp src/shui_language.cpp
FRONTEND_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(FRONTEND_SOURCES))
RUNTIME_OBJECT := $(BUILD_DIR)/runtime.o
INTERPRETER_OBJECT := $(BUILD_DIR)/shaii.o
COMPILER_OBJECT := $(BUILD_DIR)/gshai.o
DEPFILES := $(FRONTEND_OBJECTS:.o=.d) $(RUNTIME_OBJECT:.o=.d) $(INTERPRETER_OBJECT:.o=.d) $(COMPILER_OBJECT:.o=.d)

.PHONY: all build entry-home test check run compile executable clean install uninstall insent debug release help
all: build
build: entry-home $(INTERPRETER) $(COMPILER) $(ENTRY_PACKER)

entry-home:
	mkdir -p "$(ENTRY_HOME)"

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

$(ENTRY_PACKER): src/shentrypak.cpp src/shaye.hpp src/useentry.hpp src/lexer.hpp src/parser.hpp $(FRONTEND_OBJECTS) | $(BUILD_DIR)/.dir
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc src/shentrypak.cpp $(FRONTEND_OBJECTS) $(LDFLAGS) $(LDLIBS) -o "$@"

-include $(DEPFILES)

test: build
	CXX="$(CXX)" sh tests/run.sh ./$(INTERPRETER) ./$(COMPILER) ./$(ENTRY_PACKER)
	sh tests/entry-install.sh ./$(INTERPRETER) ./$(COMPILER) ./$(ENTRY_PACKER)

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
	$(INSTALL) -m 0755 "$(INTERPRETER)" "$(DESTDIR)$(BINDIR)/shaii"
	$(INSTALL) -m 0755 "$(ENTRY_PACKER)" "$(DESTDIR)$(BINDIR)/shentrypak"
	$(INSTALL) -d "$(DESTDIR)$(SHAREDIR)/siil"
	$(INSTALL) -m 0644 srchfile/siil/*.shaii "$(DESTDIR)$(SHAREDIR)/siil/"
	$(INSTALL) -m 0644 srchfile/siil/README.md "$(DESTDIR)$(SHAREDIR)/siil/README.md"

# Usage: make insent /path/to/a/folder/of/shpak/files
# The extra path is accepted as a make goal so the requested positional syntax works.
insent:
	@if [ -z "$(strip $(ENTRY_DIR))" ]; then \
	  printf '%s\n' 'Usage: make insent /path/to/shpak_folder'; exit 2; \
	fi
	@src="$(ENTRY_DIR)"; \
	if [ ! -d "$$src" ]; then printf 'insent: folder not found: %s\n' "$$src" >&2; exit 2; fi; \
	src=$$(CDPATH= cd -- "$$src" && pwd); dest="$(ENTRY_HOME)"; \
	mkdir -p "$$dest"; \
	find "$$src" -type f \( -name '*.shpak' -o -name '*.shaye' \) -exec sh -c 'src=$$1; dest=$$2; shift 2; for file do rel=$${file#"$$src"/}; target="$$dest/$$rel"; mkdir -p "$$(dirname "$$target")" || exit 1; cp -f "$$file" "$$target" || exit 1; done' sh "$$src" "$$dest" {} +; \
	printf 'Installed .shpak entries into %s\n' "$$dest"

# A positional source folder follows the insent goal; this pattern consumes it.
%:
	@:
uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/gshai" "$(DESTDIR)$(BINDIR)/shaii" "$(DESTDIR)$(BINDIR)/shentrypak"
	@for lib in srchfile/siil/*.shaii; do rm -f "$(DESTDIR)$(SHAREDIR)/siil/${lib##*/}"; done
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
	  '  make                 Build gshai, shaii, and shentrypak' \
	  '  shentrypak source.shay -o entry.shpak  # compile an entry package' \
	  '  make test            Run compiler/interpreter regression tests' \
	  '  make check PROGRAM=examples/hello.shay' \
	  '  make run PROGRAM=examples/hello.shay' \
	  '  make compile PROGRAM=examples/hello.shay  # emits .sho' \
	  '  make executable PROGRAM=examples/hello.shay  # emits executable' \
	  '  make install         Install gshai, shaii, and shentrypak' \
	  '  make clean           Remove build outputs'
