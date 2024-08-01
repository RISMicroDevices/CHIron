O ?= build

SRC_DIR := src
BUILD_DIR := $(O)

CHIRON_DIR := ../..

build: FORCE
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	$(CXX) -std=c++20 $(SRC_DIR)/clog2mkcfg.cpp -I$(SRC_DIR) -I$(CHIRON_DIR) \
		-o $(BUILD_DIR)/clog2mkcfg

run:
	@$(BUILD_DIR)/./clog2mkcfg --version

clean:
	rm -rf build

FORCE:
.PHONY: FORCE
