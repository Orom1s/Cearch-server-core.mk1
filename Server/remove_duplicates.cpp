#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	std::set<std::set<std::string>, int> doc_words;
	std::vector<int> duplicate_ids;

	for (auto document_id : search_server) {
		auto word_freqs = search_server.GetWordFrequencies(document_id);
		std::set<std::string> words;
		std::transform(word_freqs.cbegin(), word_freqs.cend(),
			std::inserter(words, words.begin()), [](const std::pair<std::string, double>& elements) {
				return elements.first;
			});

		auto [word, emplaced] = doc_words.emplace(words);
		if (!emplaced) {
			duplicate_ids.push_back(document_id);
		}
	}

	for (auto document_id : duplicate_ids) {
		std::cout << "Found duplicate document id "s << document_id << std::endl;
		search_server.RemoveDocument(document_id);
	}
}