ISSUE ?= B
O ?= build

SRC_DIR := src
BUILD_DIR := $(O)

CHIRON_DIR := ../..

-include Makefile.user
-include Makefile.config

ifeq ($(ISSUE), B)
main := $(SRC_DIR)/report_flit_b.cpp
endif

ifeq ($(ISSUE), E.b)
main := $(SRC_DIR)/report_flit_eb.cpp
endif

ifeq ($(main),)
$(error "Unknown CHI Issue specified: $(ISSUE)")
endif

build: $(main) FORCE
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	$(CXX) -std=c++20 $(main) -I$(SRC_DIR) -I$(CHIRON_DIR) -o $(BUILD_DIR)/report_flit \
		-D_NODEID_WIDTH=$(NODEID_WIDTH) \
		-D_REQ_ADDR_WIDTH=$(REQ_ADDR_WIDTH) \
		-D_REQ_RSVDC_WIDTH=$(REQ_RSVDC_WIDTH) \
		-D_DAT_RSVDC_WIDTH=$(DAT_RSVDC_WIDTH) \
		-D_DATA_WIDTH=$(DATA_WIDTH) \
		-D_DATACHECK_PRESENT=$(DATACHECK_PRESENT) \
		-D_POISON_PRESENT=$(POISON_PRESENT) \
		-D_MPAM_PRESENT=$(MPAM_PRESENT)

run:
	@$(BUILD_DIR)/./report_flit

clean:
	rm -rf build

FORCE:
.PHONY: FORCE
