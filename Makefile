# Simple Makefile for simple-magnum-gui

BUILD_DIR ?= build
CMAKE ?= cmake
CMAKE_BUILD_TYPE ?= Release
CMAKE_PRESET ?=
BUILD_CONFIG ?=

ifeq ($(OS),Windows_NT)
  CMAKE_GENERATOR ?= Ninja
  PARALLEL ?= --parallel $(if $(NUMBER_OF_PROCESSORS),$(NUMBER_OF_PROCESSORS),4)
  CMAKE_PRESET ?= windows-vcpkg
else
  CMAKE_GENERATOR ?= $(shell command -v ninja > /dev/null 2>&1 && echo "Ninja" || echo "Unix Makefiles")
  PARALLEL ?= --parallel $(shell nproc 2>/dev/null || echo 4)
endif

CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
CMAKE_EXTRA_FLAGS ?=

.PHONY: all configure build clean install test install-tools uninstall-tools

all: build

configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt:
ifneq ($(strip $(CMAKE_PRESET)),)
	$(CMAKE) --preset $(CMAKE_PRESET)
else
	$(CMAKE) -B $(BUILD_DIR) -G "$(CMAKE_GENERATOR)" $(CMAKE_FLAGS) $(CMAKE_EXTRA_FLAGS)
endif

build: configure
	$(CMAKE) --build $(BUILD_DIR) $(PARALLEL) $(if $(strip $(BUILD_CONFIG)),--config $(BUILD_CONFIG),)

clean:
	rm -rf $(BUILD_DIR)

install: build
	$(CMAKE) --install $(BUILD_DIR)

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure

# example programs as tools: bin/smg-<name> under PREFIX, with private libs in lib/smg-tools
PREFIX ?= $(HOME)/.local
TOOLS ?= fview csvplot files diskusage sysinfo

install-tools: build
	@for t in $(TOOLS); do $(CMAKE) --install $(BUILD_DIR) --component tool-$$t --prefix "$(PREFIX)" || exit 1; done

# removes every smg tool link in PREFIX/bin and the lib/smg-tools they point into
uninstall-tools:
	@dir=""; for l in "$(PREFIX)"/bin/*; do \
		[ -L "$$l" ] || continue; t=$$(readlink "$$l"); \
		case "$$t" in */smg-tools/bin/*) dir=$$(cd "$$(dirname "$$l")/$$(dirname "$$t")/.." && pwd); rm -f "$$l"; echo "removed $$l";; esac; \
	done; \
	if [ -n "$$dir" ] && [ "$$(basename "$$dir")" = smg-tools ]; then rm -rf "$$dir"; echo "removed $$dir"; fi
