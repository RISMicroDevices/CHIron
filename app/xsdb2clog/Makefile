O ?= build

SRC_DIR := src
BUILD_DIR := $(O)

CHIRON_DIR := ../..

build: FORCE
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	$(CXX) -std=c++20 $(SRC_DIR)/xsdb2clog.cpp -I$(SRC_DIR) -I$(CHIRON_DIR) \
		-lsqlite3 -o $(BUILD_DIR)/xsdb2clog

run:
	@$(BUILD_DIR)/./xsdb2clog --version

clean:
	rm -rf build

FORCE:
.PHONY: FORCE
