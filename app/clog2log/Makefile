ISSUE ?= B
O ?= build

SRC_DIR := src
BUILD_DIR := $(O)

CHIRON_DIR := ../..

-include Makefile.user
-include Makefile.config

ifeq ($(ISSUE), B)
main := $(SRC_DIR)/clog2log_b.cpp
endif

ifeq ($(ISSUE), E.b)
main := $(SRC_DIR)/clog2log_eb.cpp
endif

ifeq ($(main),)
$(error "Unknown CHI Issue specified: $(ISSUE)")
endif

build: $(main) FORCE
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	$(CXX) -std=c++20 -O3 $(main) -I$(SRC_DIR) -I$(CHIRON_DIR) -lz -o $(BUILD_DIR)/clog2log \
		-DCHI_NODEID_WIDTH=$(NODEID_WIDTH) \
		-DCHI_REQ_ADDR_WIDTH=$(REQ_ADDR_WIDTH) \
		-DCHI_REQ_RSVDC_WIDTH=$(REQ_RSVDC_WIDTH) \
		-DCHI_DAT_RSVDC_WIDTH=$(DAT_RSVDC_WIDTH) \
		-DCHI_DATA_WIDTH=$(DATA_WIDTH) \
		-DCHI_DATACHECK_PRESENT=$(DATACHECK_PRESENT) \
		-DCHI_POISON_PRESENT=$(POISON_PRESENT) \
		-DCHI_MPAM_PRESENT=$(MPAM_PRESENT)

run:
	@$(BUILD_DIR)/./clog2log

clean:
	rm -rf build

FORCE:
.PHONY: FORCE
