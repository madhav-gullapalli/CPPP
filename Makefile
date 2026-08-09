CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
PYTHON ?= python3

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
$(SRC_DIR)/compilerDriver.cpp \
$(SRC_DIR)/astParser.cpp \
$(SRC_DIR)/astPrinter.cpp \
$(SRC_DIR)/semanticAnalyzer.cpp \
$(SRC_DIR)/semanticPrinter.cpp \
$(SRC_DIR)/programEmitter.cpp \
$(SRC_DIR)/submitPostProcessor.cpp \
$(SRC_DIR)/functions.cpp \
$(SRC_DIR)/assignmentCppp.cpp \
$(SRC_DIR)/sourceSplitter.cpp \
$(SRC_DIR)/statementCompiler.cpp \
$(SRC_DIR)/statementParser.cpp \
$(SRC_DIR)/controlFlow.cpp \
$(SRC_DIR)/errors.cpp \
$(SRC_DIR)/expressionParser.cpp \
$(SRC_DIR)/expressions.cpp \
$(SRC_DIR)/listsCppp.cpp \
$(SRC_DIR)/printCppp.cpp \
$(SRC_DIR)/tokenizer.cpp \
$(SRC_DIR)/typeDeclarations.cpp \
$(SRC_DIR)/typesCppp.cpp

HEADERS := \
$(SRC_DIR)/assignmentCppp.h \
$(SRC_DIR)/astParser.h \
$(SRC_DIR)/astPrinter.h \
$(SRC_DIR)/semanticAnalyzer.h \
$(SRC_DIR)/semanticAst.h \
$(SRC_DIR)/semanticPrinter.h \
$(SRC_DIR)/compileContext.h \
$(SRC_DIR)/compilerDriver.h \
$(SRC_DIR)/controlFlow.h \
$(SRC_DIR)/errors.h \
$(SRC_DIR)/expressionParser.h \
$(SRC_DIR)/expressions.h \
$(SRC_DIR)/listsCppp.h \
$(SRC_DIR)/printCppp.h \
$(SRC_DIR)/programAst.h \
$(SRC_DIR)/sourceSplitter.h \
$(SRC_DIR)/submitPostProcessor.h \
$(SRC_DIR)/statementParser.h \
$(SRC_DIR)/statementCompiler.h \
$(SRC_DIR)/stmtAst.h \
$(SRC_DIR)/tokenizer.h \
$(SRC_DIR)/typesCppp.h

INPUT ?= in.cppp
PROGRAM := $(dir $(INPUT))$(BUILD_DIR)/$(basename $(notdir $(INPUT)))$(EXE_EXT)

.PHONY: all tokens ast semantic ast-invariants semantic-invariants transpile compile run submit subrun test codegen-freeze codegen-freeze-record clean

all: $(COMPILER)

$(BUILD_DIR):
	$(MKDIR_P)

$(COMPILER): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o "$(COMPILER)"

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
