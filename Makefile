CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic

BUILD_DIR := build
COMPILER := $(BUILD_DIR)/cppp.exe
SOURCES := cppp.cpp errors.cpp printCppp.cpp tokenizer.cpp typesCppp.cpp

INPUT ?= in.cppp
PROGRAM := $(BUILD_DIR)/$(basename $(notdir $(INPUT))).exe

.PHONY: all transpile compile run clean

all: $(COMPILER)

$(BUILD_DIR):
	if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"

$(COMPILER): $(SOURCES) errors.h printCppp.h tokenizer.h typesCppp.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(COMPILER)

transpile: $(COMPILER)
	$(COMPILER) --cppp $(INPUT)

compile: $(COMPILER)
	$(COMPILER) --cppp $(INPUT) --compile

run: compile
	$(PROGRAM)

clean:
	if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
