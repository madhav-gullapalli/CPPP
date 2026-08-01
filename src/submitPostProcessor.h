/*
 * submitPostProcessor.h
 *
 * Final post-processing for contest-oriented generated C++.
 */

#pragma once

#include <string>

// Removes comments and nonessential C++ whitespace after emission is complete.
std::string compactSubmitCpp(const std::string& readableCpp);
