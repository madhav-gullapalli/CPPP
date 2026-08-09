CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
PYTHON ?= python3

BUILD_DIR := build
SRC_DIR := src
INCLUDE_DIRS := \
    -I$(SRC_DIR) \
    -I$(SRC_DIR)/tokenize \
    -I$(SRC_DIR)/parse \
    -I$(SRC_DIR)/semantic_analyze \
    -I$(SRC_DIR)/codegen

ifeq ($(OS),Windows_NT)
EXE_EXT := .exe
MKDIR_P := if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
RM_RF := if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
else
EXE_EXT :=
MKDIR_P := mkdir -p "$(BUILD_DIR)"
RM_RF := rm -rf "$(BUILD_DIR)"
endif

COMPILER := $(BUILD_DIR)/cppp$(EXE_EXT)

SOURCES := \
$(SRC_DIR)/cppp.cpp \
$(SRC_DIR)/compilerDriver.cpp \
$(wildcard $(SRC_DIR)/tokenize/*.cpp) \
$(wildcard $(SRC_DIR)/parse/*.cpp) \
$(wildcard $(SRC_DIR)/semantic_analyze/*.cpp) \
$(wildcard $(SRC_DIR)/codegen/*.cpp)

HEADERS := \
$(SRC_DIR)/compilerDriver.h \
$(wildcard $(SRC_DIR)/tokenize/*.h) \
$(wildcard $(SRC_DIR)/parse/*.h) \
$(wildcard $(SRC_DIR)/semantic_analyze/*.h) \
$(wildcard $(SRC_DIR)/codegen/*.h)

INPUT ?= in.cppp
PROGRAM := $(dir $(INPUT))$(BUILD_DIR)/$(basename $(notdir $(INPUT)))$(EXE_EXT)

.PHONY: all tokens ast semantic ast-invariants semantic-invariants transpile compile run submit subrun test codegen-freeze codegen-freeze-record clean

all: $(COMPILER)

$(BUILD_DIR):
	$(MKDIR_P)

$(COMPILER): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(SOURCES) -o "$(COMPILER)"

transpile: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)"

tokens: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --tokens

ast: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --ast

semantic: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --semantic

ast-invariants: $(COMPILER)
	$(PYTHON) tests/ast_invariants.py

semantic-invariants: $(COMPILER)
	$(PYTHON) tests/semantic_invariants.py

compile: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --compile

run: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --run

submit: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --submit $(if $(filter 1 true yes,$(READABLE)),--readable,)

subrun: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --submit $(if $(filter 1 true yes,$(READABLE)),--readable,)
	"$(PROGRAM)"

test: $(COMPILER)
	bash tests/regression.sh

codegen-freeze: $(COMPILER)
	$(PYTHON) tests/codegen_freeze.py check --skip-build

codegen-freeze-record: $(COMPILER)
	$(PYTHON) tests/codegen_freeze.py record --skip-build

clean:
	$(RM_RF)
