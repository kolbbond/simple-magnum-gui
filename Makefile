# Simple Makefile for simple-magnum-gui

BUILD_DIR ?= build
CMAKE ?= cmake
CMAKE_BUILD_TYPE ?= Release

ifeq ($(OS),Windows_NT)
  CMAKE_GENERATOR ?= Ninja
  PARALLEL ?= --parallel $(if $(NUMBER_OF_PROCESSORS),$(NUMBER_OF_PROCESSORS),4)
else
  CMAKE_GENERATOR ?= $(shell command -v ninja > /dev/null 2>&1 && echo "Ninja" || echo "Unix Makefiles")
  PARALLEL ?= --parallel $(shell nproc 2>/dev/null || echo 4)
endif

CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
CMAKE_EXTRA_FLAGS ?=

.PHONY: all configure build clean install test

all: build

configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -B $(BUILD_DIR) -G "$(CMAKE_GENERATOR)" $(CMAKE_FLAGS) $(CMAKE_EXTRA_FLAGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) $(PARALLEL)

clean:
	rm -rf $(BUILD_DIR)

install: build
	$(CMAKE) --install $(BUILD_DIR)

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure
