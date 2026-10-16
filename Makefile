BUILD_DIR ?= build
PREFIX ?= $(HOME)/.local
SMG_PREFIX ?= $(HOME)/.local/opt/smg
GENERATOR ?= $(shell command -v ninja > /dev/null 2>&1 && echo Ninja || echo "Unix Makefiles")
ARGS ?=

.PHONY: all build test install clean

all: build

$(BUILD_DIR)/CMakeCache.txt:
	cmake -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_PREFIX_PATH=$(SMG_PREFIX) -DCMAKE_INSTALL_PREFIX=$(PREFIX)

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) --parallel

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure $(ARGS)

install: build
	cmake --install $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
