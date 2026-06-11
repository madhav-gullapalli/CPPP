CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic

BUILD_DIR := build
COMPILER := $(BUILD_DIR)/cppp.exe
SRC_DIR := src
SOURCES := $(SRC_DIR)/cppp.cpp $(SRC_DIR)/assignmentCppp.cpp $(SRC_DIR)/controlFlow.cpp $(SRC_DIR)/errors.cpp $(SRC_DIR)/expressions.cpp $(SRC_DIR)/printCppp.cpp $(SRC_DIR)/tokenizer.cpp $(SRC_DIR)/typesCppp.cpp
HEADERS := $(SRC_DIR)/assignmentCppp.h $(SRC_DIR)/controlFlow.h $(SRC_DIR)/errors.h $(SRC_DIR)/expressions.h $(SRC_DIR)/printCppp.h $(SRC_DIR)/tokenizer.h $(SRC_DIR)/typesCppp.h

INPUT ?= in.cppp
PROGRAM := $(BUILD_DIR)/$(basename $(notdir $(INPUT))).exe

.PHONY: all transpile compile run clean

all: $(COMPILER)

$(BUILD_DIR):
	if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"

$(COMPILER): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(COMPILER)

transpile: $(COMPILER)
	$(COMPILER) --cppp $(INPUT)

compile: $(COMPILER)
	$(COMPILER) --cppp $(INPUT) --compile

run: $(COMPILER)
	$(COMPILER) --cppp $(INPUT) --run

clean:
	if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
