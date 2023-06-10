#pragma once

#include <string>
#include <vector>
#include <set>
#include <algorithm>

std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWordsView(std::string_view str);

bool IsValidWord(const std::string_view& word);