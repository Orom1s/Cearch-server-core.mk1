#pragma once

#include <vector>
#include <string>
#include <deque>
#include <list>
#include "document.h"
#include "test_example_functions.h"
#include "search_server.h"


std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);