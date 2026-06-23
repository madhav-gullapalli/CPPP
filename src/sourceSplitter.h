#pragma once

#include "compileContext.h"

#include <istream>
#include <map>
#include <string>
#include <vector>

size_t findLineCommentStart(const std::string& text);
std::vector<SourceFragment> splitSourceFragments(std::istream& input, std::map<int, std::string>& sourceLines);
