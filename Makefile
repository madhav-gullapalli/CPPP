CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic

BUILD_DIR := build
SRC_DIR := src

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
	$(SRC_DIR)/assignmentCppp.cpp \
	$(SRC_DIR)/controlFlow.cpp \
	$(SRC_DIR)/errors.cpp \
	$(SRC_DIR)/expressions.cpp \
	$(SRC_DIR)/printCppp.cpp \
	$(SRC_DIR)/tokenizer.cpp \
	$(SRC_DIR)/typesCppp.cpp

HEADERS := \
	$(SRC_DIR)/assignmentCppp.h \
	$(SRC_DIR)/controlFlow.h \
	$(SRC_DIR)/errors.h \
	$(SRC_DIR)/expressions.h \
	$(SRC_DIR)/printCppp.h \
	$(SRC_DIR)/tokenizer.h \
	$(SRC_DIR)/typesCppp.h

INPUT ?= in.cppp
PROGRAM := $(BUILD_DIR)/$(basename $(notdir $(INPUT)))$(EXE_EXT)

.PHONY: all transpile compile run submit subrun clean

all: $(COMPILER)

$(BUILD_DIR):
	$(MKDIR_P)

$(COMPILER): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o "$(COMPILER)"

transpile: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)"

compile: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --compile

run: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --run

submit: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --submit

subrun: $(COMPILER)
	"$(COMPILER)" --cppp "$(INPUT)" --submit
	"$(PROGRAM)"

clean:
	$(RM_RF)