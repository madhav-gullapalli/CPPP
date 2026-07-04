/*
 * cppp.cpp
 *
 * Provides the command-line entry point for the CP++ compiler.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "compilerDriver.h"

// main is the program entry point for the compiler executable.
int main(int argc, char* argv[]) {
    return runCompilerDriver(argc, argv);
}
