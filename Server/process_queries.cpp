#include "process_queries.h"


#include <execution>
#include <algorithm>



std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> processed(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), processed.begin(), [&search_server](auto t) {return search_server.FindTopDocuments(t); });
    return processed;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<Document> result;

    for (const auto& document : ProcessQueries(search_server, queries)) {
        result.insert(result.end(), document.begin(), document.end());
    }
    return result;
}